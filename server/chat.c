#include "chat.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

// Circular buffer for chat history
static ChatEntry chat_history[MAX_CHAT_HISTORY];
static int chat_head = 0;  // Next write position
static int chat_count = 0;

// Get current timestamp in milliseconds
static uint64_t get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void chat_init(void) {
    memset(chat_history, 0, sizeof(chat_history));
    chat_head = 0;
    chat_count = 0;
    printf("[Chat] Initialized\n");
}

void chat_shutdown(void) {
    chat_head = 0;
    chat_count = 0;
}

bool chat_add_message(int player_id, const char* player_name, const char* message) {
    if (!player_name || !message) return false;
    
    // Skip empty messages
    if (message[0] == '\0') return false;
    
    ChatEntry* entry = &chat_history[chat_head];
    entry->player_id = player_id;
    entry->timestamp = get_timestamp_ms();
    
    strncpy(entry->player_name, player_name, MAX_PLAYER_NAME - 1);
    entry->player_name[MAX_PLAYER_NAME - 1] = '\0';
    
    strncpy(entry->message, message, MAX_CHAT_MESSAGE - 1);
    entry->message[MAX_CHAT_MESSAGE - 1] = '\0';
    
    // Advance circular buffer
    chat_head = (chat_head + 1) % MAX_CHAT_HISTORY;
    if (chat_count < MAX_CHAT_HISTORY) {
        chat_count++;
    }
    
    printf("[Chat] <%s> %s\n", player_name, message);
    return true;
}

int chat_get_recent(ChatEntry* out_entries, int max_count) {
    if (!out_entries || max_count <= 0) return 0;
    
    int to_return = chat_count < max_count ? chat_count : max_count;
    
    // Start from most recent (chat_head - 1) and go backwards
    for (int i = 0; i < to_return; i++) {
        int idx = (chat_head - 1 - i + MAX_CHAT_HISTORY) % MAX_CHAT_HISTORY;
        out_entries[i] = chat_history[idx];
    }
    
    return to_return;
}

int chat_get_count(void) {
    return chat_count;
}
