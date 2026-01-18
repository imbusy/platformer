#include "game.h"
#include "text.h"
#include "math.h"
#include "network.h"
#include <emscripten.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// Global game state
static Sprite sprite;
static InputState input;

// Colors for different players
static const float player_colors[][3] = {
    {0.2f, 0.8f, 0.3f},  // Green (local player)
    {0.3f, 0.5f, 0.9f},  // Blue
    {0.9f, 0.4f, 0.3f},  // Red
    {0.9f, 0.8f, 0.2f},  // Yellow
    {0.8f, 0.3f, 0.8f},  // Purple
    {0.3f, 0.9f, 0.9f},  // Cyan
    {0.9f, 0.6f, 0.3f},  // Orange
    {0.6f, 0.9f, 0.6f},  // Light green
};
#define NUM_COLORS (sizeof(player_colors) / sizeof(player_colors[0]))

void game_init(int canvas_width, int canvas_height) {
    // Initialize sprite at center of canvas (in world units)
    float world_width = canvas_width / PIXELS_PER_UNIT;
    float world_height = canvas_height / PIXELS_PER_UNIT;
    sprite.x = world_width / 2.0f;
    sprite.y = world_height / 2.0f;
    sprite.z = 0.0f;
    sprite.angle = 0.0f;
    sprite.speed = 0.0f;
    
    // Initialize input
    memset(&input, 0, sizeof(input));
    
    // Initialize network
    network_init();
    
    printf("Game initialized\n");
}

void game_update(float dt, int canvas_width, int canvas_height) {
    // Check if we're connected to server
    NetworkState net_state = network_get_state();
    
    if (net_state == NET_STATE_AUTHENTICATED) {
        // Get our state from server
        int local_id = network_get_local_player_id();
        const RemotePlayer* local = network_get_player_by_id(local_id);
        
        if (local) {
            // Update local sprite from server state
            sprite.x = local->x;
            sprite.y = local->y;
            sprite.z = local->z;
            sprite.angle = local->angle;
        }
    } else {
        // Offline mode: update locally
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
            sprite.x += sinf(sprite.angle) * move;
            sprite.y += cosf(sprite.angle) * move;
        }
        
        // Keep sprite on screen with wrapping (in world units)
        float world_width = canvas_width / PIXELS_PER_UNIT;
        float world_height = canvas_height / PIXELS_PER_UNIT;
        if (sprite.x < -SPRITE_SIZE) sprite.x = world_width + SPRITE_SIZE;
        if (sprite.x > world_width + SPRITE_SIZE) sprite.x = -SPRITE_SIZE;
        if (sprite.y < -SPRITE_SIZE) sprite.y = world_height + SPRITE_SIZE;
        if (sprite.y > world_height + SPRITE_SIZE) sprite.y = -SPRITE_SIZE;
    }
}

const Sprite* game_get_sprite(void) {
    return &sprite;
}

void game_render(const RenderContext* ctx) {
    if (!text_is_ready()) return;
    
    NetworkState net_state = network_get_state();
    int local_id = network_get_local_player_id();
    
    if (net_state == NET_STATE_AUTHENTICATED) {
        // Render all remote players
        RemotePlayer players[MAX_PLAYERS];
        int count = network_get_remote_players(players, MAX_PLAYERS);
        
        for (int i = 0; i < count; i++) {
            const RemotePlayer* rp = &players[i];
            if (!rp->active) continue;
            
            // Skip local player (rendered by main sprite system)
            if (rp->id == local_id) continue;
            
            // Get color for this player
            int color_idx = (rp->id % (NUM_COLORS - 1)) + 1;  // Skip green for others
            
            // Convert world position to pixel position for text
            float pixel_x = rp->x * PIXELS_PER_UNIT;
            float pixel_y = rp->y * PIXELS_PER_UNIT;
            
            // Render player name above their position
            float text_scale = 0.4f;
            float text_width = calculate_text_width(rp->name, text_scale);
            float text_x = pixel_x - text_width / 2.0f;
            float text_y = pixel_y + (SPRITE_SIZE * PIXELS_PER_UNIT) / 2.0f + 30.0f;
            
            render_text(ctx->pass, rp->name, text_x, text_y, text_scale,
                       player_colors[color_idx][0],
                       player_colors[color_idx][1],
                       player_colors[color_idx][2]);
        }
        
        // Render local player name
        const char* local_name = network_get_local_player_name();
        if (local_name && local_name[0]) {
            float sprite_pixel_x = sprite.x * PIXELS_PER_UNIT;
            float sprite_pixel_y = sprite.y * PIXELS_PER_UNIT;
            float text_scale = 0.5f;
            float text_width = calculate_text_width(local_name, text_scale);
            float text_x = sprite_pixel_x - text_width / 2.0f;
            float text_y = sprite_pixel_y + (SPRITE_SIZE * PIXELS_PER_UNIT) / 2.0f + 50.0f;
            
            render_text(ctx->pass, local_name, text_x, text_y, text_scale, 
                       0.2f, 0.8f, 0.3f);  // Green for local player
        }
    } else {
        // Offline mode: render "Hello, World!" above sprite
        const char* hello_text = "Hello, World!";
        float text_scale = 0.5f;
        float text_width = calculate_text_width(hello_text, text_scale);
        float sprite_pixel_x = sprite.x * PIXELS_PER_UNIT;
        float sprite_pixel_y = sprite.y * PIXELS_PER_UNIT;
        float text_x = sprite_pixel_x - text_width / 2.0f;
        float text_y = sprite_pixel_y + (SPRITE_SIZE * PIXELS_PER_UNIT) / 2.0f + 50.0f;
        
        render_text(ctx->pass, hello_text, text_x, text_y, text_scale, 1.0f, 1.0f, 1.0f);
    }
    
    // Render connection status in corner
    if (net_state != NET_STATE_AUTHENTICATED) {
        const char* status_text = "Offline - Press Connect";
        if (net_state == NET_STATE_CONNECTING) status_text = "Connecting...";
        else if (net_state == NET_STATE_CONNECTED) status_text = "Authenticating...";
        
        render_text(ctx->pass, status_text, 10, ctx->canvas_height - 30, 0.3f, 
                   0.8f, 0.8f, 0.2f);
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
