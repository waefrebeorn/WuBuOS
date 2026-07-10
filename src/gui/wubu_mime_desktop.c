/* wubu_mime_desktop.c -- MIME .desktop file parser.
 *
 * Self-contained module extracted from wubu_mime.c: string helpers
 * (str_lower/trim/dup_trim/endswith, get_file_extension) + parse_desktop_file.
 * Operates on a caller-provided DesktopEntry (no shared state). Minimal includes.
 */

#include <stdio.h>
#include "wubu_mime_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void str_lower(char *s) {
    for (; *s; s++) *s = tolower(*s);
}

static void str_trim(char *s) {
    char *end;
    while (isspace(*s)) s++;
    if (!*s) return;
    end = s + strlen(s) - 1;
    while (end > s && isspace(*end)) end--;
    *(end + 1) = '\0';
}

static char *str_dup_trim(const char *s) {
    while (isspace(*s)) s++;
    char *end = (char*)s + strlen(s) - 1;
    while (end > s && isspace(*end)) end--;
    size_t len = end - s + 1;
    char *dup = malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

bool str_endswith(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    if (lf > ls) return false;
    return strcmp(s + ls - lf, suffix) == 0;
}

const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot;
}

/* ============================================================
 * Desktop File Parser (simplified)
 * ============================================================ */
bool parse_desktop_file(const char *path, DesktopEntry *entry) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    memset(entry, 0, sizeof(DesktopEntry));
    
    /* Extract ID from filename */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char *dot = strrchr(base, '.');
    if (dot && strcmp(dot, ".desktop") == 0) {
        size_t len = dot - base;
        if (len < sizeof(entry->id)) {
            memcpy(entry->id, base, len);
            entry->id[len] = '\0';
        }
    }

    char line[1024];
    bool in_desktop_entry = false;
    char current_action[64] = {0};

    while (fgets(line, sizeof(line), f)) {
        str_trim(line);
        if (!*line || line[0] == '#') continue;

        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                in_desktop_entry = (strcmp(line + 1, "Desktop Entry") == 0);
                if (!in_desktop_entry && strncmp(line + 1, "Desktop Action ", 15) == 0) {
                    strncpy(current_action, line + 16, sizeof(current_action) - 1);
                    if (entry->action_count < MAX_DESKTOP_ACTIONS) {
                        strncpy(entry->action_list[entry->action_count].name, current_action, 63);
                    }
                } else {
                    current_action[0] = '\0';
                }
            }
            continue;
        }

        if (!in_desktop_entry && !current_action[0]) continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        str_trim(key);
        str_trim(val);

        if (current_action[0]) {
            int idx = entry->action_count - 1;
            if (idx >= 0 && idx < MAX_DESKTOP_ACTIONS) {
                if (strcmp(key, "Name") == 0) {
                    strncpy(entry->action_list[idx].name, val, 63);
                } else if (strcmp(key, "Exec") == 0) {
                    strncpy(entry->action_list[idx].exec, val, MAX_HANDLER_CMD_LEN - 1);
                }
            }
        } else {
            if (strcmp(key, "Name") == 0) strncpy(entry->name, val, sizeof(entry->name) - 1);
            else if (strcmp(key, "Comment") == 0) strncpy(entry->comment, val, sizeof(entry->comment) - 1);
            else if (strcmp(key, "Exec") == 0) strncpy(entry->exec, val, sizeof(entry->exec) - 1);
            else if (strcmp(key, "Icon") == 0) strncpy(entry->icon, val, sizeof(entry->icon) - 1);
            else if (strcmp(key, "Terminal") == 0) strncpy(entry->terminal, val, sizeof(entry->terminal) - 1);
            else if (strcmp(key, "Type") == 0) strncpy(entry->type, val, sizeof(entry->type) - 1);
            else if (strcmp(key, "Categories") == 0) strncpy(entry->categories, val, sizeof(entry->categories) - 1);
            else if (strcmp(key, "MimeType") == 0) strncpy(entry->mime_types, val, sizeof(entry->mime_types) - 1);
            else if (strcmp(key, "Actions") == 0) strncpy(entry->actions, val, sizeof(entry->actions) - 1);
            else if (strcmp(key, "Hidden") == 0) entry->hidden = (strcmp(val, "true") == 0);
            else if (strcmp(key, "NoDisplay") == 0) entry->no_display = (strcmp(val, "true") == 0);
            else if (strcmp(key, "StartupNotify") == 0) entry->startup_notify = (strcmp(val, "true") == 0);
        }
    }
    fclose(f);

    entry->action_count = (entry->action_count < MAX_DESKTOP_ACTIONS) ? entry->action_count : MAX_DESKTOP_ACTIONS;
    return entry->name[0] != '\0';
}
