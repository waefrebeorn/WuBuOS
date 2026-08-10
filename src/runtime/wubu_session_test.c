/*
 * wubu_session_test.c -- the session manager test.
 *
 * Asserts the SteamOS session-select contract:
 *   1. init defaults to desktop (the dosgui shell)
 *   2. set(game) produces the gamescope session cmd
 *   3. switch() toggles game <-> desktop
 *   4. the name parse round-trips
 */
#include "wubu_session.h"
#include <stdio.h>
#include <string.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    printf("=== wubu_session_test (the SteamOS session manager) ===\n");

    wubu_session_init(WUBU_SESSION_DESKTOP);

    /* 1. defaults */
    wubu_session_view_t v;
    wubu_session_get(&v);
    if (!v.initialized) FAIL("not initialized");
    if (v.current != WUBU_SESSION_DESKTOP)
        FAIL("start = %d, want desktop", v.current);
    if (strcmp(v.last_cmd, "wubu-desktop") != 0)
        FAIL("desktop cmd = '%s'", v.last_cmd);
    printf("  PASS: init defaults to the desktop session\n");

    /* 2. game mode */
    if (wubu_session_set(WUBU_SESSION_GAME) != 0) FAIL("set game");
    if (wubu_session_current() != WUBU_SESSION_GAME) FAIL("not game");
    wubu_session_get(&v);
    if (strstr(v.last_cmd, "gamescope") == NULL)
        FAIL("game cmd lacks gamescope: '%s'", v.last_cmd);
    printf("  PASS: game mode -> the gamescope session cmd\n");

    /* 3. switch toggles */
    if (wubu_session_switch() != 0) FAIL("switch 1");
    if (wubu_session_current() != WUBU_SESSION_DESKTOP) FAIL("switch->desktop");
    if (wubu_session_switch() != 0) FAIL("switch 2");
    if (wubu_session_current() != WUBU_SESSION_GAME) FAIL("switch->game");
    printf("  PASS: switch() toggles game <-> desktop\n");

    /* 4. the name parse */
    if (wubu_session_from_name("game") != WUBU_SESSION_GAME) FAIL("parse game");
    if (wubu_session_from_name("desktop") != WUBU_SESSION_DESKTOP) FAIL("parse desktop");
    if (wubu_session_from_name("bogus") != -1) FAIL("parse bogus accepted");
    if (strcmp(wubu_session_name(WUBU_SESSION_GAME), "game") != 0) FAIL("name game");
    if (strcmp(wubu_session_name(WUBU_SESSION_DESKTOP), "desktop") != 0) FAIL("name desktop");
    printf("  PASS: the session names parse round-trip\n");

    /* 5. a bad set is refused */
    if (wubu_session_set(99) == 0) FAIL("bad session accepted");
    printf("  PASS: a bad session is refused\n");

    printf("=== ALL SESSION TESTS PASSED (the session manager) ===\n");
    return 0;
}
