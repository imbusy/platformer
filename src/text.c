#include "text.h"
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Vertex data (position + uv)
typedef struct {
    float position[2];
    float uv[2];
} TextVertex;

// Uniform data
typedef struct {
    float transform[16];  // 4x4 matrix
    float color[4];       // RGBA color
} TextUniforms;

// Text rendering WebGPU objects
static WGPUDevice text_device = NULL;
static WGPUQueue text_queue = NULL;
static WGPURenderPipeline text_pipeline = NULL;
static WGPUBuffer text_vertex_buffer = NULL;
static WGPUBuffer text_uniform_buffer = NULL;
static WGPUBindGroup text_bind_group = NULL;
static WGPUTexture font_texture = NULL;
static WGPUTextureView font_texture_view = NULL;
static WGPUSampler font_sampler = NULL;
static WGPUBindGroupLayout text_bind_group_layout = NULL;
static WGPUTextureFormat text_surface_format = WGPUTextureFormat_BGRA8Unorm;

// Font data
static FontData font_data = {0};
static int text_vertex_count = 0;
static int font_texture_loaded = 0;
static int font_data_loaded = 0;

// Shader source (set via text_create_pipeline)
static const char* text_shader_source = NULL;

// Canvas dimensions
static int text_canvas_width = 800;
static int text_canvas_height = 600;

// Matrix helper functions (local copies)
static void mat4_ortho(float* m, float left, float right, float bottom, float top) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = -1.0f;
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[15] = 1.0f;
}

// Initialize text rendering system
void text_init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat format) {
    text_device = device;
    text_queue = queue;
    text_surface_format = format;
}

// Update canvas dimensions
void text_set_canvas_size(int width, int height) {
    text_canvas_width = width;
    text_canvas_height = height;
}

// Parse a single line from .fnt file for char data
static void parse_char_line(const char* line) {
    int id = 0;
    float x = 0, y = 0, width = 0, height = 0;
    float xoffset = 0, yoffset = 0, xadvance = 0;
    
    // Parse the line using sscanf - format: char id=X x=X y=X width=X height=X xoffset=X yoffset=X xadvance=X ...
    const char* ptr = line;
    while (*ptr) {
        if (strncmp(ptr, "id=", 3) == 0) {
            id = atoi(ptr + 3);
        } else if (strncmp(ptr, "x=", 2) == 0 && *(ptr-1) != 'o') {
            x = atof(ptr + 2);
        } else if (strncmp(ptr, "y=", 2) == 0) {
            y = atof(ptr + 2);
        } else if (strncmp(ptr, "width=", 6) == 0) {
            width = atof(ptr + 6);
        } else if (strncmp(ptr, "height=", 7) == 0) {
            height = atof(ptr + 7);
        } else if (strncmp(ptr, "xoffset=", 8) == 0) {
            xoffset = atof(ptr + 8);
        } else if (strncmp(ptr, "yoffset=", 8) == 0) {
            yoffset = atof(ptr + 8);
        } else if (strncmp(ptr, "xadvance=", 9) == 0) {
            xadvance = atof(ptr + 9);
        }
        ptr++;
    }
    
    if (id > 0 && id < MAX_GLYPHS) {
        font_data.glyphs[id].id = id;
        font_data.glyphs[id].x = x;
        font_data.glyphs[id].y = y;
        font_data.glyphs[id].width = width;
        font_data.glyphs[id].height = height;
        font_data.glyphs[id].xoffset = xoffset;
        font_data.glyphs[id].yoffset = yoffset;
        font_data.glyphs[id].xadvance = xadvance;
    }
}

// Parse common line from .fnt file
static void parse_common_line(const char* line) {
    const char* ptr = line;
    while (*ptr) {
        if (strncmp(ptr, "lineHeight=", 11) == 0) {
            font_data.line_height = atof(ptr + 11);
        } else if (strncmp(ptr, "base=", 5) == 0) {
            font_data.base = atof(ptr + 5);
        } else if (strncmp(ptr, "scaleW=", 7) == 0) {
            font_data.scale_w = atof(ptr + 7);
        } else if (strncmp(ptr, "scaleH=", 7) == 0) {
            font_data.scale_h = atof(ptr + 7);
        }
        ptr++;
    }
}

// Parse the entire .fnt file
void text_parse_fnt_data(const char* data) {
    // Initialize font data
    memset(&font_data, 0, sizeof(font_data));
    
    char line[512];
    const char* ptr = data;
    
    while (*ptr) {
        // Read a line
        int i = 0;
        while (*ptr && *ptr != '\n' && *ptr != '\r' && i < 511) {
            line[i++] = *ptr++;
        }
        line[i] = '\0';
        
        // Skip newlines
        while (*ptr == '\n' || *ptr == '\r') ptr++;
        
        // Parse the line
        if (strncmp(line, "common ", 7) == 0) {
            parse_common_line(line);
        } else if (strncmp(line, "char ", 5) == 0) {
            parse_char_line(line);
        }
    }
    
    font_data.loaded = 1;
    font_data_loaded = 1;
    printf("Font data parsed: lineHeight=%.1f, base=%.1f, texture=%dx%d\n",
           font_data.line_height, font_data.base, 
           (int)font_data.scale_w, (int)font_data.scale_h);
}

// Build text vertices for a string
// Returns the number of vertices generated
static int build_text_vertices(const char* text, float x, float y, float scale, TextVertex* vertices) {
    if (!font_data.loaded) return 0;
    
    int vertex_count = 0;
    float cursor_x = x;
    float cursor_y = y;
    
    for (const char* c = text; *c; c++) {
        int ch = (unsigned char)*c;
        
        if (ch >= MAX_GLYPHS) continue;
        
        Glyph* g = &font_data.glyphs[ch];
        if (g->width == 0 || g->height == 0) {
            // Space or unknown character - just advance
            cursor_x += g->xadvance * scale;
            continue;
        }
        
        // Calculate vertex positions (screen space)
        float x0 = cursor_x + g->xoffset * scale;
        float y0 = cursor_y - g->yoffset * scale;  // Flip Y for top-left origin
        float x1 = x0 + g->width * scale;
        float y1 = y0 - g->height * scale;
        
        // Calculate UV coordinates (normalized 0-1)
        float u0 = g->x / font_data.scale_w;
        float v0 = g->y / font_data.scale_h;
        float u1 = (g->x + g->width) / font_data.scale_w;
        float v1 = (g->y + g->height) / font_data.scale_h;
        
        // Two triangles per glyph (6 vertices)
        // Triangle 1: top-left, top-right, bottom-right
        vertices[vertex_count++] = (TextVertex){{x0, y0}, {u0, v0}};
        vertices[vertex_count++] = (TextVertex){{x1, y0}, {u1, v0}};
        vertices[vertex_count++] = (TextVertex){{x1, y1}, {u1, v1}};
        
        // Triangle 2: top-left, bottom-right, bottom-left
        vertices[vertex_count++] = (TextVertex){{x0, y0}, {u0, v0}};
        vertices[vertex_count++] = (TextVertex){{x1, y1}, {u1, v1}};
        vertices[vertex_count++] = (TextVertex){{x0, y1}, {u0, v1}};
        
        // Advance cursor
        cursor_x += g->xadvance * scale;
        
        // Safety check
        if (vertex_count >= MAX_TEXT_VERTICES - 6) break;
    }
    
    return vertex_count;
}

// Calculate text width for centering
float calculate_text_width(const char* text, float scale) {
    if (!font_data.loaded) return 0;
    
    float width = 0;
    for (const char* c = text; *c; c++) {
        int ch = (unsigned char)*c;
        if (ch < MAX_GLYPHS) {
            width += font_data.glyphs[ch].xadvance * scale;
        }
    }
    return width;
}

// Forward declaration
static void create_text_pipeline_internal(void);

// Called from JavaScript when the font texture image is loaded
EMSCRIPTEN_KEEPALIVE
void upload_font_texture(unsigned char* data, int width, int height) {
    printf("Uploading font texture: %dx%d\n", width, height);
    
    // Create texture
    WGPUTextureDescriptor tex_desc = {
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = {width, height, 1},
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    font_texture = wgpuDeviceCreateTexture(text_device, &tex_desc);
    
    // Upload data
    WGPUTexelCopyBufferLayout data_layout = {
        .offset = 0,
        .bytesPerRow = 4 * width,
        .rowsPerImage = height,
    };
    WGPUExtent3D write_size = {width, height, 1};
    WGPUTexelCopyTextureInfo dest = {
        .texture = font_texture,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = WGPUTextureAspect_All,
    };
    wgpuQueueWriteTexture(text_queue, &dest, data, width * height * 4, &data_layout, &write_size);
    
    // Create texture view
    WGPUTextureViewDescriptor view_desc = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    };
    font_texture_view = wgpuTextureCreateView(font_texture, &view_desc);
    
    // Create sampler
    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_ClampToEdge,
        .addressModeW = WGPUAddressMode_ClampToEdge,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1.0f,
        .maxAnisotropy = 1,
    };
    font_sampler = wgpuDeviceCreateSampler(text_device, &sampler_desc);
    
    font_texture_loaded = 1;
    printf("Font texture created and uploaded\n");
    
    create_text_pipeline_internal();
}

// Called from JavaScript when font data file is loaded
EMSCRIPTEN_KEEPALIVE
void load_font_data(const char* data) {
    printf("Loading font data...\n");
    text_parse_fnt_data(data);
}

// Load font data from a .fnt file path
void text_load_font_file(const char* fnt_path) {
    FILE* f = fopen(fnt_path, "rb");
    if (!f) {
        printf("Failed to open font file: %s\n", fnt_path);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        printf("Failed to allocate memory for font file: %s\n", fnt_path);
        fclose(f);
        return;
    }
    
    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';
    fclose(f);
    
    printf("Loaded font file: %s (%ld bytes)\n", fnt_path, size);
    text_parse_fnt_data(buffer);
    free(buffer);
}

// Set shader source and try to create pipeline
void text_create_pipeline(const char* shader_source) {
    text_shader_source = shader_source;
    create_text_pipeline_internal();
}

// Create the text rendering pipeline - called after both texture and data are ready
static void create_text_pipeline_internal(void) {
    if (!font_texture_loaded || !font_data_loaded || !text_device || !text_shader_source) return;
    if (text_pipeline) return;  // Already created
    
    printf("Creating text rendering pipeline...\n");
    
    // Create shader module
    WGPUShaderSourceWGSL wgsl_source = {
        .chain = {.sType = WGPUSType_ShaderSourceWGSL},
        .code = {.data = text_shader_source, .length = strlen(text_shader_source)},
    };
    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = (WGPUChainedStruct*)&wgsl_source,
    };
    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(text_device, &shader_desc);
    
    // Create text vertex buffer
    WGPUBufferDescriptor vb_desc = {
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = MAX_TEXT_VERTICES * sizeof(TextVertex),
    };
    text_vertex_buffer = wgpuDeviceCreateBuffer(text_device, &vb_desc);
    
    // Create text uniform buffer
    WGPUBufferDescriptor ub_desc = {
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .size = sizeof(TextUniforms),
    };
    text_uniform_buffer = wgpuDeviceCreateBuffer(text_device, &ub_desc);
    
    // Create bind group layout for text (uniform + texture + sampler)
    WGPUBindGroupLayoutEntry bgl_entries[] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
            .buffer = {
                .type = WGPUBufferBindingType_Uniform,
                .minBindingSize = sizeof(TextUniforms),
            },
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .texture = {
                .sampleType = WGPUTextureSampleType_Float,
                .viewDimension = WGPUTextureViewDimension_2D,
                .multisampled = false,
            },
        },
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .sampler = {
                .type = WGPUSamplerBindingType_Filtering,
            },
        },
    };
    WGPUBindGroupLayoutDescriptor bgl_desc = {
        .entryCount = 3,
        .entries = bgl_entries,
    };
    text_bind_group_layout = wgpuDeviceCreateBindGroupLayout(text_device, &bgl_desc);
    
    // Create bind group
    WGPUBindGroupEntry bg_entries[] = {
        {
            .binding = 0,
            .buffer = text_uniform_buffer,
            .offset = 0,
            .size = sizeof(TextUniforms),
        },
        {
            .binding = 1,
            .textureView = font_texture_view,
        },
        {
            .binding = 2,
            .sampler = font_sampler,
        },
    };
    WGPUBindGroupDescriptor bg_desc = {
        .layout = text_bind_group_layout,
        .entryCount = 3,
        .entries = bg_entries,
    };
    text_bind_group = wgpuDeviceCreateBindGroup(text_device, &bg_desc);
    
    // Create pipeline layout
    WGPUPipelineLayoutDescriptor pl_desc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &text_bind_group_layout,
    };
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(text_device, &pl_desc);
    
    // Create render pipeline
    WGPUVertexAttribute attrs[] = {
        {.format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 0},
        {.format = WGPUVertexFormat_Float32x2, .offset = 8, .shaderLocation = 1},
    };
    WGPUVertexBufferLayout vb_layout = {
        .arrayStride = sizeof(TextVertex),
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
        .format = text_surface_format,
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
    text_pipeline = wgpuDeviceCreateRenderPipeline(text_device, &rp_desc);
    
    // Cleanup
    wgpuShaderModuleRelease(shader);
    wgpuPipelineLayoutRelease(pipeline_layout);
    
    printf("Text rendering pipeline created\n");
}

// Check if text rendering is ready
int text_is_ready(void) {
    return text_pipeline != NULL && font_data.loaded;
}

// Render text at a specific position
void render_text(WGPURenderPassEncoder pass, const char* text, float x, float y, float scale, float r, float g, float b) {
    if (!text_pipeline || !font_data.loaded) return;
    
    // Build vertices for the text
    TextVertex vertices[MAX_TEXT_VERTICES];
    text_vertex_count = build_text_vertices(text, x, y, scale, vertices);
    
    if (text_vertex_count == 0) return;
    
    // Upload vertices
    wgpuQueueWriteBuffer(text_queue, text_vertex_buffer, 0, vertices, text_vertex_count * sizeof(TextVertex));
    
    // Update uniforms - just use orthographic projection (no rotation/scale for text)
    TextUniforms uniforms;
    mat4_ortho(uniforms.transform, 0, (float)text_canvas_width, 0, (float)text_canvas_height);
    uniforms.color[0] = r;
    uniforms.color[1] = g;
    uniforms.color[2] = b;
    uniforms.color[3] = 1.0f;
    
    wgpuQueueWriteBuffer(text_queue, text_uniform_buffer, 0, &uniforms, sizeof(TextUniforms));
    
    // Draw text
    wgpuRenderPassEncoderSetPipeline(pass, text_pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, text_bind_group, 0, NULL);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, text_vertex_buffer, 0, text_vertex_count * sizeof(TextVertex));
    wgpuRenderPassEncoderDraw(pass, text_vertex_count, 1, 0, 0);
}
