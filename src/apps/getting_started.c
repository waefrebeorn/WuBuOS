/*
 * getting_started.c -- the "hello world" path, as an in-shell document.
 *
 * The gap this closes: a person boots WuBuOS, sees a working desktop, and has
 * no idea what to type.  Everything they need is in the tree and in the skills,
 * but not in the PRODUCT.  This window is the product saying where to start.
 *
 * The steps below are the real ones -- HolyD is the in-tree compiler, the
 * terminal and the VWS designer are both reachable from the startmenu, and a
 * .wet applet is Lua.  Nothing here is aspirational.
 */
#include "../gui/dosgui_wm.h"
#include "../gui/wet/wet.h"
#include "../kernel/vbe.h"

#include <stdio.h>
#include <string.h>

/* The document.  Kept as one array so it renders identically everywhere. */
static const char *const LINES[] = {
  "Welcome to WuBuOS",
  "",
  "A from-scratch C11 operating system: its own kernel, shell, compiler and",
  "GUI framework.  Everything you are looking at was built by the OS from",
  "its own source.",
  "",
  "-- Your first program --",
  "",
  "1. Open a terminal:",
  "     Programs > Development > HolyC Terminal",
  "   (or the Terminal icon on the taskbar)",
  "",
  "2. Compile something.  HolyD is the C compiler built into the tree:",
  "     holyd hello.hc",
  "",
  "3. Or write a GUI app in Lua -- a .wet applet:",
  "",
  "     wet.applet {",
  "       title = 'Hello', width = 320, height = 200,",
  "       content = wet.stack { children = {",
  "         wet.label { text = 'Hello from Lua' },",
  "       } }",
  "     }",
  "",
  "   Save it as hello.wet and open it with the .wet applet launcher.",
  "",
  "-- Your first window, designed visually --",
  "",
  "Programs > Development > VWS Designer opens the form designer.  Drag a",
  "control onto the form, click it, edit its properties, pick an event, write",
  "the handler, and press Save.  The designer is written in Lua over .WET --",
  "the same framework it designs for.",
  "",
  "-- Where things live --",
  "",
  "  src/kernel      the kernel",
  "  src/gui/wet     the .WET widget framework",
  "  src/gui/vws     the VWS form designer",
  "  src/gui/wua     the Lua interpreter (Wua)",
  "  src/compiler    HolyD / HolyC",
  "  src/apps        the built-in applications",
  "",
  "Press Esc to close.",
  NULL
};

#define GS_LINE_H 14
#define GS_PAD    12
#define GS_MAX_W  620

static int g_scroll;
static int g_lines;

static void gs_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    ChromeContentRect c = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);
    if (!win) return;

    /* Clip to the content rect by drawing line by line and skipping what falls
     * outside -- the VBE has no text clipping, so we bound it ourselves. */
    int max_lines = (c.h - GS_PAD * 2) / GS_LINE_H;
    if (max_lines < 1) max_lines = 1;

    for (int i = 0; i < g_lines; i++) {
        int row = i - g_scroll;
        if (row < 0 || row >= max_lines) continue;
        int y = c.y + GS_PAD + row * GS_LINE_H;
        if (y < c.y || y + GS_LINE_H > c.y + c.h) continue;
        /* Title lines (0 and the -- -- headers) get the bright colour. */
        bool header = (i == 0) ||
                      (LINES[i][0] == '-' && LINES[i][1] == '-');
        vbe_draw_text(c.x + GS_PAD, y, LINES[i],
                      header ? 0x000080 : 0x000000, 1);
    }

    /* A scrollbar, so the document reads as scrollable. */
    if (g_lines > max_lines) {
        int track_h = c.h - GS_PAD * 2;
        int thumb_h = track_h * max_lines / g_lines;
        if (thumb_h < 12) thumb_h = 12;
        int thumb_y = c.y + GS_PAD + (track_h - thumb_h) * g_scroll / (g_lines - max_lines);
        int sx = c.x + c.w - 8;
        vbe_fill_rect(sx, c.y + GS_PAD, 4, track_h, 0xC0C0C0);
        vbe_fill_rect(sx, thumb_y, 4, thumb_h, 0x808080);
    }
}

static void gs_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    int max_lines = (win->h - GS_PAD * 2) / GS_LINE_H;
    int max_scroll = g_lines - max_lines;
    if (max_scroll < 0) max_scroll = 0;

    switch (key) {
    case 30: if (g_scroll > 0) g_scroll--; break;             /* up   */
    case 31: if (g_scroll < max_scroll) g_scroll++; break;    /* down */
    case 27: dosgui_wm_destroy(win); break;                   /* esc  */
    default: break;
    }
}

static void gs_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)x; (void)btn;
    if (kind != 1) return;
    int max_lines = (win->h - GS_PAD * 2) / GS_LINE_H;
    int max_scroll = g_lines - max_lines;
    if (max_scroll < 0) max_scroll = 0;
    /* Wheel-free: a press in the lower third scrolls down, upper third up. */
    int cy = win->y + 20;
    if (y < cy + win->h / 3) { if (g_scroll > 0) g_scroll--; }
    else if (y > cy + win->h * 2 / 3) { if (g_scroll < max_scroll) g_scroll++; }
}

DosGuiWindow *gs_launch(void) {
    if (g_lines == 0) {
        for (g_lines = 0; LINES[g_lines]; g_lines++) ;
    }
    g_scroll = 0;

    DosGuiWindow *win = dosgui_wm_create(90, 70, GS_MAX_W, 460,
                                         "Getting Started - WuBuOS");
    if (!win) return NULL;
    win->on_draw  = gs_draw;
    win->on_key   = gs_key;
    win->on_mouse = gs_mouse;
    return win;
}
