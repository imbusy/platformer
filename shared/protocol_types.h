#ifndef PROTOCOL_TYPES_H
#define PROTOCOL_TYPES_H

// Protocol version
#define PROTOCOL_VERSION 1

// Server configuration
#define SERVER_PORT 9000
#define SERVER_TICK_RATE 20        // Hz (50ms per tick)
#define SERVER_TICK_MS 50          // milliseconds per tick

// Limits
#define MAX_PLAYERS 64
#define MAX_PLAYER_NAME 32
#define MAX_TOKEN_LENGTH 64
#define MAX_CHAT_MESSAGE 256
#define MAX_CHAT_HISTORY 100
#define MAX_JSON_SIZE 4096

// Message types (client -> server)
#define MSG_TYPE_AUTH "auth"
#define MSG_TYPE_INPUT "input"
#define MSG_TYPE_CHAT "chat"

// Message types (server -> client)
#define MSG_TYPE_AUTH_OK "auth_ok"
#define MSG_TYPE_AUTH_FAIL "auth_fail"
#define MSG_TYPE_STATE "state"
#define MSG_TYPE_CHAT_BROADCAST "chat_broadcast"
#define MSG_TYPE_PLAYER_JOIN "player_join"
#define MSG_TYPE_PLAYER_LEAVE "player_leave"

// Input flags (bitmask for compact representation)
#define INPUT_UP     (1 << 0)
#define INPUT_DOWN   (1 << 1)
#define INPUT_LEFT   (1 << 2)
#define INPUT_RIGHT  (1 << 3)
#define INPUT_JUMP   (1 << 4)
#define INPUT_ACTION (1 << 5)

// Game constants (shared between client and server)
#define GAME_MOVE_SPEED 12.5f      // World units per second
#define GAME_ROTATE_SPEED 3.0f     // Radians per second
#define GAME_JUMP_VELOCITY 15.0f   // Initial jump velocity
#define GAME_GRAVITY 30.0f         // Gravity acceleration
#define GAME_WORLD_WIDTH 100.0f    // Default world width
#define GAME_WORLD_HEIGHT 75.0f    // Default world height

// Player state flags
#define PLAYER_FLAG_GROUNDED (1 << 0)
#define PLAYER_FLAG_JUMPING  (1 << 1)

#endif // PROTOCOL_TYPES_H
