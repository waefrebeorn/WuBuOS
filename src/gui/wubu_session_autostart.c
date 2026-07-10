/* wubu_session_autostart.c -- Autostart entries subsystem.
 *
 * Self-contained: owns g_autostart[]/g_autostart_count, parses .desktop-style
 * autostart files, saves/loads/restores them. The json_read_* helpers are shared
 * with wubu_session.c's session-restore (declared in wubu_session_internal.h).
 * Minimal includes.
 */

#include "wubu_session_internal.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* -- Config path buffers (module-private) ------------------------- */
static char g_autostart_path[512] = {0};
static char g_session_path[512] = {0};

static AutostartEntry g_autostart[MAX_AUTOSTART_ENTRIES];
static int g_autostart_count = 0;

const char *session_autostart_path(void) {
    if (g_autostart_path[0] == '\0') {
        const char *home = getenv("HOME");
        const char *xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && xdg[0]) {
            snprintf(g_autostart_path, sizeof(g_autostart_path), "%s/wibu/autostart", xdg);
        } else if (home) {
            snprintf(g_autostart_path, sizeof(g_autostart_path), "%s/.config/wibu/autostart", home);
        } else {
            strcpy(g_autostart_path, "/tmp/wibu/autostart");
        }
    }
    return g_autostart_path;
}

const char *session_file_path(void) {
    if (g_session_path[0] == '\0') {
        const char *home = getenv("HOME");
        const char *xdg = getenv("XDG_STATE_HOME");
        if (xdg && xdg[0]) {
            snprintf(g_session_path, sizeof(g_session_path), "%s/wibu/session.json", xdg);
        } else if (home) {
            snprintf(g_session_path, sizeof(g_session_path), "%s/.local/state/wibu/session.json", home);
        } else {
            strcpy(g_session_path, "/tmp/wibu/session.json");
        }
    }
    return g_session_path;
}

bool ensure_session_dirs(void) {
    char dir[512];
    const char *p1 = session_autostart_path();
    const char *p2 = session_file_path();
    
    strncpy(dir, p1, sizeof(dir) - 1);
    char *last = strrchr(dir, '/');
    if (last) { *last = '\0'; mkdir(dir, 0755); }
    
    strncpy(dir, p2, sizeof(dir) - 1);
    last = strrchr(dir, '/');
    if (last) { *last = '\0'; mkdir(dir, 0755); }
    
    return true;
}

/* -- Default Auto-start Entries ----------------------------------- */

static void session_default_autostart(void) {
    g_autostart_count = 0;
    
    /* Settings daemon */
    if (g_autostart_count < MAX_AUTOSTART_ENTRIES) {
        AutostartEntry *e = &g_autostart[g_autostart_count++];
        strcpy(e->name, "Settings Daemon");
        strcpy(e->exec, "wubu-settings-daemon");
        e->args[0] = '\0';
        e->enabled = true;
        e->terminal = false;
        e->order = 10;
        strcpy(e->only_show_in, "WuBuOS");
        e->not_show_in[0] = '\0';
        e->hidden = true;
    }
    
    /* Notification daemon */
    if (g_autostart_count < MAX_AUTOSTART_ENTRIES) {
        AutostartEntry *e = &g_autostart[g_autostart_count++];
        strcpy(e->name, "Notification Daemon");
        strcpy(e->exec, "wubu-notify-daemon");
        e->args[0] = '\0';
        e->enabled = true;
        e->terminal = false;
        e->order = 20;
        strcpy(e->only_show_in, "WuBuOS");
        e->not_show_in[0] = '\0';
        e->hidden = true;
    }
    
    /* Clipboard manager */
    if (g_autostart_count < MAX_AUTOSTART_ENTRIES) {
        AutostartEntry *e = &g_autostart[g_autostart_count++];
        strcpy(e->name, "Clipboard Manager");
        strcpy(e->exec, "wubu-clipboard");
        e->args[0] = '\0';
        e->enabled = true;
        e->terminal = false;
        e->order = 30;
        strcpy(e->only_show_in, "WuBuOS");
        e->not_show_in[0] = '\0';
        e->hidden = true;
    }
}

/* -- Auto-start Load/Save ----------------------------------------- */

static void autostart_entry_to_json(FILE *f, const AutostartEntry *e, int indent) {
    fprintf(f, "%*s{\n", indent, "");
    fprintf(f, "%*s\"name\": \"%s\",\n", indent+2, "", e->name);
    fprintf(f, "%*s\"exec\": \"%s\",\n", indent+2, "", e->exec);
    fprintf(f, "%*s\"args\": \"%s\",\n", indent+2, "", e->args);
    fprintf(f, "%*s\"enabled\": %s,\n", indent+2, "", e->enabled ? "true" : "false");
    fprintf(f, "%*s\"terminal\": %s,\n", indent+2, "", e->terminal ? "true" : "false");
    fprintf(f, "%*s\"order\": %d,\n", indent+2, "", e->order);
    fprintf(f, "%*s\"only_show_in\": \"%s\",\n", indent+2, "", e->only_show_in);
    fprintf(f, "%*s\"not_show_in\": \"%s\",\n", indent+2, "", e->not_show_in);
    fprintf(f, "%*s\"hidden\": %s\n", indent+2, "", e->hidden ? "true" : "false");
    fprintf(f, "%*s}", indent, "");
}

int wubu_autostart_save(void) {
    if (!ensure_session_dirs()) return -1;
    
    FILE *f = fopen(session_autostart_path(), "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"entries\": [\n");
    for (int i = 0; i < g_autostart_count; i++) {
        autostart_entry_to_json(f, &g_autostart[i], 4);
        if (i < g_autostart_count - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

/* Simple JSON parsing for autostart - minimal implementation */
bool json_read_bool(const char *str) {
    return strstr(str, "true") != NULL;
}

int json_read_int(const char *str) {
    return atoi(str);
}

void json_read_string(const char *src, char *dst, size_t dst_len) {
    const char *start = strchr(src, '"');
    if (!start) { dst[0] = '\0'; return; }
    start++;
    const char *end = strchr(start, '"');
    if (!end) { dst[0] = '\0'; return; }
    size_t len = end - start;
    if (len >= dst_len) len = dst_len - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
}

int wubu_autostart_load(void) {
    const char *path = session_autostart_path();
    FILE *f = fopen(path, "r");
    if (!f) {
        session_default_autostart();
        wubu_autostart_save();
        return 0;
    }
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    
    /* Very simple parsing - find each entry object */
    g_autostart_count = 0;
    char *p = buf;
    while ((p = strstr(p, "{\"name\"")) && g_autostart_count < MAX_AUTOSTART_ENTRIES) {
        AutostartEntry *e = &g_autostart[g_autostart_count];
        memset(e, 0, sizeof(*e));
        
        char *field;
        field = strstr(p, "\"name\""); if (field) json_read_string(field, e->name, sizeof(e->name));
        field = strstr(p, "\"exec\""); if (field) json_read_string(field, e->exec, sizeof(e->exec));
        field = strstr(p, "\"args\""); if (field) json_read_string(field, e->args, sizeof(e->args));
        field = strstr(p, "\"enabled\""); if (field) e->enabled = json_read_bool(field);
        field = strstr(p, "\"terminal\""); if (field) e->terminal = json_read_bool(field);
        field = strstr(p, "\"order\""); if (field) e->order = json_read_int(field);
        field = strstr(p, "\"only_show_in\""); if (field) json_read_string(field, e->only_show_in, sizeof(e->only_show_in));
        field = strstr(p, "\"not_show_in\""); if (field) json_read_string(field, e->not_show_in, sizeof(e->not_show_in));
        field = strstr(p, "\"hidden\""); if (field) e->hidden = json_read_bool(field);
        
        g_autostart_count++;
        p = strchr(p, '}');
        if (p) p++;
    }
    
    free(buf);
    return 0;
}

/* -- Auto-start API ----------------------------------------------- */

int wubu_autostart_add(const AutostartEntry *entry) {
    if (g_autostart_count >= MAX_AUTOSTART_ENTRIES) return -1;
    
    /* Check for existing */
    for (int i = 0; i < g_autostart_count; i++) {
        if (strcmp(g_autostart[i].name, entry->name) == 0) {
            g_autostart[i] = *entry;
            return 0;
        }
    }
    
    g_autostart[g_autostart_count++] = *entry;
    /* Sort by order */
    for (int i = g_autostart_count - 1; i > 0; i--) {
        if (g_autostart[i].order < g_autostart[i-1].order) {
            AutostartEntry tmp = g_autostart[i];
            g_autostart[i] = g_autostart[i-1];
            g_autostart[i-1] = tmp;
        } else break;
    }
    return 0;
}

int wubu_autostart_remove(const char *name) {
    for (int i = 0; i < g_autostart_count; i++) {
        if (strcmp(g_autostart[i].name, name) == 0) {
            for (int j = i; j < g_autostart_count - 1; j++)
                g_autostart[j] = g_autostart[j+1];
            g_autostart_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_autostart_count(void) { return g_autostart_count; }

const AutostartEntry *wubu_autostart_get(int index) {
    if (index < 0 || index >= g_autostart_count) return NULL;
    return &g_autostart[index];
}

void wubu_autostart_run(void) {
    for (int i = 0; i < g_autostart_count; i++) {
        if (!g_autostart[i].enabled) continue;
        
        pid_t pid = fork();
        if (pid == 0) {
            /* Child */
            char *argv[4];
            argv[0] = "/bin/sh";
            argv[1] = "-c";
            argv[2] = g_autostart[i].exec;
            argv[3] = NULL;
            if (g_autostart[i].args[0]) {
                static char cmd[512];
                snprintf(cmd, sizeof(cmd), "%s %s", g_autostart[i].exec, g_autostart[i].args);
                argv[2] = cmd;
            }
            execv("/bin/sh", argv);
            _exit(1);
        } else if (pid > 0) {
            /* Parent - don't wait, let it run */
        }
    }
}
