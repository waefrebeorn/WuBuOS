/* wubu_bottles_json.c -- Bottle JSON parsing helpers (self-contained).
 *
 * Pure JSON literal extraction: json_find_string/int/bool_literal. Used by
 * wubu_bottle_save/load and other bottle ops. Types via wubu_bottles.h.
 * Minimal includes.
 */

#include "wubu_bottles_internal.h"

const char *json_find_string_literal(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *key_pos = strstr(json, search);
    if (!key_pos) return NULL;
    const char *colon = strchr(key_pos + strlen(search), ':');
    if (!colon) return NULL;
    const char *quote = strchr(colon, '"');
    if (!quote) return NULL;
    const char *end = strchr(quote + 1, '"');
    if (!end) return NULL;
    return quote + 1;
}

int json_find_int_literal(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *key_pos = strstr(json, search);
    if (!key_pos) return 0;
    const char *colon = strchr(key_pos + strlen(search), ':');
    if (!colon) return 0;
    while (*colon && !isdigit((unsigned char)*colon) && *colon != '-') colon++;
    return atoi(colon);
}

bool json_find_bool_literal(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *key_pos = strstr(json, search);
    if (!key_pos) return false;
    const char *colon = strchr(key_pos + strlen(search), ':');
    if (!colon) return false;
    while (*colon && isspace((unsigned char)*colon)) colon++;
    if (strncmp(colon, "true", 4) == 0) return true;
    if (strncmp(colon, "false", 5) == 0) return false;
    return false;
}
