/*
 * wubu_ui_test.c -- AGI UI automation layer test suite.
 *
 * Proves the central AGI-OS claim: a synthetic driver can manipulate the
 * desktop through the SAME input path a human uses -- move windows, close
 * them, resize them, and type into them -- and that every action is recorded
 * so a session can be replayed and watched. If these pass, "watch the AGI
 * operate the computer" is real, not staged.
 */
#include "wubu_ui.h"
#include "wm.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"   /* MOD_ALT */
#include "../gui/wubu_theme.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static int g_run = 0, g_pass = 0;
#define T(cond, msg) do { g_run++; if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
                         else { printf("  ❌ %s\n", msg); } } while (0)

/* on_key capture buffer for the focused window. */
static char g_typed[64];
static void on_key(DosGuiWindow *w, uint32_t key, uint32_t mods) {
    (void)w; (void)mods;
    size_t n = strlen(g_typed);
    if (n < sizeof(g_typed) - 1) { g_typed[n] = (char)(unsigned char)key; g_typed[n+1] = 0; }
}

static DosGuiWindow *spawn_at(int x, int y, int w, int h, const char *title) {
    DosGuiWindow *win = wm_create(x, y, w, h, title);
    if (win) { win->on_key = on_key; wm_set_focus(win); }
    return win;
}

int main(void) {
    printf("=== WuBuOS AGI UI Automation Test Suite ===\n\n");
    vbe_init(1024, 768);
    wm_init(1024, 768);

    /* -- 1. Move a window by dragging its title bar -- */
    printf("[Move window via synthetic drag]\n");
    DosGuiWindow *a = spawn_at(100, 100, 200, 150, "Notes");
    int ax0 = a->x, ay0 = a->y;
    /* Grab the title bar LEFT of the chrome close/max/min buttons: on a
     * 200px window the buttons (laid out from the right, ~38px each) start
     * around x+182, so the centre (x+w/2) IS the minimize button and a drag
     * there minimizes the window instead of moving it. */
    wubu_ui_drag(a->x + 30, a->y + 8, a->x + 30 + 60, a->y + 8 + 40, 1);
    T(a->x == ax0 + 60 && a->y == ay0 + 40, "window moved to dragged position");

    /* -- 2. Resize a window from its right edge -- */
    printf("\n[Resize window via synthetic edge drag]\n");
    DosGuiWindow *b = spawn_at(300, 300, 200, 150, "ResizeMe");
    int bw0 = b->w;
    /* Grab 2px inside the right border, mid-height, drag +80px right. */
    int gx = b->x + b->w - 2, gy = b->y + b->h/2;
    wubu_ui_drag(gx, gy, gx + 80, gy, 1);
    T(b->w == bw0 + 80, "window resized wider from right edge");
    T(b->x == 300, "right-edge resize keeps left edge fixed");

    /* -- 3. Close a window with the keyboard (Alt+F4-style global close) -- */
    printf("\n[Close window via synthetic key]\n");
    DosGuiWindow *c = spawn_at(500, 200, 180, 120, "Bye");
    wubu_ui_key(0x3E, MOD_ALT);  /* Alt+F4 -> close focused */
    /* Closing is ANIMATED now (wm_request_close starts a shrink; wm_close_tick finishes it after
     * WUBU_ANIM_OPEN_MS). Asserting !alive straight away was asserting the pre-animation contract,
     * so this gate failed against correct behaviour. The contract is: the key STARTS a close, and
     * the window is gone once the animation's time is up. */
    T(c->close_t0 != 0 || !c->alive, "Alt+F4 starts the close animation");
    for (int frame = 0; frame < 200 && c->alive; frame++) {
        struct timespec ts = { 0, 5 * 1000 * 1000 };   /* 5ms, 1s ceiling */
        nanosleep(&ts, NULL);
        wm_close_tick();
    }
    T(!c->alive, "window closed via synthetic key event");

    /* -- 4. Type into the focused window -- */
    printf("\n[Type text into focused window]\n");
    DosGuiWindow *d = spawn_at(150, 400, 220, 140, "Editor");
    g_typed[0] = 0;
    wubu_ui_type("Hi");
    T(strcmp(g_typed, "Hi") == 0, "typed text delivered to focused window on_key");

    /* -- 5. Recording + replay -- */
    printf("\n[Record + replay session]\n");
    wubu_ui_record_clear();
    wubu_ui_record(true);
    DosGuiWindow *e = spawn_at(600, 500, 160, 100, "Rec");
    int ex0 = e->x;
    /* Same as the move test: grab LEFT of the chrome buttons (x+30), not the
     * centre which is the minimize button on a 160px window. */
    wubu_ui_drag(e->x + 30, e->y + 6, e->x + 30 + 25, e->y + 6, 1);
    wubu_ui_record(false);
    int recorded = wubu_ui_recorded_count();
    T(recorded > 0, "UI actions were recorded");
    /* Replay should reproduce the same move: reset, then replay the buffer. */
    int moved = e->x - ex0;                 /* delta the recorded drag produced */
    wubu_ui_drag(e->x + 30, e->y + 6, e->x + 30 - 25, e->y + 6, 1); /* move back */
    int before_replay = e->x - ex0;
    wubu_ui_replay();
    T(e->x - ex0 == before_replay + moved, "replay reproduces the recorded drag");

    wm_shutdown();
    vbe_shutdown();
    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
