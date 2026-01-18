#ifndef GAME_H
#define GAME_H

#include <webgpu/webgpu.h>

// Game constants
#define SPRITE_SIZE 4.0f
#define PIXELS_PER_UNIT 16.0f  // 4 units * 16 = 64 pixels (same visual size as before)

// Sprite state
typedef struct {
    float x;
    float y;
    float z;      // depth: 0 = at camera plane, negative = farther from camera
    float angle;  // in radians
    float speed;
} Sprite;

// Render context passed to game for rendering operations
typedef struct {
    WGPURenderPassEncoder pass;
    int canvas_width;
    int canvas_height;
} RenderContext;

// Initialize game state (sprite position, input)
void game_init(int canvas_width, int canvas_height);

// Update game state (call each frame with delta time)
void game_update(float dt, int canvas_width, int canvas_height);

// Render game objects (call during render pass)
void game_render(const RenderContext* ctx);

// Get current sprite state (for rendering)
const Sprite* game_get_sprite(void);

#endif // GAME_H
