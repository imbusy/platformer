#include <emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPRITE_SIZE 64.0f
#define MOVE_SPEED 200.0f
#define ROTATE_SPEED 3.0f
#define PI 3.14159265358979323846f

// Sprite state
typedef struct {
    float x;
    float y;
    float angle;  // in radians
    float speed;
} Sprite;

// Input state
typedef struct {
    int up;
    int down;
    int left;
    int right;
} InputState;

// Global state
static Sprite sprite;
static InputState input;
static double last_time = 0.0;

// Canvas dimensions (updated dynamically)
static int canvas_width = 800;
static int canvas_height = 600;

// WebGPU objects
static WGPUDevice device = NULL;
static WGPUQueue queue = NULL;
static WGPUSurface surface = NULL;
static WGPURenderPipeline pipeline = NULL;
static WGPUBuffer vertex_buffer = NULL;
static WGPUBuffer uniform_buffer = NULL;
static WGPUBindGroup bind_group = NULL;
static WGPUTextureFormat surface_format = WGPUTextureFormat_BGRA8Unorm;

// Vertex data (position + uv)
typedef struct {
    float position[2];
    float uv[2];
} Vertex;

// Uniform data
typedef struct {
    float transform[16];  // 4x4 matrix
    float color[4];       // RGBA color
} Uniforms;

// Shader source (WGSL)
static const char* shader_source = 
    "struct Uniforms {\n"
    "    transform: mat4x4<f32>,\n"
    "    color: vec4<f32>,\n"
    "};\n"
    "\n"
    "@group(0) @binding(0) var<uniform> uniforms: Uniforms;\n"
    "\n"
    "struct VertexInput {\n"
    "    @location(0) position: vec2<f32>,\n"
    "    @location(1) uv: vec2<f32>,\n"
    "};\n"
    "\n"
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "    @location(0) uv: vec2<f32>,\n"
    "};\n"
    "\n"
    "@vertex\n"
    "fn vs_main(in: VertexInput) -> VertexOutput {\n"
    "    var out: VertexOutput;\n"
    "    out.position = uniforms.transform * vec4<f32>(in.position, 0.0, 1.0);\n"
    "    out.uv = in.uv;\n"
    "    return out;\n"
    "}\n"
    "\n"
    "@fragment\n"
    "fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {\n"
    "    // Create a simple arrow/triangle sprite pattern\n"
    "    let uv = in.uv - vec2<f32>(0.5, 0.5);\n"
    "    \n"
    "    // Arrow body (rectangle)\n"
    "    let body = abs(uv.x) < 0.15 && uv.y > -0.3 && uv.y < 0.2;\n"
    "    \n"
    "    // Arrow head (triangle pointing up)\n"
    "    let head_y = uv.y - 0.2;\n"
    "    let head = head_y > 0.0 && head_y < 0.3 && abs(uv.x) < (0.3 - head_y);\n"
    "    \n"
    "    if (body || head) {\n"
    "        return uniforms.color;\n"
    "    }\n"
    "    \n"
    "    // Transparent background - return fully transparent\n"
    "    return vec4<f32>(0.0, 0.0, 0.0, 0.0);\n"
    "}\n";

// Matrix helper functions
void mat4_identity(float* m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void mat4_ortho(float* m, float left, float right, float bottom, float top) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = -1.0f;
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[15] = 1.0f;
}

void mat4_translate(float* m, float x, float y) {
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
}

void mat4_rotate_z(float* m, float angle) {
    mat4_identity(m);
    float c = cosf(angle);
    float s = sinf(angle);
    m[0] = c;
    m[1] = s;
    m[4] = -s;
    m[5] = c;
}

void mat4_scale(float* m, float sx, float sy) {
    mat4_identity(m);
    m[0] = sx;
    m[5] = sy;
}

void mat4_multiply(float* result, const float* a, const float* b) {
    float temp[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                temp[i * 4 + j] += a[k * 4 + j] * b[i * 4 + k];
            }
        }
    }
    memcpy(result, temp, 16 * sizeof(float));
}

// Input handlers (called from JavaScript)
EMSCRIPTEN_KEEPALIVE
void on_key_down(int key_code) {
    switch (key_code) {
        case 38: input.up = 1; break;    // Up arrow
        case 40: input.down = 1; break;  // Down arrow
        case 37: input.left = 1; break;  // Left arrow
        case 39: input.right = 1; break; // Right arrow
    }
}

EMSCRIPTEN_KEEPALIVE
void on_key_up(int key_code) {
    switch (key_code) {
        case 38: input.up = 0; break;
        case 40: input.down = 0; break;
        case 37: input.left = 0; break;
        case 39: input.right = 0; break;
    }
}

// Update sprite based on input
void update_sprite(float dt) {
    // Rotate left/right
    if (input.left) {
        sprite.angle -= ROTATE_SPEED * dt;
    }
    if (input.right) {
        sprite.angle += ROTATE_SPEED * dt;
    }
    
    // Keep angle in [0, 2*PI]
    while (sprite.angle < 0) sprite.angle += 2 * PI;
    while (sprite.angle >= 2 * PI) sprite.angle -= 2 * PI;
    
    // Move forward/backward based on angle
    float move = 0.0f;
    if (input.up) move += MOVE_SPEED * dt;
    if (input.down) move -= MOVE_SPEED * dt;
    
    if (move != 0.0f) {
        // Move in the direction the sprite is facing (angle 0 = up)
        sprite.x += sinf(sprite.angle) * move;
        sprite.y += cosf(sprite.angle) * move;
    }
    
    // Keep sprite on screen with wrapping
    if (sprite.x < -SPRITE_SIZE) sprite.x = canvas_width + SPRITE_SIZE;
    if (sprite.x > canvas_width + SPRITE_SIZE) sprite.x = -SPRITE_SIZE;
    if (sprite.y < -SPRITE_SIZE) sprite.y = canvas_height + SPRITE_SIZE;
    if (sprite.y > canvas_height + SPRITE_SIZE) sprite.y = -SPRITE_SIZE;
}

// Render frame
void render_frame(void) {
    if (!device) return;
    
    // Get current time and calculate delta
    double current_time = emscripten_get_now() / 1000.0;
    float dt = (float)(current_time - last_time);
    if (dt > 0.1f) dt = 0.1f;  // Cap delta time
    last_time = current_time;
    
    // Update sprite
    update_sprite(dt);
    
    // Update uniforms
    Uniforms uniforms;
    
    // Build transformation matrix: projection * translation * rotation * scale
    float proj[16], trans[16], rot[16], scale[16];
    float temp1[16], temp2[16];
    
    mat4_ortho(proj, 0, (float)canvas_width, 0, (float)canvas_height);
    mat4_translate(trans, sprite.x, sprite.y);
    mat4_rotate_z(rot, -sprite.angle);  // Negative because we rotate counter-clockwise
    mat4_scale(scale, SPRITE_SIZE, SPRITE_SIZE);
    
    mat4_multiply(temp1, rot, scale);
    mat4_multiply(temp2, trans, temp1);
    mat4_multiply(uniforms.transform, proj, temp2);
    
    // Sprite color (bright green)
    uniforms.color[0] = 0.2f;
    uniforms.color[1] = 0.8f;
    uniforms.color[2] = 0.3f;
    uniforms.color[3] = 1.0f;
    
    wgpuQueueWriteBuffer(queue, uniform_buffer, 0, &uniforms, sizeof(Uniforms));
    
    // Get current texture view
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(surface, &surface_texture);
    
    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        return;
    }
    
    WGPUTextureViewDescriptor view_desc = {
        .format = surface_format,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    };
    WGPUTextureView view = wgpuTextureCreateView(surface_texture.texture, &view_desc);
    
    // Create command encoder
    WGPUCommandEncoderDescriptor enc_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &enc_desc);
    
    // Begin render pass
    WGPURenderPassColorAttachment color_attachment = {
        .view = view,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,  // Required for non-3D textures
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = {0.1f, 0.1f, 0.15f, 1.0f},  // Dark blue-gray background
    };
    
    WGPURenderPassDescriptor pass_desc = {
        .colorAttachmentCount = 1,
        .colorAttachments = &color_attachment,
    };
    
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);
    
    // Draw sprite
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bind_group, 0, NULL);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertex_buffer, 0, 6 * sizeof(Vertex));
    wgpuRenderPassEncoderDraw(pass, 6, 1, 0, 0);
    
    wgpuRenderPassEncoderEnd(pass);
    
    // Submit commands
    WGPUCommandBufferDescriptor cmd_desc = {};
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, &cmd_desc);
    wgpuQueueSubmit(queue, 1, &commands);
    
    // Note: wgpuSurfacePresent is not needed with Emscripten - presentation is automatic
    
    // Cleanup
    wgpuCommandBufferRelease(commands);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(surface_texture.texture);
}

// Get current canvas size
void get_canvas_size(int* width, int* height) {
    double w, h;
    emscripten_get_element_css_size("#canvas", &w, &h);
    *width = (int)w;
    *height = (int)h;
}

// Configure/reconfigure the WebGPU surface
void configure_surface(void) {
    if (!device || !surface) return;
    
    get_canvas_size(&canvas_width, &canvas_height);
    
    // Ensure minimum size
    if (canvas_width < 1) canvas_width = 1;
    if (canvas_height < 1) canvas_height = 1;
    
    WGPUSurfaceConfiguration config = {
        .device = device,
        .format = surface_format,
        .usage = WGPUTextureUsage_RenderAttachment,
        .alphaMode = WGPUCompositeAlphaMode_Opaque,
        .width = (uint32_t)canvas_width,
        .height = (uint32_t)canvas_height,
        .presentMode = WGPUPresentMode_Fifo,
    };
    wgpuSurfaceConfigure(surface, &config);
    
    printf("Surface configured: %dx%d\n", canvas_width, canvas_height);
}

// Resize callback
EM_BOOL on_canvas_resize(int event_type, const EmscriptenUiEvent* ui_event, void* user_data) {
    (void)event_type;
    (void)ui_event;
    (void)user_data;
    configure_surface();
    return EM_TRUE;
}

// Initialize WebGPU
void init_webgpu(WGPUDevice dev) {
    device = dev;
    queue = wgpuDeviceGetQueue(device);
    
    printf("WebGPU device initialized\n");
    
    // Get initial canvas size
    get_canvas_size(&canvas_width, &canvas_height);
    
    // Create surface from canvas
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_source = {
        .chain = {.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector},
        .selector = "#canvas",
    };
    WGPUSurfaceDescriptor surface_desc = {
        .nextInChain = (WGPUChainedStruct*)&canvas_source,
    };
    
    WGPUInstance instance = wgpuCreateInstance(NULL);
    surface = wgpuInstanceCreateSurface(instance, &surface_desc);
    
    // Configure surface with actual canvas size
    configure_surface();
    
    // Register resize callback
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_FALSE, on_canvas_resize);
    
    // Create shader module
    WGPUShaderSourceWGSL wgsl_source = {
        .chain = {.sType = WGPUSType_ShaderSourceWGSL},
        .code = {.data = shader_source, .length = strlen(shader_source)},
    };
    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = (WGPUChainedStruct*)&wgsl_source,
    };
    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(device, &shader_desc);
    
    // Create vertex buffer
    Vertex vertices[] = {
        {{-0.5f, -0.5f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f}, {0.0f, 1.0f}},
    };
    
    WGPUBufferDescriptor vb_desc = {
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = sizeof(vertices),
        .mappedAtCreation = true,
    };
    vertex_buffer = wgpuDeviceCreateBuffer(device, &vb_desc);
    memcpy(wgpuBufferGetMappedRange(vertex_buffer, 0, sizeof(vertices)), vertices, sizeof(vertices));
    wgpuBufferUnmap(vertex_buffer);
    
    // Create uniform buffer
    WGPUBufferDescriptor ub_desc = {
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = sizeof(Uniforms),
    };
    uniform_buffer = wgpuDeviceCreateBuffer(device, &ub_desc);
    
    // Create bind group layout
    WGPUBindGroupLayoutEntry bgl_entry = {
        .binding = 0,
        .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
        .buffer = {
            .type = WGPUBufferBindingType_Uniform,
            .minBindingSize = sizeof(Uniforms),
        },
    };
    WGPUBindGroupLayoutDescriptor bgl_desc = {
        .entryCount = 1,
        .entries = &bgl_entry,
    };
    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(device, &bgl_desc);
    
    // Create bind group
    WGPUBindGroupEntry bg_entry = {
        .binding = 0,
        .buffer = uniform_buffer,
        .offset = 0,
        .size = sizeof(Uniforms),
    };
    WGPUBindGroupDescriptor bg_desc = {
        .layout = bind_group_layout,
        .entryCount = 1,
        .entries = &bg_entry,
    };
    bind_group = wgpuDeviceCreateBindGroup(device, &bg_desc);
    
    // Create pipeline layout
    WGPUPipelineLayoutDescriptor pl_desc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bind_group_layout,
    };
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(device, &pl_desc);
    
    // Create render pipeline
    WGPUVertexAttribute attrs[] = {
        {.format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 0},
        {.format = WGPUVertexFormat_Float32x2, .offset = 8, .shaderLocation = 1},
    };
    WGPUVertexBufferLayout vb_layout = {
        .arrayStride = sizeof(Vertex),
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 2,
        .attributes = attrs,
    };
    
    WGPUBlendState blend_state = {
        .color = {
            .srcFactor = WGPUBlendFactor_SrcAlpha,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
            .operation = WGPUBlendOperation_Add,
        },
        .alpha = {
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
            .operation = WGPUBlendOperation_Add,
        },
    };
    
    WGPUColorTargetState color_target = {
        .format = surface_format,
        .blend = &blend_state,
        .writeMask = WGPUColorWriteMask_All,
    };
    
    WGPUFragmentState fragment = {
        .module = shader,
        .entryPoint = {.data = "fs_main", .length = 7},
        .targetCount = 1,
        .targets = &color_target,
    };
    
    WGPURenderPipelineDescriptor rp_desc = {
        .layout = pipeline_layout,
        .vertex = {
            .module = shader,
            .entryPoint = {.data = "vs_main", .length = 7},
            .bufferCount = 1,
            .buffers = &vb_layout,
        },
        .fragment = &fragment,
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = WGPUCullMode_None,
        },
        .multisample = {
            .count = 1,
            .mask = 0xFFFFFFFF,
        },
    };
    pipeline = wgpuDeviceCreateRenderPipeline(device, &rp_desc);
    
    // Cleanup
    wgpuShaderModuleRelease(shader);
    wgpuBindGroupLayoutRelease(bind_group_layout);
    wgpuPipelineLayoutRelease(pipeline_layout);
    
    // Initialize sprite at center of canvas
    sprite.x = canvas_width / 2.0f;
    sprite.y = canvas_height / 2.0f;
    sprite.angle = 0.0f;
    
    // Initialize input
    memset(&input, 0, sizeof(input));
    
    // Initialize time
    last_time = emscripten_get_now() / 1000.0;
    
    printf("WebGPU initialization complete\n");
    
    // Start render loop
    emscripten_set_main_loop(render_frame, 0, 0);
}

void request_device_callback(WGPURequestDeviceStatus status, WGPUDevice dev, WGPUStringView msg, void* userdata1, void* userdata2) {
    (void)userdata1;
    (void)userdata2;
    if (status == WGPURequestDeviceStatus_Success) {
        init_webgpu(dev);
    } else {
        printf("Failed to get WebGPU device: %.*s\n", (int)msg.length, msg.data);
    }
}

void request_adapter_callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView msg, void* userdata1, void* userdata2) {
    (void)userdata1;
    (void)userdata2;
    if (status == WGPURequestAdapterStatus_Success) {
        WGPUDeviceDescriptor dev_desc = {};
        WGPURequestDeviceCallbackInfo callback_info = {
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = request_device_callback,
            .userdata1 = NULL,
            .userdata2 = NULL,
        };
        wgpuAdapterRequestDevice(adapter, &dev_desc, callback_info);
    } else {
        printf("Failed to get WebGPU adapter: %.*s\n", (int)msg.length, msg.data);
    }
}

int main() {
    printf("Starting WebGPU Sprite Demo\n");
    
    // Request adapter
    WGPUInstance instance = wgpuCreateInstance(NULL);
    WGPURequestAdapterOptions options = {};
    WGPURequestAdapterCallbackInfo callback_info = {
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = request_adapter_callback,
        .userdata1 = NULL,
        .userdata2 = NULL,
    };
    wgpuInstanceRequestAdapter(instance, &options, callback_info);
    
    return 0;
}
