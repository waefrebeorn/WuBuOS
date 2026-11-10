/*
 * bpm_games.h -- Big Picture Mode's games grid (the real
 * games through the kernel routing).
 */
#ifndef WUBU_DOSGUI_BPM_GAMES_H
#define WUBU_DOSGUI_BPM_GAMES_H

#include "wubu_game_launch.h"

/* BG1: init. */
void bpm_games_init(void);

/* BG2: the launch hook override (the tests). */
void bpm_games_set_launch(int (*fn)(const char *path, const char *name));

/* BG3: scan the real games from ~/.wubu/games/. Returns the count. */
int bpm_games_scan(void);

/* BG4: the count + the accessors. */
int bpm_games_count(void);
const char *bpm_games_name(int idx);
const char *bpm_games_path(int idx);
WUBU_GAME_KIND bpm_games_kind(int idx);
int bpm_games_selected(void);

/* BG5: launch the selected game (the kernel personality routing). */
int bpm_games_launch_selected(void);

/* BG6: the gamepad navigation. */
void bpm_games_select(int idx);
void bpm_games_move(int delta);

/* BG7: the test hooks. */
typedef struct {
    int count;
    int selected;
    int launched;
} bpm_games_view_t;
int bpm_games_get(bpm_games_view_t *out);

#endif
