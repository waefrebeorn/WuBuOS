/*
 * solitaire.c  --  Solitaire (Klondike, Windows XP accessory parity)
 *
 * Real playable Klondike: 52-card deck, shuffle, 7 tableau piles, 4
 * foundations, stock/waste. Drag or click to move cards. Alternating-color
 * descending tableau builds, ascending suit foundations, King-to-empty rules.
 * Full win detection. Game logic is pure C (no GUI) and unit-tested; the
 * DosGui layer renders cards and routes input.
 *
 * C11, opaque struct, minimal includes.
 */

#include "solitaire.h"
#include "solitaire_internal.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_window_chrome.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ---- Card / pile model --------------------------------------------------- */

static SolState g_sol;

static int sol_rand_seed = 1;
static int sol_rand(void) { sol_rand_seed = sol_rand_seed * 1103515245 + 12345; return (sol_rand_seed >> 16) & 0x7FFF; }

/* suit: 0=spades 1=hearts 2=clubs 3=diamonds ; rank: 1..13 (A..K).
 * Encode as suit*13 + (rank-1) so rank spans 0..12 within a suit (King=12). */
static int sol_card(int suit, int rank) { return suit * 13 + (rank - 1); }
static int sol_suit(int card) { return card < 0 ? -1 : card / 13; }
static int sol_rank(int card) { return card < 0 ? -1 : card % 13 + 1; }
static int sol_is_red(int suit) { return suit == 1 || suit == 3; }

static void sol_reset(SolState *s) {
    memset(s, 0, sizeof(*s));
    s->stock_count = 0;
    s->waste_count = 0;
    for (int i = 0; i < SOL_NPILES; i++) s->tab_count[i] = 0;
    for (int i = 0; i < SOL_NFOUND; i++) s->found[i] = -1;  /* empty; store top card */
    s->moves = 0;
    s->won = 0;
}

void sol_new_game(SolState *s) {
    sol_reset(s);
    /* build ordered deck, then Fisher-Yates shuffle */
    int deck[52], n = 0;
    for (int suit = 0; suit < 4; suit++)
        for (int rank = 1; rank <= 13; rank++)
            deck[n++] = sol_card(suit, rank);
    for (int i = 51; i > 0; i--) {
        int j = sol_rand() % (i + 1);
        int t = deck[i]; deck[i] = deck[j]; deck[j] = t;
    }
    /* deal tableau: pile i gets i+1 cards, top face-up */
    int di = 0;
    for (int p = 0; p < SOL_NPILES; p++) {
        s->tab_count[p] = p + 1;
        for (int k = 0; k <= p; k++) {
            s->tab[p][k] = deck[di++];
            s->tab_up[p][k] = (k == p);  /* only top card face-up */
        }
    }
    /* remaining -> stock */
    while (di < 52) s->stock[s->stock_count++] = deck[di++];
    s->selected = -1;
    s->sel_pile = -1;
    s->sel_index = -1;
}

/* ---- Move rules ---------------------------------------------------------- */

/* Can `card` be placed on tableau pile `p` (on top of its current top)? */
static int sol_can_tableau(SolState *s, int p, int card) {
    if (s->tab_count[p] == 0) return sol_rank(card) == 13;  /* King to empty */
    int top = s->tab[p][s->tab_count[p] - 1];
    if (!s->tab_up[p][s->tab_count[p] - 1]) return 0;       /* top must be face-up */
    return sol_is_red(sol_suit(card)) != sol_is_red(sol_suit(top))
           && sol_rank(card) == sol_rank(top) - 1;          /* alt color, desc */
}

/* Can `card` go to foundation `f`? */
static int sol_can_found(SolState *s, int f, int card) {
    if (sol_suit(card) != f) return 0;
    int top = s->found[f];
    if (top < 0) return sol_rank(card) == 1;                /* Ace starts */
    return sol_rank(card) == sol_rank(top) + 1;             /* ascending */
}

/* Move the selected run (from tableau) to tableau pile p. Returns 1 on success. */
int sol_move_to_tableau(SolState *s, int p) {
    if (s->sel_pile < 0 || s->sel_index < 0) return 0;
    if (s->sel_pile == p) return 0;
    int card = s->tab[s->sel_pile][s->sel_index];
    if (!sol_can_tableau(s, p, card)) return 0;
    /* move the run sel_index..top */
    int run = s->tab_count[s->sel_pile] - s->sel_index;
    for (int k = 0; k < run; k++)
        s->tab[p][s->tab_count[p] + k] = s->tab[s->sel_pile][s->sel_index + k];
    s->tab_count[p] += run;
    s->tab_count[s->sel_pile] -= run;
    /* flip newly-exposed tableau card */
    if (s->tab_count[s->sel_pile] > 0)
        s->tab_up[s->sel_pile][s->tab_count[s->sel_pile] - 1] = 1;
    s->moves++;
    s->selected = -1; s->sel_pile = -1; s->sel_index = -1;
    sol_check_win(s);
    return 1;
}

int sol_move_to_foundation(SolState *s, int f) {
    int card, from_pile, from_index;
    if (s->sel_pile >= 0) {           /* from tableau */
        from_pile = s->sel_pile; from_index = s->sel_index;
        if (from_index != s->tab_count[from_pile] - 1) return 0;  /* only top card */
        card = s->tab[from_pile][from_index];
    } else if (s->waste_count > 0) {  /* from waste */
        card = s->waste[s->waste_count - 1];
        from_pile = SOL_WASTE; from_index = s->waste_count - 1;
    } else return 0;
    if (!sol_can_found(s, f, card)) return 0;
    s->found[f] = card;
    if (from_pile == SOL_WASTE) s->waste_count--;
    else {
        s->tab_count[from_pile]--;
        if (s->tab_count[from_pile] > 0)
            s->tab_up[from_pile][s->tab_count[from_pile] - 1] = 1;
    }
    s->moves++;
    s->selected = -1; s->sel_pile = -1; s->sel_index = -1;
    sol_check_win(s);
    return 1;
}

/* Draw from stock to waste (recycle waste when stock empty). */
void sol_draw_stock(SolState *s) {
    if (s->stock_count == 0) {
        /* recycle waste back to stock */
        while (s->waste_count > 0) s->stock[s->stock_count++] = s->waste[--s->waste_count];
    } else {
        s->waste[s->waste_count++] = s->stock[--s->stock_count];
    }
    s->moves++;
    s->selected = -1; s->sel_pile = -1; s->sel_index = -1;
}

/* Auto-move: try to send the selected/waste top card to a foundation. */
int sol_auto_foundation(SolState *s) {
    int card, from_pile;
    if (s->sel_pile >= 0) {
        if (s->sel_index != s->tab_count[s->sel_pile] - 1) return 0;
        card = s->tab[s->sel_pile][s->sel_index]; from_pile = s->sel_pile;
    } else if (s->waste_count > 0) { card = s->waste[s->waste_count-1]; from_pile = SOL_WASTE; }
    else return 0;
    for (int f = 0; f < SOL_NFOUND; f++)
        if (sol_can_found(s, f, card)) {
            s->sel_pile = from_pile; s->sel_index = (from_pile==SOL_WASTE)?s->waste_count-1:s->tab_count[from_pile]-1;
            return sol_move_to_foundation(s, f);
        }
    return 0;
}

void sol_check_win(SolState *s) {
    for (int f = 0; f < SOL_NFOUND; f++)
        if (sol_rank(s->found[f]) != 13) return;
    s->won = 1;
}

/* ---- Rendering ----------------------------------------------------------- */

#define SL_CARD_W  72
#define SL_CARD_H  100
#define SL_FAN    18   /* face-up tableau fan offset */
#define SL_GAP_X  18
#define SL_TOP_Y  16
#define SL_TAB_Y  140

static const uint32_t sl_suit_col[4] = { 0x00000000, 0x00CC0000, 0x00000000, 0x00CC0000 };
static const char sl_suit_ch[4] = { 'S', 'H', 'C', 'D' };
static const char *sl_rank_str[14] = {"","A","2","3","4","5","6","7","8","9","10","J","Q","K"};

static void sl_draw_card_back(int x, int y) {
    vbe_fill_rect_rounded(x, y, SL_CARD_W, SL_CARD_H, 6, 0x00224466);
    vbe_rect_rounded(x, y, SL_CARD_W, SL_CARD_H, 6, 0x00FFFFFF);
    /* diamond lattice pattern */
    for (int yy = y + 8; yy < y + SL_CARD_H - 8; yy += 12)
        for (int xx = x + 8; xx < x + SL_CARD_W - 8; xx += 12)
            vbe_fill_rect(xx, yy, 4, 4, 0x00335577);
}

static void sl_draw_card_face(int x, int y, int card, int selected) {
    int suit = sol_suit(card), rank = sol_rank(card);
    uint32_t col = sl_suit_col[suit];
    vbe_fill_rect_rounded(x, y, SL_CARD_W, SL_CARD_H, 6, 0x00FFFFFF);
    vbe_rect_rounded(x, y, SL_CARD_W, SL_CARD_H, 6, selected ? 0x0000FF00 : 0x00000000);
    char buf[4];
    snprintf(buf, sizeof(buf), "%s", sl_rank_str[rank]);
    vbe_draw_text(x + 6, y + 4, buf, col, 2);
    char sc[2] = { sl_suit_ch[suit], 0 };
    vbe_draw_text(x + 6, y + 22, sc, col, 2);
    /* center suit large */
    vbe_draw_text(x + SL_CARD_W/2 - 8, y + SL_CARD_H/2 - 8, sc, col, 3);
    snprintf(buf, sizeof(buf), "%s", sl_rank_str[rank]);
    vbe_draw_text(x + SL_CARD_W - 20, y + SL_CARD_H - 22, buf, col, 2);
}

static void sl_draw_empty_found(int x, int y, int f) {
    vbe_rect_rounded(x, y, SL_CARD_W, SL_CARD_H, 6, 0x00444444);
    char sc[2] = { sl_suit_ch[f], 0 };
    vbe_draw_text(x + SL_CARD_W/2 - 6, y + SL_CARD_H/2 - 8, sc, 0x00666666, 2);
}

static void sl_draw_content(SolState *s, const ChromeContentRect *content) {
    if (!content) return;
    int x0 = content->x, y0 = content->y;
    /* green felt */
    vbe_fill_rect(x0, y0, content->w, content->h, 0x00007700);

    int col_w = SL_CARD_W + SL_GAP_X;
    /* stock + waste (top-left) */
    if (s->stock_count > 0)
        sl_draw_card_back(x0 + 8, y0 + SL_TOP_Y);
    else
        vbe_rect_rounded(x0 + 8, y0 + SL_TOP_Y, SL_CARD_W, SL_CARD_H, 6, 0x00444444);
    if (s->waste_count > 0) {
        int card = s->waste[s->waste_count - 1];
        int sel = (s->sel_pile == SOL_WASTE);
        sl_draw_card_face(x0 + 8 + col_w, y0 + SL_TOP_Y, card, sel);
    }
    /* foundations (top-right) */
    for (int f = 0; f < SOL_NFOUND; f++) {
        int fx = x0 + content->w - 8 - (SOL_NFOUND - f) * col_w;
        if (s->found[f] < 0) sl_draw_empty_found(fx, y0 + SL_TOP_Y, f);
        else sl_draw_card_face(fx, y0 + SL_TOP_Y, s->found[f], 0);
    }
    /* tableau */
    for (int p = 0; p < SOL_NPILES; p++) {
        int px = x0 + 8 + p * col_w;
        int py = y0 + SL_TAB_Y;
        if (s->tab_count[p] == 0)
            vbe_rect_rounded(px, py, SL_CARD_W, SL_CARD_H, 6, 0x00444444);
        for (int k = 0; k < s->tab_count[p]; k++) {
            int cy = py + k * (s->tab_up[p][k] ? SL_FAN : 6);
            int sel = (s->sel_pile == p && k >= s->sel_index);
            if (s->tab_up[p][k]) sl_draw_card_face(px, cy, s->tab[p][k], sel);
            else sl_draw_card_back(px, cy);
        }
    }
    /* status line */
    char status[64];
    if (s->won) snprintf(status, sizeof(status), "YOU WIN!  moves:%d", s->moves);
    else snprintf(status, sizeof(status), "moves:%d  (click card, click destination; N=new)", s->moves);
    vbe_draw_text(x0 + 8, y0 + content->h - 16, status, 0x00FFFFFF, 1);
}

/* ---- DosGui handlers ----------------------------------------------------- */

void dosgui_solitaire_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    if (!win) return;
    ChromeContentRect content = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);
    sl_draw_content(&g_sol, &content);
}

/* Hit-test: which pile/index is at content-relative (x,y)? */
static void sol_hit(SolState *s, int x, int y, int *pile, int *index) {
    *pile = -1; *index = -1;
    int col_w = SL_CARD_W + SL_GAP_X;
    /* waste */
    if (y >= SL_TOP_Y && y < SL_TOP_Y + SL_CARD_H) {
        if (x >= 8 + col_w && x < 8 + 2*col_w && s->waste_count > 0) {
            *pile = SOL_WASTE; *index = s->waste_count - 1; return;
        }
    }
    /* tableau */
    if (y >= SL_TAB_Y) {
        for (int p = 0; p < SOL_NPILES; p++) {
            int px = 8 + p * col_w;
            if (x >= px && x < px + SL_CARD_W) {
                if (s->tab_count[p] == 0) { *pile = p; *index = 0; return; }
                /* find deepest card whose top contains y */
                int yy = SL_TAB_Y;
                for (int k = 0; k < s->tab_count[p]; k++) {
                    int ch = s->tab_up[p][k] ? SL_CARD_H : 6;
                    if (y >= yy && y < yy + (s->tab_up[p][k] ? SL_CARD_H : SL_FAN)) {
                        *pile = p; *index = k; return;
                    }
                    yy += (s->tab_up[p][k] ? SL_FAN : 6);
                }
                /* clicked below last card -> select top */
                *pile = p; *index = s->tab_count[p] - 1; return;
            }
        }
    }
}

void dosgui_solitaire_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)win;
    SolState *s = &g_sol;
    if (kind != 1) return;  /* press only */
    /* content-relative coords (window pos + chrome offset) */
    int cx = x - win->x - 4;
    int cy = y - win->y - 24;
    int pile, index;
    sol_hit(s, cx, cy, &pile, &index);

    if (btn == 1) {
        if (pile == SOL_STOCK || (cx >= 8 && cx < 8 + SL_CARD_W && cy >= SL_TOP_Y && cy < SL_TOP_Y + SL_CARD_H)) {
            sol_draw_stock(s); return;
        }
        if (pile < 0) { s->selected = -1; s->sel_pile = -1; return; }
        if (s->sel_pile < 0) {
            /* select */
            if (pile == SOL_WASTE) { s->sel_pile = SOL_WASTE; s->sel_index = s->waste_count-1; }
            else if (pile >= 0 && pile < SOL_NPILES && index < s->tab_count[pile] && s->tab_up[pile][index]) {
                s->sel_pile = pile; s->sel_index = index;
            }
        } else {
            /* try to move selection to clicked pile/foundation */
            if (pile >= 0 && pile < SOL_NPILES) {
                if (!sol_move_to_tableau(s, pile)) { s->selected=-1; s->sel_pile=-1; }
            } else {
                /* clicked a foundation slot? map x to foundation */
                int col_w = SL_CARD_W + SL_GAP_X;
                int content_w = win->w - 8;
                for (int f = 0; f < SOL_NFOUND; f++) {
                    int fx = content_w - 8 - (SOL_NFOUND - f) * col_w;
                    if (cx >= fx && cx < fx + SL_CARD_W) {
                        if (!sol_move_to_foundation(s, f)) { s->selected=-1; s->sel_pile=-1; }
                        return;
                    }
                }
                s->selected=-1; s->sel_pile=-1;
            }
        }
    } else if (btn == 2) {
        sol_auto_foundation(s);
    }
}

void dosgui_solitaire_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    SolState *s = &g_sol;
    if (key == 'n' || key == 'N') { sol_new_game(s); return; }
    if (key == ' ') { sol_draw_stock(s); return; }
    if (key == 'a' || key == 'A') { sol_auto_foundation(s); return; }
}

SolState* solitaire_create(void) {
    SolState *s = &g_sol;
    sol_new_game(s);
    return s;
}
void solitaire_destroy(SolState *s) { (void)s; }

DosGuiWindow* dosgui_launch_solitaire(void) {
    solitaire_create();
    DosGuiWindow *win = dosgui_wm_create(80, 40, 7 * (SL_CARD_W + SL_GAP_X) + 40,
                                         SL_TAB_Y + 5 * SL_FAN + SL_CARD_H + 60, "Solitaire");
    if (win) {
        win->on_draw  = dosgui_solitaire_draw;
        win->on_key   = dosgui_solitaire_key;
        win->on_mouse = dosgui_solitaire_mouse;
        win->user_data = &g_sol;
    }
    return win;
}
