#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "protocol_types.h"
#include "protocol.h"
#include "players.h"
#include "game_sim.h"
#include "chat.h"

static int interrupted = 0;
static struct lws_context* context = NULL;

// Scheduled timer for game ticks
static struct lws_sorted_usec_list game_tick_timer;

// Per-connection data
struct per_session_data {
    struct lws* wsi;
    int player_slot;
};

// Send a message to a specific client
static int send_message(struct lws* wsi, const char* msg) {
    if (!wsi || !msg) return -1;
    
    size_t len = strlen(msg);
    // LWS requires LWS_PRE bytes before the data
    unsigned char* buf = malloc(LWS_PRE + len);
    if (!buf) return -1;
    
    memcpy(buf + LWS_PRE, msg, len);
    int result = lws_write(wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);
    free(buf);
    
    return result;
}

// Broadcast context for iteration
typedef struct {
    const char* message;
    struct lws* exclude;  // Optional: exclude this wsi
} BroadcastCtx;

// Broadcast callback
static void broadcast_to_player(Player* player, void* userdata) {
    BroadcastCtx* ctx = (BroadcastCtx*)userdata;
    if (player->wsi && player->wsi != ctx->exclude) {
        send_message(player->wsi, ctx->message);
    }
}

// Broadcast a message to all authenticated players
static void broadcast_message(const char* msg, struct lws* exclude) {
    BroadcastCtx ctx = { .message = msg, .exclude = exclude };
    players_for_each_authenticated(broadcast_to_player, &ctx);
}

// Handle incoming message from client
static void handle_client_message(struct lws* wsi, const char* msg, size_t len) {
    Player* player = players_find_by_wsi(wsi);
    if (!player) {
        printf("[Server] Message from unknown connection\n");
        return;
    }
    
    // Create null-terminated copy
    char* json = malloc(len + 1);
    if (!json) return;
    memcpy(json, msg, len);
    json[len] = '\0';
    
    ClientMessage client_msg;
    if (!protocol_parse_client_message(json, &client_msg)) {
        printf("[Server] Failed to parse message: %.*s\n", (int)len, msg);
        free(json);
        return;
    }
    free(json);
    
    switch (client_msg.type) {
        case CLIENT_MSG_AUTH: {
            if (player->authenticated) {
                // Already authenticated
                char* response = protocol_serialize_auth_fail("already authenticated");
                if (response) {
                    send_message(wsi, response);
                    free(response);
                }
                break;
            }
            
            if (players_authenticate(player, client_msg.data.auth.token)) {
                // Send auth success
                char* response = protocol_serialize_auth_ok(player->id, player->name);
                if (response) {
                    send_message(wsi, response);
                    free(response);
                }
                
                // Broadcast player join to others
                char* join_msg = protocol_serialize_player_join(player->id, player->name);
                if (join_msg) {
                    broadcast_message(join_msg, wsi);
                    free(join_msg);
                }
                
                printf("[Server] Player %d (%s) joined\n", player->id, player->name);
            } else {
                char* response = protocol_serialize_auth_fail("invalid token");
                if (response) {
                    send_message(wsi, response);
                    free(response);
                }
            }
            break;
        }
        
        case CLIENT_MSG_INPUT: {
            if (!player->authenticated) {
                printf("[Server] Input from unauthenticated player\n");
                break;
            }
            players_update_input(player, client_msg.data.input.inputs);
            break;
        }
        
        case CLIENT_MSG_CHAT: {
            if (!player->authenticated) {
                printf("[Server] Chat from unauthenticated player\n");
                break;
            }
            
            // Add to chat history
            chat_add_message(player->id, player->name, client_msg.data.chat.message);
            
            // Broadcast to all players
            char* chat_msg = protocol_serialize_chat_broadcast(
                player->id, player->name, client_msg.data.chat.message);
            if (chat_msg) {
                broadcast_message(chat_msg, NULL);  // Include sender
                free(chat_msg);
            }
            break;
        }
        
        default:
            printf("[Server] Unknown message type\n");
            break;
    }
}

// Game tick callback
static void game_tick_callback(struct lws_sorted_usec_list* sul) {
    (void)sul;
    
    // Run simulation
    float dt = SERVER_TICK_MS / 1000.0f;
    game_sim_tick(dt);
    
    // Get player states and broadcast
    int player_count = players_get_authenticated_count();
    if (player_count > 0) {
        PlayerStateData states[MAX_PLAYERS];
        int count = game_sim_get_player_states(states, MAX_PLAYERS);
        
        char* state_msg = protocol_serialize_state(game_sim_get_tick(), states, count);
        if (state_msg) {
            broadcast_message(state_msg, NULL);
            free(state_msg);
        }
    }
    
    // Schedule next tick
    lws_sul_schedule(context, 0, &game_tick_timer, game_tick_callback, 
                     SERVER_TICK_MS * 1000);  // Convert ms to us
}

// WebSocket protocol callback
static int callback_game(struct lws* wsi, enum lws_callback_reasons reason,
                         void* user, void* in, size_t len) {
    struct per_session_data* pss = (struct per_session_data*)user;
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            printf("[Server] New WebSocket connection\n");
            pss->wsi = wsi;
            pss->player_slot = players_add_connection(wsi);
            if (pss->player_slot < 0) {
                printf("[Server] Server full, rejecting connection\n");
                return -1;
            }
            break;
        }
        
        case LWS_CALLBACK_CLOSED: {
            printf("[Server] WebSocket connection closed\n");
            Player* player = players_find_by_wsi(wsi);
            if (player && player->authenticated) {
                // Broadcast player leave
                char* leave_msg = protocol_serialize_player_leave(player->id);
                if (leave_msg) {
                    broadcast_message(leave_msg, wsi);
                    free(leave_msg);
                }
            }
            players_remove_connection(wsi);
            break;
        }
        
        case LWS_CALLBACK_RECEIVE: {
            handle_client_message(wsi, (const char*)in, len);
            break;
        }
        
        default:
            break;
    }
    
    return 0;
}

// Protocol definition
static struct lws_protocols protocols[] = {
    {
        .name = "game-protocol",
        .callback = callback_game,
        .per_session_data_size = sizeof(struct per_session_data),
        .rx_buffer_size = MAX_JSON_SIZE,
    },
    { NULL, NULL, 0, 0 }  // Terminator
};

// Signal handler
static void sigint_handler(int sig) {
    (void)sig;
    interrupted = 1;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    // Set up signal handler
    signal(SIGINT, sigint_handler);
    
    // Initialize subsystems
    players_init();
    game_sim_init();
    chat_init();
    
    // Create libwebsockets context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = SERVER_PORT;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
    
    context = lws_create_context(&info);
    if (!context) {
        printf("[Server] Failed to create libwebsockets context\n");
        return 1;
    }
    
    printf("[Server] Game server started on port %d\n", SERVER_PORT);
    printf("[Server] Tick rate: %d Hz (%d ms)\n", SERVER_TICK_RATE, SERVER_TICK_MS);
    
    // Start game tick timer
    lws_sul_schedule(context, 0, &game_tick_timer, game_tick_callback, 
                     SERVER_TICK_MS * 1000);
    
    // Main event loop
    while (!interrupted) {
        lws_service(context, 0);
    }
    
    printf("[Server] Shutting down...\n");
    
    // Cleanup
    lws_sul_cancel(&game_tick_timer);
    lws_context_destroy(context);
    
    chat_shutdown();
    game_sim_shutdown();
    players_shutdown();
    
    printf("[Server] Goodbye!\n");
    return 0;
}
