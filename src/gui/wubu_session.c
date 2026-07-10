/*
 * wubu_session.c  --  WuBuOS Session Manager Implementation
 * Phase 2: Session management, auto-start, shutdown dialog
 */

#include "wubu_session_internal.h"
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
static bool g_idle_inhibited = false;
static char g_idle_inhibit_reason[128] = {0};

/* -- Config Paths ------------------------------------------------- */



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


