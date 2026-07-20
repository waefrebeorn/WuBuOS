/*
 * wubu_ui.c -- AGI UI automation layer (see wubu_ui.h).
 *
 * Every action routes through dosgui_wm_handle_mouse / dosgui_wm_handle_key,
 * the identical entry points a real human's input device feeds. This keeps the
 * input model single and honest: there is no "AGI backdoor" -- the AGI drives
 * the desktop the same way a person would, and the rendered cursor (g_dwm.
 * mouse_x/y, updated inside dosgui_wm_handle_mouse) makes that visible.
 */
#include "wubu_ui.h"
#include "dosgui_wm.h"

#ifdef WUBU_EDR_AGENT
#include "wubu_edr.h"
#endif

#include <string.h>

/* -- Recording ring buffer ----------------------------------------- */
#define WUBU_UI_RB  256
static WubuUiEvent  g_rb[WUBU_UI_RB];
static int          g_rb_head = 0;   /* next write slot */
static int          g_rb_count = 0;
static bool         g_recording = false;

static void record_event(const WubuUiEvent *e) {
    if (!g_recording) return;
    g_rb[g_rb_head] = *e;
    g_rb_head = (g_rb_head + 1) % WUBU_UI_RB;
    if (g_rb_count < WUBU_UI_RB) g_rb_count++;
}

/* -- Mouse ---------------------------------------------------------- */

void wubu_ui_mouse_move(int x, int y) {
    WubuUiEvent e = { .type = WUBU_UI_MOVE, .x = x, .y = y };
    record_event(&e);
    dosgui_wm_handle_mouse(x, y, 0, 0);
#ifdef WUBU_EDR_AGENT
    edr_log_agent_action(EDR_AGENT_MOUSE_MOVE, x, y, 0, 0, NULL);
#endif
}

/* Press/release a button at the cursor's current position. */
static void mouse_btn(int btn, int kind) {
    int mx = 0, my = 0;
    dosgui_wm_get_mouse(&mx, &my);
    WubuUiEvent e = { .type = (kind == 1) ? WUBU_UI_DOWN : WUBU_UI_UP,
                      .x = mx, .y = my, .btn = btn };
    record_event(&e);
    dosgui_wm_handle_mouse(mx, my, btn, kind);
#ifdef WUBU_EDR_AGENT
    edr_log_agent_action(kind == 1 ? EDR_AGENT_MOUSE_DOWN : EDR_AGENT_MOUSE_UP,
                         mx, my, btn, 0, NULL);
#endif
}

void wubu_ui_mouse_down(int btn) { mouse_btn(btn, 1); }
void wubu_ui_mouse_up(int btn)   { mouse_btn(btn, 2); }

void wubu_ui_click(int x, int y, int btn) {
    wubu_ui_mouse_move(x, y);
    wubu_ui_mouse_down(btn);
    wubu_ui_mouse_up(btn);
}

void wubu_ui_drag(int x0, int y0, int x1, int y1, int btn) {
    wubu_ui_mouse_move(x0, y0);
    wubu_ui_mouse_down(btn);
    /* Interpolate so the cursor is visibly dragged (resize/drag track it). */
    int steps = 16;
    for (int i = 1; i <= steps; i++) {
        int cx = x0 + (x1 - x0) * i / steps;
        int cy = y0 + (y1 - y0) * i / steps;
        wubu_ui_mouse_move(cx, cy);
    }
    wubu_ui_mouse_up(btn);
}

/* -- Keyboard ------------------------------------------------------- */

void wubu_ui_key(uint32_t key, uint32_t mods) {
    WubuUiEvent e = { .type = WUBU_UI_KEY, .key = key, .mods = mods };
    record_event(&e);
    dosgui_wm_handle_key(key, mods);
#ifdef WUBU_EDR_AGENT
    edr_log_agent_action(EDR_AGENT_KEY, 0, 0, 0, key, NULL);
#endif
}

void wubu_ui_type(const char *text) {
    if (!text) return;
    for (const char *p = text; *p; p++)
        wubu_ui_key((uint32_t)(unsigned char)*p, 0);
}

/* -- Record / replay ----------------------------------------------- */

void wubu_ui_record(bool on) { g_recording = on; }
int  wubu_ui_recorded_count(void) { return g_rb_count; }
void wubu_ui_record_clear(void) { g_rb_head = 0; g_rb_count = 0; }

void wubu_ui_replay(void) {
    int start = (g_rb_head - g_rb_count + WUBU_UI_RB) % WUBU_UI_RB;
    for (int i = 0; i < g_rb_count; i++) {
        WubuUiEvent *e = &g_rb[(start + i) % WUBU_UI_RB];
        switch (e->type) {
            case WUBU_UI_MOVE: wubu_ui_mouse_move(e->x, e->y); break;
            case WUBU_UI_DOWN: mouse_btn(e->btn, 1); break;
            case WUBU_UI_UP:   mouse_btn(e->btn, 2); break;
            case WUBU_UI_KEY:  wubu_ui_key(e->key, e->mods); break;
        }
    }
}
