/*
 * wubu_agi_play.h -- the AGI PLAYS the games (perception-action).
 */
#ifndef WUBU_AGI_PLAY_H
#define WUBU_AGI_PLAY_H

#include "wubu_game_launch.h"
#include "../kernel/wubu_world.h"

/* the actions */
enum {
    AGI_ACT_MOVE_FWD = 0,
    AGI_ACT_MOVE_BWD,
    AGI_ACT_STRAFE_L,
    AGI_ACT_STRAFE_R,
    AGI_ACT_JUMP,
    AGI_ACT_FIRE,
    AGI_ACT_NONE,
};

/* AP1: start the AGI play session. */
void wubu_agi_play_start(const char *game, WUBU_GAME_KIND kind);

/* AP2: stop (the ledger line lands). */
void wubu_agi_play_stop(void);

/* AP2b: opt in to REAL input injection.  Off by default so the desktop
 * session never clicks or types on the user's behalf; a game session
 * enables it to drive the game through the shared input queue. */
void wubu_agi_play_set_input_injection(int on);

/* AP3: the policy — the world state -> an action. */
int wubu_agi_play_policy(const wubu_world_t *w);

/* AP5: one tick (perceive -> decide -> act). Returns the action. */
int wubu_agi_play_tick(void);

/* AP6: the experience ledger (world -> action lines). */
void wubu_agi_play_learn(void);

/* AP7: the test hooks. */
void wubu_agi_play_test_reset(void);
int wubu_agi_play_actions(int action);
int wubu_agi_play_ticks(void);

#endif
