#include "network.h"
#include <emscripten.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Network state
static NetworkState state = NET_STATE_DISCONNECTED;
static int local_player_id = -1;
static char local_player_name[MAX_PLAYER_NAME] = "";
static uint32_t server_tick = 0;

// Remote players
static RemotePlayer remote_players[MAX_PLAYERS];
static int remote_player_count = 0;

// Pending auth token
static char pending_token[MAX_TOKEN_LENGTH] = "";

void network_init(void) {
    state = NET_STATE_DISCONNECTED;
    local_player_id = -1;
    local_player_name[0] = '\0';
    server_tick = 0;
    remote_player_count = 0;
    memset(remote_players, 0, sizeof(remote_players));
    pending_token[0] = '\0';
    
    printf("[Network] Initialized\n");
}

void network_shutdown(void) {
    network_disconnect();
    state = NET_STATE_DISCONNECTED;
}

void network_connect(const char* url) {
    if (state != NET_STATE_DISCONNECTED) {
        printf("[Network] Already connected or connecting\n");
        return;
    }
    
    state = NET_STATE_CONNECTING;
    printf("[Network] Connecting to %s\n", url);
    
    // Call JavaScript to create WebSocket
    EM_ASM({
        var url = UTF8ToString($0);
        window.gameWebSocket = new WebSocket(url);
        
        window.gameWebSocket.onopen = function() {
            Module._network_on_open();
        };
        
        window.gameWebSocket.onclose = function() {
            Module._network_on_close();
        };
        
        window.gameWebSocket.onerror = function() {
            Module._network_on_error();
        };
        
        window.gameWebSocket.onmessage = function(event) {
            var data = event.data;
            var len = lengthBytesUTF8(data) + 1;
            var buf = _malloc(len);
            stringToUTF8(data, buf, len);
            Module._network_on_message(buf, len - 1);
            _free(buf);
        };
    }, url);
}

void network_disconnect(void) {
    if (state == NET_STATE_DISCONNECTED) return;
    
    EM_ASM({
        if (window.gameWebSocket) {
            window.gameWebSocket.close();
            window.gameWebSocket = null;
        }
    });
    
    state = NET_STATE_DISCONNECTED;
    local_player_id = -1;
    local_player_name[0] = '\0';
    remote_player_count = 0;
    
    printf("[Network] Disconnected\n");
}

void network_authenticate(const char* token) {
    if (state != NET_STATE_CONNECTED) {
        // Save token and auth when connected
        strncpy(pending_token, token, MAX_TOKEN_LENGTH - 1);
        pending_token[MAX_TOKEN_LENGTH - 1] = '\0';
        printf("[Network] Auth pending (not connected yet)\n");
        return;
    }
    
    printf("[Network] Authenticating with token: %s\n", token);
    
    EM_ASM({
        var token = UTF8ToString($0);
        var msg = JSON.stringify({type: "auth", token: token});
        if (window.gameWebSocket && window.gameWebSocket.readyState === 1) {
            window.gameWebSocket.send(msg);
        }
    }, token);
}

void network_send_input(uint8_t inputs) {
    if (state != NET_STATE_AUTHENTICATED) return;
    
    EM_ASM({
        var inputs = $0;
        var msg = JSON.stringify({
            type: "input",
            up: (inputs & 1) ? 1 : 0,
            down: (inputs & 2) ? 1 : 0,
            left: (inputs & 4) ? 1 : 0,
            right: (inputs & 8) ? 1 : 0,
            jump: (inputs & 16) ? 1 : 0,
            action: (inputs & 32) ? 1 : 0
        });
        if (window.gameWebSocket && window.gameWebSocket.readyState === 1) {
            window.gameWebSocket.send(msg);
        }
    }, inputs);
}

void network_send_chat(const char* message) {
    if (state != NET_STATE_AUTHENTICATED) return;
    if (!message || message[0] == '\0') return;
    
    EM_ASM({
        var message = UTF8ToString($0);
        var msg = JSON.stringify({type: "chat", msg: message});
        if (window.gameWebSocket && window.gameWebSocket.readyState === 1) {
            window.gameWebSocket.send(msg);
        }
    }, message);
}

NetworkState network_get_state(void) {
    return state;
}

int network_get_local_player_id(void) {
    return local_player_id;
}

const char* network_get_local_player_name(void) {
    return local_player_name;
}

int network_get_remote_players(RemotePlayer* out_players, int max_count) {
    int count = 0;
    for (int i = 0; i < remote_player_count && count < max_count; i++) {
        if (remote_players[i].active) {
            out_players[count++] = remote_players[i];
        }
    }
    return count;
}

const RemotePlayer* network_get_player_by_id(int id) {
    for (int i = 0; i < remote_player_count; i++) {
        if (remote_players[i].active && remote_players[i].id == id) {
            return &remote_players[i];
        }
    }
    return NULL;
}

uint32_t network_get_server_tick(void) {
    return server_tick;
}

// JavaScript callbacks (exported via Emscripten)

EMSCRIPTEN_KEEPALIVE
void network_on_open(void) {
    printf("[Network] Connected!\n");
    state = NET_STATE_CONNECTED;
    EM_ASM({
        window.onNetworkConnected();
    });
}

EMSCRIPTEN_KEEPALIVE
void network_on_close(void) {
    printf("[Network] Connection closed\n");
    state = NET_STATE_DISCONNECTED;
    local_player_id = -1;
    local_player_name[0] = '\0';
    remote_player_count = 0;
}

EMSCRIPTEN_KEEPALIVE
void network_on_error(void) {
    printf("[Network] Connection error\n");
    state = NET_STATE_DISCONNECTED;
}

// Simple JSON parsing helpers (avoiding external dependencies in client)
static const char* json_find_string(const char* json, const char* key, char* out, int max_len) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char* start = strstr(json, search);
    if (!start) return NULL;
    
    start += strlen(search);
    const char* end = strchr(start, '"');
    if (!end) return NULL;
    
    int len = end - start;
    if (len >= max_len) len = max_len - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    return out;
}

static int json_find_int(const char* json, const char* key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* start = strstr(json, search);
    if (!start) return 0;
    
    start += strlen(search);
    return atoi(start);
}

static float json_find_float(const char* json, const char* key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* start = strstr(json, search);
    if (!start) return 0.0f;
    
    start += strlen(search);
    return (float)atof(start);
}

EMSCRIPTEN_KEEPALIVE
void network_on_message(const char* data, int length) {
    (void)length;
    
    char type[32] = "";
    json_find_string(data, "type", type, sizeof(type));
    
    if (strcmp(type, "auth_ok") == 0) {
        local_player_id = json_find_int(data, "player_id");
        json_find_string(data, "name", local_player_name, MAX_PLAYER_NAME);
        state = NET_STATE_AUTHENTICATED;
        printf("[Network] Authenticated as %s (id %d)\n", local_player_name, local_player_id);
        EM_ASM({
            window.onNetworkAuthenticated();
        });
    }
    else if (strcmp(type, "auth_fail") == 0) {
        char reason[64] = "";
        json_find_string(data, "reason", reason, sizeof(reason));
        printf("[Network] Auth failed: %s\n", reason);
    }
    else if (strcmp(type, "state") == 0) {
        server_tick = json_find_int(data, "tick");
        
        // Parse players array - simple parsing
        // Clear previous state
        for (int i = 0; i < remote_player_count; i++) {
            remote_players[i].active = false;
        }
        remote_player_count = 0;
        
        // Find players array
        const char* players_start = strstr(data, "\"players\":[");
        if (players_start) {
            players_start = strchr(players_start, '[') + 1;
            const char* players_end = strstr(players_start, "]}");
            if (!players_end) players_end = data + strlen(data);
            
            // Parse each player object
            const char* obj_start = players_start;
            while ((obj_start = strchr(obj_start, '{')) && obj_start < players_end) {
                const char* obj_end = strchr(obj_start, '}');
                if (!obj_end || obj_end > players_end) break;
                
                // Extract player data from this object
                char obj_buf[512];
                int obj_len = obj_end - obj_start + 1;
                if (obj_len >= (int)sizeof(obj_buf)) obj_len = sizeof(obj_buf) - 1;
                strncpy(obj_buf, obj_start, obj_len);
                obj_buf[obj_len] = '\0';
                
                if (remote_player_count < MAX_PLAYERS) {
                    RemotePlayer* rp = &remote_players[remote_player_count];
                    rp->id = json_find_int(obj_buf, "id");
                    rp->x = json_find_float(obj_buf, "x");
                    rp->y = json_find_float(obj_buf, "y");
                    rp->z = json_find_float(obj_buf, "z");
                    rp->angle = json_find_float(obj_buf, "angle");
                    json_find_string(obj_buf, "name", rp->name, MAX_PLAYER_NAME);
                    rp->active = true;
                    remote_player_count++;
                }
                
                obj_start = obj_end + 1;
            }
        }
    }
    else if (strcmp(type, "chat_broadcast") == 0) {
        int player_id = json_find_int(data, "player_id");
        char name[MAX_PLAYER_NAME] = "";
        char msg[MAX_CHAT_MESSAGE] = "";
        json_find_string(data, "name", name, MAX_PLAYER_NAME);
        json_find_string(data, "msg", msg, MAX_CHAT_MESSAGE);

        printf("[Chat] <%s> %s\n", name, msg);

        EM_ASM({
            if (window.onChatMessage) {
                window.onChatMessage($0, UTF8ToString($1), UTF8ToString($2));
            }
        }, player_id, name, msg);
    }
    else if (strcmp(type, "player_join") == 0) {
        int player_id = json_find_int(data, "player_id");
        char name[MAX_PLAYER_NAME] = "";
        json_find_string(data, "name", name, MAX_PLAYER_NAME);

        printf("[Network] Player joined: %s (id %d)\n", name, player_id);

        EM_ASM({
            if (window.onPlayerJoin) {
                window.onPlayerJoin($0, UTF8ToString($1));
            }
        }, player_id, name);
    }
    else if (strcmp(type, "player_leave") == 0) {
        int player_id = json_find_int(data, "player_id");

        printf("[Network] Player left: id %d\n", player_id);

        EM_ASM({
            if (window.onPlayerLeave) {
                window.onPlayerLeave($0);
            }
        }, player_id);
    }
}
