#ifndef CHAT_H
#define CHAT_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol_types.h"

// Chat message entry
typedef struct {
    int player_id;
    char player_name[MAX_PLAYER_NAME];
    char message[MAX_CHAT_MESSAGE];
    uint64_t timestamp;  // Unix timestamp in milliseconds
} ChatEntry;

// Initialize chat system
void chat_init(void);

// Shutdown chat system
void chat_shutdown(void);

// Add a chat message to history
// Returns true on success
bool chat_add_message(int player_id, const char* player_name, const char* message);

// Get recent chat messages
// Returns count, fills out_entries array (newest first)
int chat_get_recent(ChatEntry* out_entries, int max_count);

// Get total message count
int chat_get_count(void);

#endif // CHAT_H
