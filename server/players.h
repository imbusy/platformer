#ifndef PLAYERS_H
#define PLAYERS_H

#include <stdint.h>
#include <stdbool.h>
#include <libwebsockets.h>
#include "protocol_types.h"

// Player state
typedef struct {
    bool active;
    bool authenticated;
    int id;
    char name[MAX_PLAYER_NAME];
    char token[MAX_TOKEN_LENGTH];
    
    // Position and movement
    float x, y, z;
    float angle;
    float vz;  // Vertical velocity
    uint8_t flags;
    
    // Current input state
    uint8_t inputs;
    
    // WebSocket connection
    struct lws* wsi;
} Player;

// Initialize players system
void players_init(void);

// Shutdown players system
void players_shutdown(void);

// Add a new connection, returns player slot index or -1 if full
int players_add_connection(struct lws* wsi);

// Remove a connection by wsi
void players_remove_connection(struct lws* wsi);

// Find player by wsi, returns NULL if not found
Player* players_find_by_wsi(struct lws* wsi);

// Find player by id, returns NULL if not found
Player* players_find_by_id(int id);

// Authenticate a player with token
// Returns true if auth succeeds, populates player name
bool players_authenticate(Player* player, const char* token);

// Update player input state
void players_update_input(Player* player, uint8_t inputs);

// Get all active players
// Returns count, fills out_players array (up to max_count)
int players_get_all_active(Player** out_players, int max_count);

// Get count of authenticated players
int players_get_authenticated_count(void);

// Register a token -> name mapping (for demo purposes)
void players_register_token(const char* token, const char* name);

// Iterate over all authenticated players (for broadcasting)
typedef void (*PlayerIteratorFunc)(Player* player, void* userdata);
void players_for_each_authenticated(PlayerIteratorFunc func, void* userdata);

#endif // PLAYERS_H
