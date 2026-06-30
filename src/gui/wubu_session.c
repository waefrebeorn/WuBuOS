/*
 * wubu_session.c  --  WuBuOS Session Manager Implementation
 * Phase 2: Session management, auto-start, shutdown dialog
 */

#include "wubu_session.h"
#include "wubu_settings.h"
#include "dosgui_wm.h"
#include "dosgui_startmenu.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* -- Session State ------------------------------------------------ */

static SessionState g_session = {0};
static AutostartEntry g_autostart[MAX_AUTOSTART_ENTRIES];
static int g_autostart_count = 0;
static bool g_idle_inhibited = false;
static char g_idle_inhibit_reason[128] = {0};

/* -- Config Paths ------------------------------------------------- */

static char g_autostart_path[512] = {0};
static char g_session_path[512] = {0};

static const char *session_autostart_path(void) {
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

static const char *session_file_path(void) {
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

static bool ensure_session_dirs(void) {
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
static bool json_read_bool(const char *str) {
    return strstr(str, "true") != NULL;
}

static int json_read_int(const char *str) {
    return atoi(str);
}

static void json_read_string(const char *src, char *dst, size_t dst_len) {
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

/* -- Session Save/Restore ----------------------------------------- */

static void running_app_to_json(FILE *f, const SessionState *s, int idx, int indent) {
    fprintf(f, "%*s{\n", indent, "");
    fprintf(f, "%*s\"app_name\": \"%s\",\n", indent+2, "", s->running_apps[idx].app_name);
    fprintf(f, "%*s\"window_title\": \"%s\",\n", indent+2, "", s->running_apps[idx].window_title);
    fprintf(f, "%*s\"x\": %d,\n", indent+2, "", s->running_apps[idx].x);
    fprintf(f, "%*s\"y\": %d,\n", indent+2, "", s->running_apps[idx].y);
    fprintf(f, "%*s\"w\": %d,\n", indent+2, "", s->running_apps[idx].w);
    fprintf(f, "%*s\"h\": %d,\n", indent+2, "", s->running_apps[idx].h);
    fprintf(f, "%*s\"desktop\": %d,\n", indent+2, "", s->running_apps[idx].desktop);
    fprintf(f, "%*s\"maximized\": %s,\n", indent+2, "", s->running_apps[idx].maximized ? "true" : "false");
    fprintf(f, "%*s\"minimized\": %s\n", indent+2, "", s->running_apps[idx].minimized ? "true" : "false");
    fprintf(f, "%*s}", indent, "");
}

int wubu_session_save(void) {
    if (!ensure_session_dirs()) return -1;
    
    FILE *f = fopen(session_file_path(), "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"running_apps\": [\n");
    for (int i = 0; i < g_session.running_app_count; i++) {
        running_app_to_json(f, &g_session, i, 4);
        if (i < g_session.running_app_count - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"save_session\": %s\n", g_session.save_session ? "true" : "false");
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

int wubu_session_restore(void) {
    const char *path = session_file_path();
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    
    g_session.running_app_count = 0;
    char *p = buf;
    while ((p = strstr(p, "{\"app_name\"")) && g_session.running_app_count < 32) {
        int idx = g_session.running_app_count;
        json_read_string(strstr(p, "\"app_name\""), g_session.running_apps[idx].app_name, sizeof(g_session.running_apps[idx].app_name));
        json_read_string(strstr(p, "\"window_title\""), g_session.running_apps[idx].window_title, sizeof(g_session.running_apps[idx].window_title));
        
        char *field;
        field = strstr(p, "\"x\""); if (field) g_session.running_apps[idx].x = json_read_int(field);
        field = strstr(p, "\"y\""); if (field) g_session.running_apps[idx].y = json_read_int(field);
        field = strstr(p, "\"w\""); if (field) g_session.running_apps[idx].w = json_read_int(field);
        field = strstr(p, "\"h\""); if (field) g_session.running_apps[idx].h = json_read_int(field);
        field = strstr(p, "\"desktop\""); if (field) g_session.running_apps[idx].desktop = json_read_int(field);
        field = strstr(p, "\"maximized\""); if (field) g_session.running_apps[idx].maximized = json_read_bool(field);
        field = strstr(p, "\"minimized\""); if (field) g_session.running_apps[idx].minimized = json_read_bool(field);
        
        g_session.running_app_count++;
        p = strchr(p, '}');
        if (p) p++;
    }
    
    free(buf);
    return 0;
}

/* -- Session API -------------------------------------------------- */

int wubu_session_init(void) {
    memset(&g_session, 0, sizeof(g_session));
    g_session.save_session = true;
    strncpy(g_session.session_file, session_file_path(), sizeof(g_session.session_file) - 1);
    
    wubu_autostart_load();
    wubu_autostart_run();
    
    if (g_session.save_session) {
        wubu_session_restore();
    }
    
    return 0;
}

void wubu_session_shutdown(void) {
    if (g_session.save_session) {
        wubu_session_save();
    }
}

const SessionState *wubu_session_state(void) { return &g_session; }

void wubu_session_request_action(SessionAction action) {
    g_session.pending_action = action;
    g_session.action_requested = true;
}

void wubu_session_process(void) {
    if (!g_session.action_requested) return;
    
    switch (g_session.pending_action) {
        case SESSION_ACTION_LOGOUT:
            /* Signal to hosted.c to exit GUI mode */
            extern void dosgui_platform_shutdown(void);
            dosgui_platform_shutdown();
            break;
        case SESSION_ACTION_SHUTDOWN:
            /* Full system shutdown */
            extern void dosgui_platform_shutdown(void);
            dosgui_platform_shutdown();
            break;
        case SESSION_ACTION_RESTART:
            /* Reboot - would need system integration */
            extern void dosgui_platform_shutdown(void);
            dosgui_platform_shutdown();
            break;
        case SESSION_ACTION_SUSPEND:
            /* Suspend to RAM */
            {
                pid_t pid = fork();
                if (pid == 0) {
                    execlp("systemctl", "systemctl", "suspend", (char*)NULL);
                    _exit(1);
                }
                waitpid(pid, NULL, 0);
            }
            break;
        case SESSION_ACTION_HIBERNATE:
            {
                pid_t pid = fork();
                if (pid == 0) {
                    execlp("systemctl", "systemctl", "hibernate", (char*)NULL);
                    _exit(1);
                }
                waitpid(pid, NULL, 0);
            }
            break;
        default:
            break;
    }
    g_session.action_requested = false;
}

/* -- Shutdown Dialog ---------------------------------------------- */

static bool g_shutdown_dialog_open = false;
static int g_shutdown_selection = 0;  /* 0=Shutdown, 1=Restart, 2=Logout, 3=Suspend, 4=Cancel */

void wubu_session_show_shutdown_dialog(void) {
    g_shutdown_dialog_open = true;
    g_shutdown_selection = 0;
}

bool wubu_session_shutdown_dialog_visible(void) {
    return g_shutdown_dialog_open;
}

int wubu_session_shutdown_dialog_handle_key(uint32_t key, uint32_t mods) {
    if (!g_shutdown_dialog_open) return 0;
    
    switch (key) {
        case 0xE04B: /* Left */
            if (g_shutdown_selection > 0) g_shutdown_selection--;
            return 1;
        case 0xE04D: /* Right */
            if (g_shutdown_selection < 4) g_shutdown_selection++;
            return 1;
        case 0x1C: /* Enter */
            switch (g_shutdown_selection) {
                case 0: wubu_session_request_action(SESSION_ACTION_SHUTDOWN); break;
                case 1: wubu_session_request_action(SESSION_ACTION_RESTART); break;
                case 2: wubu_session_request_action(SESSION_ACTION_LOGOUT); break;
                case 3: wubu_session_request_action(SESSION_ACTION_SUSPEND); break;
                case 4: break; /* Cancel */
            }
            g_shutdown_dialog_open = false;
            return 1;
        case 0x01: /* Escape */
            g_shutdown_dialog_open = false;
            return 1;
    }
    return 0;
}

void wubu_session_shutdown_dialog_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_shutdown_dialog_open) return;
    
    const WubuThemeColors *tc = wubu_theme_colors();
    const WubuTheme *th = wubu_theme_get();
    
    int dw = 400, dh = 180;
    int dx = (fb_w - dw) / 2;
    int dy = (fb_h - dh) / 2;
    int rad = th->rounded_buttons ? 8 : 0;
    
    /* Shadow */
    vbe_shade_rect(dx + 4, dy + 4, dw, dh);
    
    /* Dialog background */
    vbe_fill_rect_rounded(dx, dy, dw, dh, rad, tc->startmenu_bg);
    vbe_rect_rounded(dx, dy, dw, dh, rad, tc->border_dark);
    
    /* Title */
    vbe_draw_text(dx + 16, dy + 16, "Shut Down", tc->startmenu_text, 1);
    vbe_draw_text(dx + 16, dy + 36, "Choose an action:", tc->startmenu_text, 1);
    
    /* Buttons */
    const char *labels[] = {"Shut Down", "Restart", "Log Out", "Suspend", "Cancel"};
    int bw = 70, bh = 32;
    int bx = dx + 20;
    int by = dy + 70;
    int gap = 10;
    
    for (int i = 0; i < 5; i++) {
        bool sel = (i == g_shutdown_selection);
        int btn_x = bx + i * (bw + gap);
        
        if (th->rounded_buttons) {
            if (sel) {
                vbe_fill_rect_rounded(btn_x, by, bw, bh, 4, tc->select_bg);
                vbe_3d_sunken_rounded_colors(btn_x, by, bw, bh, 4,
                    tc->border_light, tc->border_face, tc->border_dark, tc->border_darkest);
            } else {
                vbe_fill_rect_rounded(btn_x, by, bw, bh, 4, tc->btn_face);
                vbe_3d_raised_rounded_colors(btn_x, by, bw, bh, 4,
                    tc->border_light, tc->border_face, tc->border_dark, tc->border_darkest);
            }
        } else {
            if (sel) {
                vbe_fill_rect(btn_x, by, bw, bh, 0x000080);
                vbe_3d_sunken_colors(btn_x, by, bw, bh,
                                      tc->border_light, tc->border_face,
                                      tc->border_dark, tc->border_darkest);
            } else {
                vbe_fill_rect(btn_x, by, bw, bh, tc->btn_face);
                vbe_3d_raised_colors(btn_x, by, bw, bh,
                                      tc->border_light, tc->border_face,
                                      tc->border_dark, tc->border_darkest);
            }
        }
        
        int tw = vbe_text_width(labels[i], 1);
        vbe_draw_text(btn_x + (bw - tw) / 2, by + (bh - 8) / 2, 
                      labels[i], sel ? tc->select_text : tc->btn_text, 1);
    }
}

/* -- Idle Inhibition ---------------------------------------------- */

void wubu_session_inhibit_idle(const char *reason) {
    g_idle_inhibited = true;
    if (reason) strncpy(g_idle_inhibit_reason, reason, sizeof(g_idle_inhibit_reason) - 1);
}

void wubu_session_uninhibit_idle(void) {
    g_idle_inhibited = false;
    g_idle_inhibit_reason[0] = '\0';
}

bool wubu_session_idle_inhibited(void) { return g_idle_inhibited; }

/* -- Power Management --------------------------------------------- */

int wubu_session_can_suspend(void) {
    /* Check if system supports suspend */
    return access("/sys/power/state", R_OK) == 0;
}

int wubu_session_can_hibernate(void) {
    /* Check if system supports hibernate */
    return access("/sys/power/disk", R_OK) == 0;
}