/*
 * wubu_gamemode.h -- the game mode / perf governor (SteamOS E2).
 */
#ifndef WUBU_GAMEMODE_H
#define WUBU_GAMEMODE_H

/* GM1: init — record the current state (idempotent). */
void wubu_gamemode_init(void);

/* GM2: activate — switch everything to performance. */
int wubu_gamemode_activate(void);

/* GM3: deactivate — restore what init recorded. */
int wubu_gamemode_deactivate(void);

/* GM4: is game mode on? */
int wubu_gamemode_active(void);

/* GM5: the test hooks — point the sysfs reads at a fake tree. */
void wubu_gamemode_set_sysroot(const char *root);

typedef struct {
    int  active;
    char prev_governor[64];
    char sys_root[128];
    int  policies_switched;
} wubu_gamemode_view_t;
int wubu_gamemode_get(const wubu_gamemode_view_t *out);

/* the governor names (the tests assert the writes) */
const char *wubu_gamemode_gov_perf(void);
const char *wubu_gamemode_gov_restore(void);

#endif
