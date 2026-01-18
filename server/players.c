#include "players.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Player slots
static Player players[MAX_PLAYERS];
static int next_player_id = 1;

// Simple token registry (for demo - in production use a database)
#define MAX_TOKENS 100
typedef struct {
    char token[MAX_TOKEN_LENGTH];
    char name[MAX_PLAYER_NAME];
    bool used;
} TokenEntry;
static TokenEntry token_registry[MAX_TOKENS];
static int token_count = 0;

void players_init(void) {
    memset(players, 0, sizeof(players));
    next_player_id = 1;
    
    // Register some demo tokens
    players_register_token("player1", "Alice");
    players_register_token("player2", "Bob");
    players_register_token("player3", "Charlie");
    players_register_token("debug", "Debug Player");
    
    printf("[Players] Initialized with %d demo tokens\n", token_count);
}

void players_shutdown(void) {
    memset(players, 0, sizeof(players));
    memset(token_registry, 0, sizeof(token_registry));
    token_count = 0;
}

int players_add_connection(struct lws* wsi) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active) {
            memset(&players[i], 0, sizeof(Player));
            players[i].active = true;
            players[i].authenticated = false;
            players[i].id = next_player_id++;
            players[i].wsi = wsi;
            
            // Default spawn position
            players[i].x = GAME_WORLD_WIDTH / 2.0f;
            players[i].y = GAME_WORLD_HEIGHT / 2.0f;
            players[i].z = 0.0f;
            players[i].angle = 0.0f;
            players[i].vz = 0.0f;
            players[i].flags = PLAYER_FLAG_GROUNDED;
            
            printf("[Players] New connection, slot %d, id %d\n", i, players[i].id);
            return i;
        }
    }
    printf("[Players] No free slots!\n");
    return -1;
}

void players_remove_connection(struct lws* wsi) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active && players[i].wsi == wsi) {
            printf("[Players] Removing player id %d (%s)\n", 
                   players[i].id, players[i].name);
            players[i].active = false;
            players[i].wsi = NULL;
            return;
        }
    }
}

Player* players_find_by_wsi(struct lws* wsi) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active && players[i].wsi == wsi) {
            return &players[i];
        }
    }
    return NULL;
}

Player* players_find_by_id(int id) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active && players[i].id == id) {
            return &players[i];
        }
    }
    return NULL;
}

bool players_authenticate(Player* player, const char* token) {
    if (!player || !token) return false;
    
    // Look up token in registry
    for (int i = 0; i < token_count; i++) {
        if (token_registry[i].used && 
            strcmp(token_registry[i].token, token) == 0) {
            
            player->authenticated = true;
            strncpy(player->token, token, MAX_TOKEN_LENGTH - 1);
            strncpy(player->name, token_registry[i].name, MAX_PLAYER_NAME - 1);
            
            printf("[Players] Player %d authenticated as '%s'\n", 
                   player->id, player->name);
            return true;
        }
    }
    
    printf("[Players] Auth failed for token: %s\n", token);
    return false;
}

void players_update_input(Player* player, uint8_t inputs) {
    if (player) {
        player->inputs = inputs;
    }
}

int players_get_all_active(Player** out_players, int max_count) {
    int count = 0;
    for (int i = 0; i < MAX_PLAYERS && count < max_count; i++) {
        if (players[i].active && players[i].authenticated) {
            out_players[count++] = &players[i];
        }
    }
    return count;
}

int players_get_authenticated_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active && players[i].authenticated) {
            count++;
        }
    }
    return count;
}

void players_register_token(const char* token, const char* name) {
    if (token_count >= MAX_TOKENS) {
        printf("[Players] Token registry full!\n");
        return;
    }
    
    strncpy(token_registry[token_count].token, token, MAX_TOKEN_LENGTH - 1);
    strncpy(token_registry[token_count].name, name, MAX_PLAYER_NAME - 1);
    token_registry[token_count].used = true;
    token_count++;
}

void players_for_each_authenticated(PlayerIteratorFunc func, void* userdata) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active && players[i].authenticated) {
            func(&players[i], userdata);
        }
    }
}
