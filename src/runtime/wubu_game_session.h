/*
 * wubu_game_session.h -- the GAME-PLAY TRAINING LEDGER.
 */
#ifndef WUBU_GAME_SESSION_H
#define WUBU_GAME_SESSION_H

#include "wubu_game_launch.h"

/* GS1: begin a play session. */
void wubu_game_session_begin(const char *game, WUBU_GAME_KIND kind);

/* GS2: end the play — append the training line (game|format|t|dur|
 * world) to ~/.wubu/games/plays.log. */
void wubu_game_session_end(void);

/* GS3: is a play active? */
int wubu_game_session_active(void);

/* GS4: the ledger path. */
const char *wubu_game_session_log(void);

/* GS5: the test hooks. */
void wubu_game_session_test_reset(void);

#endif
