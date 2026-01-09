#include "game.h"
#include "text.h"
#include "math.h"
#include <emscripten.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// Global game state
static Sprite sprite;
static InputState input;

void game_init(int canvas_width, int canvas_height) {
    // Initialize sprite at center of canvas
    sprite.x = canvas_width / 2.0f;
    sprite.y = canvas_height / 2.0f;
    sprite.z = 0.0f;  // At camera plane (z=0), negative moves away from camera
    sprite.angle = 0.0f;
    sprite.speed = 0.0f;
    
    // Initialize input
    memset(&input, 0, sizeof(input));
    
    printf("Game initialized\n");
}

void game_update(float dt, int canvas_width, int canvas_height) {
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

const Sprite* game_get_sprite(void) {
    return &sprite;
}

void game_render(const RenderContext* ctx) {
    // Draw "Hello, World!" text above the sprite
    if (text_is_ready()) {
        const char* hello_text = "Hello, World!";
        float text_scale = 0.5f;  // Scale down the font
        float text_width = calculate_text_width(hello_text, text_scale);
        float text_x = sprite.x - text_width / 2.0f;  // Center above sprite
        float text_y = sprite.y + SPRITE_SIZE / 2.0f + 50.0f;  // Position above sprite
        
        render_text(ctx->pass, hello_text, text_x, text_y, text_scale, 1.0f, 1.0f, 1.0f);  // White text
    }
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
