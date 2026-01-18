#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include "../shared/protocol_types.h"

// Connection state
typedef enum {
    NET_STATE_DISCONNECTED = 0,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_AUTHENTICATED
} NetworkState;

// Remote player state (received from server)
typedef struct {
    int id;
    float x, y, z;
    float angle;
    char name[MAX_PLAYER_NAME];
    bool active;
} RemotePlayer;

// Initialize network system
void network_init(void);

// Shutdown network system
void network_shutdown(void);

// Connect to server
// url: WebSocket URL (e.g., "ws://localhost:9000")
void network_connect(const char* url);

// Disconnect from server
void network_disconnect(void);

// Authenticate with token
void network_authenticate(const char* token);

// Send input state to server
void network_send_input(uint8_t inputs);

// Send chat message
void network_send_chat(const char* message);

// Get connection state
NetworkState network_get_state(void);

// Get local player ID (valid after authentication)
int network_get_local_player_id(void);

// Get local player name (valid after authentication)
const char* network_get_local_player_name(void);

// Get all remote players
// Returns count, fills out_players array
int network_get_remote_players(RemotePlayer* out_players, int max_count);

// Get a specific remote player by ID
const RemotePlayer* network_get_player_by_id(int id);

// Get current server tick
uint32_t network_get_server_tick(void);

// Called from JavaScript when WebSocket events occur
// These are exported functions called via Emscripten
void network_on_open(void);
void network_on_close(void);
void network_on_error(void);
void network_on_message(const char* data, int length);

#endif // NETWORK_H
