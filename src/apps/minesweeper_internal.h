/*
 * minesweeper_internal.h  --  private definitions for minesweeper.c
 * (kept separate so the opaque struct in minesweeper.h stays opaque)
 */
#ifndef WUBU_MINESWEEPER_INTERNAL_H
#define WUBU_MINESWEEPER_INTERNAL_H

#include "minesweeper.h"

/* Full definition of the opaque struct (minesweeper.c only) */
struct MineState {
    int cols, rows, mines;
    int grid_w, grid_h;
    uint8_t mine[MS_MAX_CELLS];
    uint8_t adj[MS_MAX_CELLS];
    uint8_t state[MS_MAX_CELLS];
    int revealed_count;
    int flag_count;
    int first_click;
    int game_over;
    int cursor_r, cursor_c;
    int started;
    uint32_t start_tick;
    int elapsed;
};

/* Cell pixel geometry (minesweeper.c rendering) */
#define MS_CELL   16
#define MS_HEADER 44
#define MS_MARGIN 12

#endif /* WUBU_MINESWEEPER_INTERNAL_H */
