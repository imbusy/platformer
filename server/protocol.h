#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol_types.h"

// Parsed client message types
typedef enum {
    CLIENT_MSG_UNKNOWN = 0,
    CLIENT_MSG_AUTH,
    CLIENT_MSG_INPUT,
    CLIENT_MSG_CHAT
} ClientMsgType;

// Auth message
typedef struct {
    char token[MAX_TOKEN_LENGTH];
} AuthMsg;

// Input message
typedef struct {
    uint8_t inputs;  // Bitmask of INPUT_* flags
} InputMsg;

// Chat message
typedef struct {
    char message[MAX_CHAT_MESSAGE];
} ChatMsg;

// Parsed client message
typedef struct {
    ClientMsgType type;
    union {
        AuthMsg auth;
        InputMsg input;
        ChatMsg chat;
    } data;
} ClientMessage;

// Player state for serialization
typedef struct {
    int id;
    float x, y, z;
    float angle;
    float vz;  // Vertical velocity for jump
    uint8_t flags;
    char name[MAX_PLAYER_NAME];
} PlayerStateData;

// Parse a client message from JSON string
// Returns true on success, false on parse error
bool protocol_parse_client_message(const char* json, ClientMessage* msg);

// Serialize auth success response
// Returns allocated string (caller must free) or NULL on error
char* protocol_serialize_auth_ok(int player_id, const char* name);

// Serialize auth failure response
char* protocol_serialize_auth_fail(const char* reason);

// Serialize game state update
// players: array of player states, count: number of players
char* protocol_serialize_state(uint32_t tick, const PlayerStateData* players, int count);

// Serialize chat broadcast
char* protocol_serialize_chat_broadcast(int player_id, const char* name, const char* message);

// Serialize player join notification
char* protocol_serialize_player_join(int player_id, const char* name);

// Serialize player leave notification
char* protocol_serialize_player_leave(int player_id);

#endif // PROTOCOL_H
