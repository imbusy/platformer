#include "protocol.h"
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool protocol_parse_client_message(const char* json, ClientMessage* msg) {
    if (!json || !msg) return false;
    
    memset(msg, 0, sizeof(ClientMessage));
    msg->type = CLIENT_MSG_UNKNOWN;
    
    cJSON* root = cJSON_Parse(json);
    if (!root) {
        return false;
    }
    
    cJSON* type_item = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type_item)) {
        cJSON_Delete(root);
        return false;
    }
    
    const char* type_str = type_item->valuestring;
    
    if (strcmp(type_str, MSG_TYPE_AUTH) == 0) {
        msg->type = CLIENT_MSG_AUTH;
        cJSON* token = cJSON_GetObjectItem(root, "token");
        if (cJSON_IsString(token)) {
            strncpy(msg->data.auth.token, token->valuestring, MAX_TOKEN_LENGTH - 1);
            msg->data.auth.token[MAX_TOKEN_LENGTH - 1] = '\0';
        }
    }
    else if (strcmp(type_str, MSG_TYPE_INPUT) == 0) {
        msg->type = CLIENT_MSG_INPUT;
        msg->data.input.inputs = 0;
        
        cJSON* up = cJSON_GetObjectItem(root, "up");
        cJSON* down = cJSON_GetObjectItem(root, "down");
        cJSON* left = cJSON_GetObjectItem(root, "left");
        cJSON* right = cJSON_GetObjectItem(root, "right");
        cJSON* jump = cJSON_GetObjectItem(root, "jump");
        cJSON* action = cJSON_GetObjectItem(root, "action");
        
        if (cJSON_IsTrue(up) || (cJSON_IsNumber(up) && up->valueint)) 
            msg->data.input.inputs |= INPUT_UP;
        if (cJSON_IsTrue(down) || (cJSON_IsNumber(down) && down->valueint)) 
            msg->data.input.inputs |= INPUT_DOWN;
        if (cJSON_IsTrue(left) || (cJSON_IsNumber(left) && left->valueint)) 
            msg->data.input.inputs |= INPUT_LEFT;
        if (cJSON_IsTrue(right) || (cJSON_IsNumber(right) && right->valueint)) 
            msg->data.input.inputs |= INPUT_RIGHT;
        if (cJSON_IsTrue(jump) || (cJSON_IsNumber(jump) && jump->valueint)) 
            msg->data.input.inputs |= INPUT_JUMP;
        if (cJSON_IsTrue(action) || (cJSON_IsNumber(action) && action->valueint)) 
            msg->data.input.inputs |= INPUT_ACTION;
    }
    else if (strcmp(type_str, MSG_TYPE_CHAT) == 0) {
        msg->type = CLIENT_MSG_CHAT;
        cJSON* message = cJSON_GetObjectItem(root, "msg");
        if (cJSON_IsString(message)) {
            strncpy(msg->data.chat.message, message->valuestring, MAX_CHAT_MESSAGE - 1);
            msg->data.chat.message[MAX_CHAT_MESSAGE - 1] = '\0';
        }
    }
    
    cJSON_Delete(root);
    return msg->type != CLIENT_MSG_UNKNOWN;
}

char* protocol_serialize_auth_ok(int player_id, const char* name) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddStringToObject(root, "type", MSG_TYPE_AUTH_OK);
    cJSON_AddNumberToObject(root, "player_id", player_id);
    cJSON_AddStringToObject(root, "name", name ? name : "");
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char* protocol_serialize_auth_fail(const char* reason) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddStringToObject(root, "type", MSG_TYPE_AUTH_FAIL);
    cJSON_AddStringToObject(root, "reason", reason ? reason : "unknown");
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char* protocol_serialize_state(uint32_t tick, const PlayerStateData* players, int count) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddStringToObject(root, "type", MSG_TYPE_STATE);
    cJSON_AddNumberToObject(root, "tick", tick);
    
    cJSON* players_array = cJSON_CreateArray();
    if (!players_array) {
        cJSON_Delete(root);
        return NULL;
    }
    
    for (int i = 0; i < count; i++) {
        cJSON* player = cJSON_CreateObject();
        if (!player) continue;
        
        cJSON_AddNumberToObject(player, "id", players[i].id);
        cJSON_AddNumberToObject(player, "x", players[i].x);
        cJSON_AddNumberToObject(player, "y", players[i].y);
        cJSON_AddNumberToObject(player, "z", players[i].z);
        cJSON_AddNumberToObject(player, "angle", players[i].angle);
        cJSON_AddStringToObject(player, "name", players[i].name);
        
        cJSON_AddItemToArray(players_array, player);
    }
    
    cJSON_AddItemToObject(root, "players", players_array);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char* protocol_serialize_chat_broadcast(int player_id, const char* name, const char* message) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddStringToObject(root, "type", MSG_TYPE_CHAT_BROADCAST);
    cJSON_AddNumberToObject(root, "player_id", player_id);
    cJSON_AddStringToObject(root, "name", name ? name : "");
    cJSON_AddStringToObject(root, "msg", message ? message : "");
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char* protocol_serialize_player_join(int player_id, const char* name) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddStringToObject(root, "type", MSG_TYPE_PLAYER_JOIN);
    cJSON_AddNumberToObject(root, "player_id", player_id);
    cJSON_AddStringToObject(root, "name", name ? name : "");
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char* protocol_serialize_player_leave(int player_id) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddStringToObject(root, "type", MSG_TYPE_PLAYER_LEAVE);
    cJSON_AddNumberToObject(root, "player_id", player_id);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}
