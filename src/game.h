#ifndef GAME_H
#define GAME_H

#include <webgpu/webgpu.h>

// Game constants
#define SPRITE_SIZE 64.0f
#define MOVE_SPEED 200.0f
#define ROTATE_SPEED 3.0f

// Sprite state
typedef struct {
    float x;
    float y;
    float z;      // depth: 0 = at camera plane, negative = farther from camera
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

// Input handlers (called from JavaScript)
void on_key_down(int key_code);
void on_key_up(int key_code);

#endif // GAME_H
