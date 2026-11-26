/*
 * wm_toast_test.c -- the toast card's geometry and hit-test, without a desktop.
 *
 * A toast that renders but cannot be clicked is decoration, and a clicked toast whose hit-test uses
 * different numbers than its renderer is the bug class this tree keeps paying for (the Explorer
 * address bar tested the top 28 px while it was drawn 36 px lower). Both come from wm_toast_rect(),
 * so that is what this drives: every visible card must be hit-testable at its own centre, no card
 * may be hit outside its rect, and the collapsed fourth notification must not be clickable at all.
 *
 * C11.
 */
#include <stdio.h>
#include <string.h>

#include "../wm.h"
#include "../wubu_tokens.h"
#include "wubu_notify.h"
#include "../wubu_theme.h"
#include "../../kernel/vbe.h"

static int g_pass, g_fail;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) { g_pass++; }                                               \
        else { g_fail++; fprintf(stderr, "  FAIL: %s\n", (msg)); }            \
    } while (0)

/* ---- the two dependencies the module reads --------------------------------- */
static int g_count;
static Notification g_items[8];

int wubu_notify_active_count(void) { return g_count; }

const Notification *wubu_notify_get(int i)
{
    if (i < 0 || i >= g_count) return NULL;
    return &g_items[i];
}

/* The taskbar height is WM state; a fixed height is all the geometry needs. */
int taskbar_height_dynamic(void) { return 48; }

/* ---- drawing stubs -------------------------------------------------------
 * This test asserts GEOMETRY, so the renderer runs with a no-op pen: the functions below cannot
 * disagree with the real ones in a way that hides a bug here, because nothing in the assertions
 * reads a pixel. The one thing they do prove is that the renderer touches no allocation and no WM
 * state it does not declare. */
static const WubuThemeColors *stub_theme(void)
{
    static WubuThemeColors c;
    return &c;
}
const WubuThemeColors *tc(void) { return stub_theme(); }
bool notif_center_is_open(void) { return false; }
void vbe_fill_rect(int x, int y, int w, int h, uint32_t color)
{ (void)x; (void)y; (void)w; (void)h; (void)color; }
void vbe_fill_rect_rounded(int x, int y, int w, int h, int r, uint32_t color)
{ (void)x; (void)y; (void)w; (void)h; (void)r; (void)color; }
void vbe_rect_rounded(int x, int y, int w, int h, int r, uint32_t color)
{ (void)x; (void)y; (void)w; (void)h; (void)r; (void)color; }
void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale)
{ (void)x; (void)y; (void)s; (void)color; (void)scale; }

void wm_toast_rect(int index, int fb_w, int fb_h, int *x, int *y, int *w, int *h);
int  wm_toast_at(int mx, int my, int fb_w, int fb_h);

int main(void)
{
    printf("=== wm_toast_test (the card is where it is drawn, and clicking it lands) ===\n");
    const int fbw = 1600, fbh = 900;

    memset(g_items, 0, sizeof g_items);
    for (int i = 0; i < 8; i++) {
        snprintf(g_items[i].app_name, sizeof g_items[i].app_name, "WuBuOS");
        snprintf(g_items[i].summary, sizeof g_items[i].summary, "notice %d", i);
    }

    /* 1. One card: inside the screen, above the taskbar, hit-testable at its centre. */
    g_count = 1;
    int x, y, w, h;
    wm_toast_rect(0, fbw, fbh, &x, &y, &w, &h);
    CHECK(w > 0 && h > 0, "the card has a size");
    CHECK(x >= 0 && x + w <= fbw, "the card is inside the screen horizontally");
    CHECK(y > 0 && y + h <= fbh - 48, "the card sits above the taskbar");
    CHECK(wm_toast_at(x + w / 2, y + h / 2, fbw, fbh) == 0,
          "the centre of the card hits the card");
    CHECK(wm_toast_at(x - 2, y + h / 2, fbw, fbh) == -1, "just left of the card is not the card");
    CHECK(wm_toast_at(x + w / 2, y - 2, fbw, fbh) == -1, "just above the card is not the card");

    /* 2. Three cards stack upward and each is hit-testable at its own centre. */
    g_count = 3;
    int prev_y = fbh;
    for (int i = 0; i < 3; i++) {
        int cx, cy, cw, ch;
        wm_toast_rect(i, fbw, fbh, &cx, &cy, &cw, &ch);
        CHECK(cy + ch < prev_y, "each card sits above the previous one");
        prev_y = cy;
        CHECK(wm_toast_at(cx + cw / 2, cy + ch / 2, fbw, fbh) == (g_count - 1 - i),
              "a card's centre maps to the notification it is drawing");
    }

    /* 3. The fourth notification collapses into the centre: it is not drawn, so it is not clickable. */
    g_count = 4;
    int cx3, cy3, cw3, ch3;
    wm_toast_rect(3, fbw, fbh, &cx3, &cy3, &cw3, &ch3);
    CHECK(wm_toast_at(cx3 + cw3 / 2, cy3 + ch3 / 2, fbw, fbh) == -1,
          "the collapsed fourth card is not clickable");

    /* 4. Nothing to show, nothing to hit. */
    g_count = 0;
    CHECK(wm_toast_at(fbw - 40, fbh - 200, fbw, fbh) == -1, "with no notifications nothing is hit");

    printf("\n Results: %d/%d passed, %d failed\n", g_pass, g_pass + g_fail, g_fail);
    return g_fail ? 1 : 0;
}
