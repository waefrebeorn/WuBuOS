/*
 * wubu_petdex.c -- the petdex, as a window.
 *
 * WuBuOS's pets are deterministic pixel art (tools/pet_gen.py): a locked
 * palette, a one-pixel outline, a two-step shading ramp, four animation
 * frames. They are baked into an RGBA table (wubu_petdex.h) because the WM
 * has no image decoder -- the same discipline as the mascot.
 *
 * This window shows every pet, animates them, and lets the user pick one.
 * The pick lands in the agent namespace (~/.wubu/n/agent/pet) so the agent
 * channel can read it -- the pet is a shared thing, not a per-window one.
 *
 * C11, opaque API, no god headers.
 */
#include "../gui/wm.h"
#include "../gui/shell/window_chrome.h"
#include "../gui/wubu_petdex.h"
#include "../kernel/vbe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PD_COLS      4
#define PD_CELL_W    120
#define PD_CELL_H    96
#define PD_PAD       16
#define PD_SCALE     3          /* 24px art shown at 72px */

static ChromeContentRect g_rect;
static int g_selected;
static int g_frame;
static int g_last_frame_ms;

static void pd_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    ChromeContentRect c = chrome_draw_window(win, fb, fb_w, fb_h);
    if (!win) return;
    g_rect = c;

    int cols = PD_COLS;
    int rows = (WUBU_PET_COUNT + cols - 1) / cols;

    /* Advance the animation about four times a second. */
    int now = (int)(time(NULL) * 1000);
    if (now - g_last_frame_ms > 250) {
        g_frame = (g_frame + 1) % WUBU_PET_FRAMES;
        g_last_frame_ms = now;
    }

    vbe_draw_text(c.x + PD_PAD, c.y + 10, "WuBu Petdex",
                  0x000080, 2);
    vbe_draw_text(c.x + PD_PAD, c.y + 34,
                  "Every pet is deterministic pixel art: same recipe, same pixels.",
                  0x404040, 1);

    int grid_y = c.y + 58;
    for (int i = 0; i < WUBU_PET_COUNT; i++) {
        int col = i % cols;
        int row = i / cols;
        int cx = c.x + PD_PAD + col * PD_CELL_W;
        int cy = grid_y + row * PD_CELL_H;

        const WubuPet *p = &wubu_petdex[i];
        const unsigned char *rgba =
            p->rgba + (size_t)g_frame * WUBU_PET_SIZE * WUBU_PET_SIZE * 4;

        /* Selected pet gets a plate. */
        if (i == g_selected) {
            vbe_fill_rect(cx - 4, cy - 4, PD_CELL_W - 8, PD_CELL_H - 20,
                          0xE8F0FF);
            vbe_rect_rounded(cx - 4, cy - 4, PD_CELL_W - 8, PD_CELL_H - 20,
                             4, 0x000080);
        }

        /* The pet, scaled nearest-neighbour so the pixels stay square. */
        int ax = cx + (PD_CELL_W - WUBU_PET_SIZE * PD_SCALE) / 2;
        int ay = cy + 6;
        for (int y = 0; y < WUBU_PET_SIZE; y++) {
            for (int x = 0; x < WUBU_PET_SIZE; x++) {
                const unsigned char *px = rgba + (y * WUBU_PET_SIZE + x) * 4;
                if (px[3] == 0) continue;
                uint32_t col32 = 0xFF000000u |
                                 ((uint32_t)px[0] << 16) |
                                 ((uint32_t)px[1] << 8) |
                                 (uint32_t)px[2];
                vbe_fill_rect(ax + x * PD_SCALE, ay + y * PD_SCALE,
                              PD_SCALE, PD_SCALE, col32);
            }
        }

        vbe_draw_text(cx + PD_CELL_W / 2 - (int)strlen(p->name) * 4,
                      ay + WUBU_PET_SIZE * PD_SCALE + 8,
                      p->name, 0x000000, 1);
    }

    int rows_used = rows;
    int hint_y = grid_y + rows_used * PD_CELL_H + 6;
    vbe_draw_text(c.x + PD_PAD, hint_y,
                  "Click a pet to choose it. Esc closes.",
                  0x404040, 1);
}

static void pd_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    switch (key) {
    case 27: wm_destroy(win); break;                       /* esc */
    case 30: g_selected = (g_selected + WUBU_PET_COUNT - 1) % WUBU_PET_COUNT; break;
    case 31: g_selected = (g_selected + 1) % WUBU_PET_COUNT; break;
    default: break;
    }
}

static void pd_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    if (kind != 1) return;
    ChromeContentRect c = g_rect;
    int cols = PD_COLS;
    int grid_y = c.y + 58;
    int col = (x - c.x - PD_PAD) / PD_CELL_W;
    int row = (y - grid_y) / PD_CELL_H;
    if (col < 0 || col >= cols || row < 0) return;
    int idx = row * cols + col;
    if (idx >= 0 && idx < WUBU_PET_COUNT) g_selected = idx;
}

DosGuiWindow *wubu_petdex_open(void) {
    int w = PD_PAD * 2 + PD_COLS * PD_CELL_W;
    int h = 58 + ((WUBU_PET_COUNT + PD_COLS - 1) / PD_COLS) * PD_CELL_H + 34;
    g_selected = 0;
    g_frame = 0;

    DosGuiWindow *win = wm_create(120, 60, w, h, "WuBu Petdex");
    if (!win) return NULL;
    win->on_draw = pd_draw;
    win->on_key = pd_key;
    win->on_mouse = pd_mouse;
    return win;
}
