/*
 * solitaire.h  --  Solitaire (Klondike, Windows XP accessory parity)
 * Opaque struct, C11, minimal includes, self-contained game logic.
 */

#ifndef WUBU_SOLITAIRE_H
#define WUBU_SOLITAIRE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct DosGuiWindow DosGuiWindow;

#define SOL_NPILES 7       /* tableau piles */
#define SOL_NFOUND 4       /* foundations */
#define SOL_STOCK  (-1)    /* pseudo-pile id for stock */
#define SOL_WASTE  (-2)    /* pseudo-pile id for waste */
#define SOL_MAXRUN 19      /* max cards in a tableau pile */

typedef struct SolState SolState;

/* Lifecycle */
SolState* solitaire_create(void);
void      solitaire_destroy(SolState *s);
void      sol_new_game(SolState *s);

/* Moves (pure logic) */
int  sol_move_to_tableau(SolState *s, int p);
int  sol_move_to_foundation(SolState *s, int f);
void sol_draw_stock(SolState *s);
int  sol_auto_foundation(SolState *s);
void sol_check_win(SolState *s);

/* DosGui window handlers */
void dosgui_solitaire_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_solitaire_key(DosGuiWindow *win, uint32_t key, uint32_t mods);
void dosgui_solitaire_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);

/* Launch */
DosGuiWindow* dosgui_launch_solitaire(void);

#endif /* WUBU_SOLITAIRE_H */
