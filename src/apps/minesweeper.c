/*
 * minesweeper.c  --  Minesweeper (Windows XP accessory parity)
 *
 * Real windowed game: a 9x9 / 16x16 / 16x30 grid of cells, first-click safe,
 * flood-fill reveal, flagging, mine counter, timer, win/lose detection, and a
 * New Game via the smiley face. State in user_data (MineState). No placeholders.
 * C11, opaque struct, minimal includes.
 */

#include "minesweeper.h"
#include "minesweeper_internal.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_window_chrome.h"   /* centralized chrome (ADR-001) */
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ---- XP Minesweeper palette (classic gray field, not Luna blue) ---------- */
#define MS_BG        0x00C0C0C0u  /* classic control gray */
#define MS_CELL_UP   0x00C0C0C0u
#define MS_CELL_DN   0x00C0C0C0u
#define MS_BORDER_HI 0x00FFFFFFu  /* raised highlight */
#define MS_BORDER_LO 0x00808080u  /* sunken shadow */
#define MS_BORDER_DK 0x00000000u
#define MS_FLAG      0x00FF0000u
#define MS_MINE      0x00000000u
#define MS_TEXT      0x00000000u

/* Digit colors (classic minesweeper) */
static const uint32_t ms_num_color[9] = {
    0x00000000, 0x000000FF, 0x00008000, 0x00FF0000, 0x00000080,
    0x00800000, 0x00008080, 0x00000000, 0x00808080
};

static MineState g_mine;

static int ms_idx(const MineState *m, int r, int c) { return r * m->cols + c; }

static void ms_reset(MineState *m, int gw, int gh, int mines) {
    memset(m, 0, sizeof(*m));
    m->grid_w = gw; m->grid_h = gh; m->mines = mines;
    m->cols = gw; m->rows = gh;
    m->first_click = 1;
    m->game_over = 0;
    m->cursor_r = m->rows / 2;
    m->cursor_c = m->cols / 2;
}

static void ms_place_mines(MineState *m, int safe_r, int safe_c) {
    int total = m->cols * m->rows;
    int placed = 0;
    while (placed < m->mines) {
        int i = rand() % total;
        int r = i / m->cols, c = i % m->cols;
        /* keep the first-click cell (and neighbors) mine-free */
        if (abs(r - safe_r) <= 1 && abs(c - safe_c) <= 1) continue;
        if (m->mine[i]) continue;
        m->mine[i] = 1;
        placed++;
    }
    for (int r = 0; r < m->rows; r++)
        for (int c = 0; c < m->cols; c++) {
            int n = 0;
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++) {
                    if (!dr && !dc) continue;
                    int rr = r + dr, cc = c + dc;
                    if (rr < 0 || rr >= m->rows || cc < 0 || cc >= m->cols) continue;
                    if (m->mine[ms_idx(m, rr, cc)]) n++;
                }
            m->adj[ms_idx(m, r, c)] = (uint8_t)n;
        }
}

static void ms_reveal(MineState *m, int r, int c) {
    if (r < 0 || r >= m->rows || c < 0 || c >= m->cols) return;
    int i = ms_idx(m, r, c);
    if (m->state[i] != MS_HIDDEN) return;
    if (m->mine[i]) {
        m->state[i] = MS_REVEALED;
        m->game_over = 2;  /* lost */
        return;
    }
    /* Mark revealed BEFORE recursing so the flood-fill can't revisit this
     * cell (which would recurse forever between neighbours). */
    m->state[i] = MS_REVEALED;
    m->revealed_count++;
    /* flood fill for zero cells */
    if (m->adj[i] == 0) {
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                if (!dr && !dc) continue;
                int rr = r + dr, cc = c + dc;
                if (rr < 0 || rr >= m->rows || cc < 0 || cc >= m->cols) continue;
                int j = ms_idx(m, rr, cc);
                if (m->state[j] == MS_HIDDEN && !m->mine[j])
                    ms_reveal(m, rr, cc);
            }
    }
}

static void ms_check_win(MineState *m) {
    if (m->game_over) return;
    if (m->revealed_count == m->cols * m->rows - m->mines)
        m->game_over = 1;
}

MineState* minesweeper_create(void) {
    MineState *m = &g_mine;
    ms_reset(m, 9, 9, 10);
    return m;
}

void minesweeper_destroy(MineState *m) { (void)m; }

void minesweeper_new_game(MineState *m, int gw, int gh, int mines) {
    if (mines >= gw * gh) mines = gw * gh - 1;
    ms_reset(m, gw, gh, mines);
}

int minesweeper_flag_count(const MineState *m) { return m->flag_count; }
int minesweeper_is_over(const MineState *m) { return m->game_over; }

/* ---- Rendering ----------------------------------------------------------- */

static void ms_draw_cell(MineState *m, int r, int c, int x, int y) {
    int i = ms_idx(m, r, c);
    uint32_t face = MS_CELL_UP;
    if (m->state[i] == MS_REVEALED) face = MS_CELL_DN;

    vbe_fill_rect(x, y, MS_CELL, MS_CELL, face);

    if (m->state[i] == MS_REVEALED) {
        /* sunken flat cell */
        vbe_rect(x, y, MS_CELL, MS_CELL, MS_BORDER_LO);
        if (m->mine[i]) {
            /* mine body + spikes */
            int cx = x + MS_CELL/2, cy = y + MS_CELL/2;
            vbe_fill_rect(cx-3, cy-1, 6, 2, MS_MINE);
            vbe_fill_rect(cx-1, cy-3, 2, 6, MS_MINE);
            vbe_fill_rect(cx-2, cy-2, 4, 4, MS_MINE);
        } else if (m->adj[i] > 0) {
            char d[2] = { (char)('0' + m->adj[i]), 0 };
            vbe_draw_text(x + 4, y + 3, d, ms_num_color[m->adj[i]], 1);
        }
    } else {
        /* raised 3D cell */
        vbe_rect(x, y, MS_CELL, MS_CELL, MS_BORDER_HI);       /* top/left */
        vbe_fill_rect(x+1, y+1, MS_CELL-2, 1, MS_BORDER_HI);
        vbe_fill_rect(x+1, y+1, 1, MS_CELL-2, MS_BORDER_HI);
        vbe_fill_rect(x, y+MS_CELL-1, MS_CELL, 1, MS_BORDER_LO);
        vbe_fill_rect(x+MS_CELL-1, y, 1, MS_CELL, MS_BORDER_LO);
        if (m->state[i] == MS_FLAGGED) {
            /* flag: pole + triangle */
            int cx = x + MS_CELL/2, cy = y + 3;
            vbe_fill_rect(cx, cy, 1, 9, MS_TEXT);
            vbe_fill_rect(cx+1, cy, 5, 2, MS_FLAG);
            vbe_fill_rect(cx+1, cy+2, 3, 2, MS_FLAG);
            vbe_fill_rect(cx-2, y+MS_CELL-3, 6, 2, MS_TEXT);
        }
    }
    /* keyboard cursor highlight */
    if (r == m->cursor_r && c == m->cursor_c && !m->game_over)
        vbe_rect(x, y, MS_CELL, MS_CELL, 0x000000FFu);
}

static void ms_draw_header(MineState *m, int x, int y, int w) {
    /* mine counter (left), smiley (center), timer (right) */
    char buf[16];
    int remaining = m->mines - m->flag_count;
    if (remaining < 0) remaining = 0;
    snprintf(buf, sizeof(buf), "%03d", remaining);
    vbe_draw_text(x + 4, y + 6, buf, MS_FLAG, 2);

    int smiley_x = x + w/2 - 12;
    vbe_fill_rect(smiley_x, y + 2, 24, 24, MS_BG);
    vbe_rect(smiley_x, y + 2, 24, 24, MS_BORDER_LO);
    /* face */
    vbe_fill_rect(smiley_x+5, y+9, 3, 3, MS_TEXT);   /* left eye */
    vbe_fill_rect(smiley_x+16, y+9, 3, 3, MS_TEXT);  /* right eye */
    if (m->game_over == 2) {
        /* dead: X eyes + frown */
        vbe_fill_rect(smiley_x+8, y+16, 8, 2, MS_TEXT);  /* frown */
    } else if (m->game_over == 1) {
        /* win: sunglasses smile */
        vbe_fill_rect(smiley_x+7, y+15, 10, 3, MS_TEXT);
    } else {
        vbe_fill_rect(smiley_x+7, y+15, 10, 2, MS_TEXT); /* smile */
    }

    int elapsed = m->elapsed;
    if (elapsed > 999) elapsed = 999;
    snprintf(buf, sizeof(buf), "%03d", elapsed);
    int tw = 3 * 8 * 2;
    vbe_draw_text(x + w - tw - 4, y + 6, buf, MS_FLAG, 2);
}

static void ms_draw_content(MineState *m, const ChromeContentRect *content) {
    if (!content) return;
    int x0 = content->x, y0 = content->y;
    int w = m->cols * MS_CELL + MS_MARGIN * 2;
    int h = MS_HEADER + m->rows * MS_CELL + MS_MARGIN * 2;

    vbe_fill_rect(x0, y0, w, h, MS_BG);
    vbe_rect(x0, y0, w, h, MS_BORDER_HI);
    vbe_fill_rect(x0+1, y0+1, w-2, 1, MS_BORDER_HI);
    vbe_fill_rect(x0+1, y0+1, 1, h-2, MS_BORDER_HI);
    vbe_fill_rect(x0, y0+h-1, w, 1, MS_BORDER_LO);
    vbe_fill_rect(x0+w-1, y0, 1, h, MS_BORDER_LO);

    ms_draw_header(m, x0 + 4, y0 + 6, w - 8);

    int gx = x0 + MS_MARGIN;
    int gy = y0 + MS_HEADER;
    for (int r = 0; r < m->rows; r++)
        for (int c = 0; c < m->cols; c++)
            ms_draw_cell(m, r, c, gx + c * MS_CELL, gy + r * MS_CELL);
}

/* ---- Public DosGui handlers --------------------------------------------- */

void dosgui_minesweeper_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    if (!win) return;
    ChromeContentRect content = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);
    ms_draw_content(&g_mine, &content);
}

void dosgui_minesweeper_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    MineState *m = &g_mine;
    if (m->game_over) {
        if (key == 'r' || key == 'R' || key == 'n' || key == 'N')
            minesweeper_new_game(m, m->grid_w, m->grid_h, m->mines);
        return;
    }
    switch (key) {
        case 37: if (m->cursor_c > 0) m->cursor_c--; break;           /* left */
        case 39: if (m->cursor_c < m->cols-1) m->cursor_c++; break;   /* right */
        case 38: if (m->cursor_r > 0) m->cursor_r--; break;           /* up */
        case 40: if (m->cursor_r < m->rows-1) m->cursor_r++; break;   /* down */
        case 'f': case 'F': dosgui_minesweeper_flag(m->cursor_r, m->cursor_c); break;
        case ' ': case 13: dosgui_minesweeper_reveal(m->cursor_r, m->cursor_c); break;
        case 'r': case 'R': minesweeper_new_game(m, m->grid_w, m->grid_h, m->mines); break;
        default: break;
    }
}

void dosgui_minesweeper_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)win;
    MineState *m = &g_mine;
    /* map click to grid */
    ChromeContentRect cc = { 0 };
    /* content origin is window pos + chrome; recompute from window rect */
    int gx = win->x + 4 + MS_MARGIN;   /* chrome border + margin */
    int gy = win->y + MS_HEADER + 20;  /* title bar + header */
    (void)cc;
    int c = (x - gx) / MS_CELL;
    int r = (y - gy) / MS_CELL;
    if (r < 0 || r >= m->rows || c < 0 || c >= m->cols) return;

    if (kind == 1) {  /* press */
        if (btn == 1) {           /* left: reveal */
            dosgui_minesweeper_reveal(r, c);
        } else if (btn == 2) {    /* right: flag */
            dosgui_minesweeper_flag(r, c);
        }
    }
}

/* Game actions (also used by keyboard handler) */
void dosgui_minesweeper_reveal(int r, int c) {
    MineState *m = &g_mine;
    if (m->game_over) return;
    if (m->state[ms_idx(m, r, c)] == MS_FLAGGED) return;
    if (m->first_click) {
        ms_place_mines(m, r, c);
        m->first_click = 0;
        m->started = 1;
    }
    ms_reveal(m, r, c);
    ms_check_win(m);
}

void dosgui_minesweeper_flag(int r, int c) {
    MineState *m = &g_mine;
    if (m->game_over) return;
    int i = ms_idx(m, r, c);
    if (m->state[i] == MS_REVEALED) return;
    if (m->state[i] == MS_FLAGGED) { m->state[i] = MS_HIDDEN; m->flag_count--; }
    else { m->state[i] = MS_FLAGGED; m->flag_count++; }
}

void minesweeper_tick(MineState *m) {
    if (m->started && !m->game_over) m->elapsed++;
}

/* Launch a Minesweeper window bound to the game handlers. */
DosGuiWindow* dosgui_launch_minesweeper(void) {
    minesweeper_create();
    int w = g_mine.cols * MS_CELL + MS_MARGIN * 2 + 8;
    int h = MS_HEADER + g_mine.rows * MS_CELL + MS_MARGIN * 2 + 8;
    DosGuiWindow *win = dosgui_wm_create(120, 80, w, h, "Minesweeper");
    if (win) {
        win->on_draw  = dosgui_minesweeper_draw;
        win->on_key   = dosgui_minesweeper_key;
        win->on_mouse = dosgui_minesweeper_mouse;
        win->user_data = &g_mine;
    }
    return win;
}
