#ifndef GAME_SIM_H
#define GAME_SIM_H

#include <stdint.h>
#include "players.h"
#include "protocol.h"

// Initialize game simulation
void game_sim_init(void);

// Shutdown game simulation
void game_sim_shutdown(void);

// Run one simulation tick (called at 20Hz)
// dt is the time delta in seconds (typically 0.05s)
void game_sim_tick(float dt);

// Get current tick number
uint32_t game_sim_get_tick(void);

// Get all player states for serialization
// Returns count, fills out_states array
int game_sim_get_player_states(PlayerStateData* out_states, int max_count);

#endif // GAME_SIM_H
