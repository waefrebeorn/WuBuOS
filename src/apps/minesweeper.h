/*
 * minesweeper.h  --  Minesweeper (Windows XP accessory parity)
 * Opaque struct, C11, minimal includes, self-contained game logic.
 */

#ifndef WUBU_MINESWEEPER_H
#define WUBU_MINESWEEPER_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration for window manager */
typedef struct DosGuiWindow DosGuiWindow;

/* Cell states */
#define MS_HIDDEN   0
#define MS_REVEALED 1
#define MS_FLAGGED  2

/* Game-over codes */
#define MS_PLAYING 0
#define MS_WON     1
#define MS_LOST    2

#define MS_MAX_CELLS 900   /* 30x30 max */

/* Opaque game state */
typedef struct MineState MineState;

/* Lifecycle */
MineState* minesweeper_create(void);
void       minesweeper_destroy(MineState *m);
void       minesweeper_new_game(MineState *m, int grid_w, int grid_h, int mines);
void       minesweeper_tick(MineState *m);

/* Queries */
int  minesweeper_flag_count(const MineState *m);
int  minesweeper_is_over(const MineState *m);

/* Actions */
void dosgui_minesweeper_reveal(int r, int c);
void dosgui_minesweeper_flag(int r, int c);

/* DosGui window handlers */
void dosgui_minesweeper_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_minesweeper_key(DosGuiWindow *win, uint32_t key, uint32_t mods);
void dosgui_minesweeper_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);

/* Launch a Minesweeper window */
DosGuiWindow* dosgui_launch_minesweeper(void);

#endif /* WUBU_MINESWEEPER_H */
