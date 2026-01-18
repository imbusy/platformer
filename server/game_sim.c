#include "game_sim.h"
#include "protocol_types.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint32_t current_tick = 0;

void game_sim_init(void) {
    current_tick = 0;
    printf("[GameSim] Initialized\n");
}

void game_sim_shutdown(void) {
    current_tick = 0;
}

static void update_player(Player* player, float dt) {
    if (!player || !player->authenticated) return;
    
    uint8_t inputs = player->inputs;
    
    // Rotation (left/right)
    if (inputs & INPUT_LEFT) {
        player->angle -= GAME_ROTATE_SPEED * dt;
    }
    if (inputs & INPUT_RIGHT) {
        player->angle += GAME_ROTATE_SPEED * dt;
    }
    
    // Normalize angle to [0, 2*PI]
    while (player->angle < 0) player->angle += 2 * M_PI;
    while (player->angle >= 2 * M_PI) player->angle -= 2 * M_PI;
    
    // Forward/backward movement
    float move = 0.0f;
    if (inputs & INPUT_UP) move += GAME_MOVE_SPEED * dt;
    if (inputs & INPUT_DOWN) move -= GAME_MOVE_SPEED * dt;
    
    if (move != 0.0f) {
        // Move in facing direction (angle 0 = up/north)
        player->x += sinf(player->angle) * move;
        player->y += cosf(player->angle) * move;
    }
    
    // Jump handling
    if ((inputs & INPUT_JUMP) && (player->flags & PLAYER_FLAG_GROUNDED)) {
        player->vz = GAME_JUMP_VELOCITY;
        player->flags &= ~PLAYER_FLAG_GROUNDED;
        player->flags |= PLAYER_FLAG_JUMPING;
    }
    
    // Gravity and vertical movement
    if (!(player->flags & PLAYER_FLAG_GROUNDED)) {
        player->vz -= GAME_GRAVITY * dt;
        player->z += player->vz * dt;
        
        // Ground collision (z = 0 is ground)
        if (player->z <= 0.0f) {
            player->z = 0.0f;
            player->vz = 0.0f;
            player->flags |= PLAYER_FLAG_GROUNDED;
            player->flags &= ~PLAYER_FLAG_JUMPING;
        }
    }
    
    // World wrapping (keep players in bounds)
    if (player->x < 0) player->x += GAME_WORLD_WIDTH;
    if (player->x >= GAME_WORLD_WIDTH) player->x -= GAME_WORLD_WIDTH;
    if (player->y < 0) player->y += GAME_WORLD_HEIGHT;
    if (player->y >= GAME_WORLD_HEIGHT) player->y -= GAME_WORLD_HEIGHT;
}

void game_sim_tick(float dt) {
    current_tick++;
    
    // Update all authenticated players
    Player* active_players[MAX_PLAYERS];
    int count = players_get_all_active(active_players, MAX_PLAYERS);
    
    for (int i = 0; i < count; i++) {
        update_player(active_players[i], dt);
    }
}

uint32_t game_sim_get_tick(void) {
    return current_tick;
}

int game_sim_get_player_states(PlayerStateData* out_states, int max_count) {
    Player* active_players[MAX_PLAYERS];
    int count = players_get_all_active(active_players, MAX_PLAYERS);
    
    if (count > max_count) count = max_count;
    
    for (int i = 0; i < count; i++) {
        Player* p = active_players[i];
        out_states[i].id = p->id;
        out_states[i].x = p->x;
        out_states[i].y = p->y;
        out_states[i].z = p->z;
        out_states[i].angle = p->angle;
        out_states[i].vz = p->vz;
        out_states[i].flags = p->flags;
        strncpy(out_states[i].name, p->name, MAX_PLAYER_NAME - 1);
        out_states[i].name[MAX_PLAYER_NAME - 1] = '\0';
    }
    
    return count;
}
