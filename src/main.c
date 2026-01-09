#include <emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text.h"
#include "math.h"
#include "game.h"

// Global state
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

// Shader source buffers (loaded from files)
static char* sprite_shader_source = NULL;
static char* text_shader_source = NULL;

// Load a file from the virtual filesystem (preloaded by Emscripten)
static char* load_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Failed to open file: %s\n", path);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        printf("Failed to allocate memory for file: %s\n", path);
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';
    fclose(f);
    
    printf("Loaded file: %s (%ld bytes)\n", path, size);
    return buffer;
}

// Load all shader files
static int load_shaders(void) {
    sprite_shader_source = load_file("data/shaders/sprite.wgsl");
    if (!sprite_shader_source) return 1;
    
    text_shader_source = load_file("data/shaders/text.wgsl");
    if (!text_shader_source) return 2;
    
    return 0;
}

// Render frame
void render_frame(void) {
    if (!device) return;
    
    // Get current time and calculate delta
    double current_time = emscripten_get_now() / 1000.0;
    float dt = (float)(current_time - last_time);
    if (dt > 0.1f) dt = 0.1f;  // Cap delta time
    last_time = current_time;
    
    // Update game state
    game_update(dt, canvas_width, canvas_height);
    
    // Get sprite for rendering
    const Sprite* sprite = game_get_sprite();
    
    // Update uniforms
    Uniforms uniforms;
    
    // Build transformation matrix: projection * translation * rotation * scale
    // Using perspective projection for 3D rendering
    float proj[16], trans[16], rot[16], scale[16];
    float temp1[16], temp2[16];
    
    // Perspective projection: camera_dist controls FOV strength
    // Objects at z=0 appear at same size as orthographic
    // Objects with z<0 appear smaller (farther from camera)
    // Use world units (canvas pixels / PIXELS_PER_UNIT)
    const float camera_dist = 500.0f / PIXELS_PER_UNIT;  // ~31.25 world units
    const float far_plane = 1000.0f / PIXELS_PER_UNIT;   // ~62.5 world units
    float world_width = (float)canvas_width / PIXELS_PER_UNIT;
    float world_height = (float)canvas_height / PIXELS_PER_UNIT;
    mat4_perspective(proj, world_width, world_height, camera_dist, far_plane);
    mat4_translate_3d(trans, sprite->x, sprite->y, sprite->z);
    mat4_rotate_z(rot, -sprite->angle);  // Negative because we rotate counter-clockwise
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
    
    // Render game objects (text, etc.)
    RenderContext render_ctx = {
        .pass = pass,
        .canvas_width = canvas_width,
        .canvas_height = canvas_height,
    };
    game_render(&render_ctx);
    
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
    
    // Update text rendering canvas size
    text_set_canvas_size(canvas_width, canvas_height);
    
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
    
    // Load shaders from preloaded files
    if (load_shaders()) {
        printf("Failed to load shaders!\n");
        return;
    }
    
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
        .code = {.data = sprite_shader_source, .length = strlen(sprite_shader_source)},
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
    
    // Initialize time
    last_time = emscripten_get_now() / 1000.0;
    
    printf("WebGPU initialization complete\n");
    
    // Initialize text rendering system
    text_init(device, queue, surface_format);
    text_set_canvas_size(canvas_width, canvas_height);
    text_load_font_file("data/fonts/mikado-medium-f00f2383.fnt");
    text_create_pipeline(text_shader_source);
    
    // Initialize game state
    game_init(canvas_width, canvas_height);
    
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
