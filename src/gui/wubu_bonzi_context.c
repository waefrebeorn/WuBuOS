/*
 * wubu_bonzi_context.c -- the companion's AMBIENT CONTEXT (Track B1).
 *
 * The research (Jarvis 2026, Perplexity Personal Computer, Gemma Wagon)
 * is unambiguous: what separates a desktop agent from a chatbot is that
 * the companion SEES the active window -- "context is the prompt", the
 * user never describes what they're looking at. This module produces that
 * context: on summon, it snapshots the focused window's rect + title plus
 * a structured inventory of the WM's windows, ready to hand to the Brain
 * as the prompt prefix.
 *
 * It is deliberately dependency-light: it does NOT pull in the screenshot
 * renderer or the full WM. The caller feeds it a focused-window descriptor
 * + a window list (from dosgui_wm), and it formats a compact text context.
 * The pixel capture itself (wubu_screenshot_capture(WUBU_SSHOT_WINDOW)) is
 * the caller's job when a vision model is attached; the structured text
 * context is always available and is what a text model reads.
 *
 * C11, minimal includes. Pure formatting + a bounded ring of the last
 * context so the companion can recall what was focused.
 */

#include "wubu_bonzi_context.h"
#include <stdio.h>
#include <string.h>

/* Bounded ring of recent focused-window titles (companion continuity). */
#define CTX_RING 16

static struct {
    char  focus_title[128];
    int   focus_x, focus_y, focus_w, focus_h;
    int   has_focus;
    int   n_windows;
    char  ring[CTX_RING][128];
    int   ring_n;
} g_c;

/* Record the focused window (from dosgui_wm_get_focused()). */
void wubu_bonzi_context_set_focus(const char *title, int x, int y, int w, int h) {
    g_c.has_focus = 1;
    snprintf(g_c.focus_title, sizeof(g_c.focus_title), "%s", title ? title : "");
    g_c.focus_x = x; g_c.focus_y = y; g_c.focus_w = w; g_c.focus_h = h;
    /* push onto the recent-focus ring */
    if (g_c.ring_n < CTX_RING) {
        snprintf(g_c.ring[g_c.ring_n], sizeof(g_c.ring[0]), "%s", g_c.focus_title);
        g_c.ring_n++;
    } else {
        memmove(g_c.ring[0], g_c.ring[1], (CTX_RING - 1) * sizeof(g_c.ring[0]));
        snprintf(g_c.ring[CTX_RING - 1], sizeof(g_c.ring[0]), "%s", g_c.focus_title);
    }
}

/* Record the WM window count (the breadth of what's open). */
void wubu_bonzi_context_set_window_count(int n) { g_c.n_windows = n; }

/* Format the ambient context into buf as the prompt prefix the Brain reads.
 * Returns the number of bytes written (like snprintf). */
int wubu_bonzi_context_format(char *buf, int cap) {
    if (!buf || cap <= 0) return 0;
    int off = 0;
    off += snprintf(buf + off, cap - off,
                    "[ambient] windows=%d\n", g_c.n_windows);
    if (g_c.has_focus) {
        off += snprintf(buf + off, cap - off,
                        "[focus] \"%s\" at %d,%d %dx%d\n",
                        g_c.focus_title, g_c.focus_x, g_c.focus_y,
                        g_c.focus_w, g_c.focus_h);
    } else {
        off += snprintf(buf + off, cap - off, "[focus] none\n");
    }
    return off;
}

/* The recent-focus ring (what the companion was last attending to). */
int wubu_bonzi_context_recent(char out[][128], int max) {
    int n = g_c.ring_n < max ? g_c.ring_n : max;
    for (int i = 0; i < n; i++)
        memcpy(out[i], g_c.ring[i], 128);
    return n;
}
