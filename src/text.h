#ifndef TEXT_H
#define TEXT_H

#include <webgpu/webgpu.h>

// Text rendering constants
#define MAX_GLYPHS 256
#define MAX_TEXT_VERTICES 1024  // Max characters * 6 vertices per char

// Glyph data from .fnt file
typedef struct {
    int id;
    float x, y;           // Position in texture (pixels)
    float width, height;  // Size in texture (pixels)
    float xoffset, yoffset;
    float xadvance;
} Glyph;

// Font data
typedef struct {
    Glyph glyphs[MAX_GLYPHS];
    float line_height;
    float base;
    float scale_w;  // Texture width
    float scale_h;  // Texture height
    int loaded;
} FontData;

// Initialize text rendering system
// Must be called after WebGPU device is ready
void text_init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat format);

// Load font data from a .fnt file path
void text_load_font_file(const char* fnt_path);

// Parse font data directly from string
void text_parse_fnt_data(const char* data);

// Upload font texture (called from JavaScript when image is loaded)
void upload_font_texture(unsigned char* data, int width, int height);

// Load font data (called from JavaScript)
void load_font_data(const char* data);

// Create the text rendering pipeline
// Called automatically when both font texture and data are ready
void text_create_pipeline(const char* shader_source);

// Render text at a specific position
void render_text(WGPURenderPassEncoder pass, const char* text, float x, float y, float scale, float r, float g, float b);

// Calculate text width for centering
float calculate_text_width(const char* text, float scale);

// Check if text rendering is ready
int text_is_ready(void);

// Update canvas dimensions (call when canvas resizes)
void text_set_canvas_size(int width, int height);

#endif // TEXT_H
