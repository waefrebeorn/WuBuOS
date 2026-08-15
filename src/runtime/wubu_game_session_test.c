/*
 * wubu_game_session_test.c -- the game-play training ledger test.
 *
 * Asserts:
 *   1. a play session begin -> active
 *   2. the end appends a REAL line to the ledger (with the world
 *      state at the session end)
 *   3. the ledger line has the game + the format + a duration
 */
#include "wubu_game_session.h"
#include "wubu_test.h"
#include "wubu_world.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* FAIL: use wubu_test.h */

/* the world stub — wubu_world.c pulls the whole driver registry; the
 * game-session test needs the snapshot's SHAPE (the real wubu_world
 * is proven by test_world). The stub fills a realistic world. */
static wubu_world_t g_stub_world;

void wubu_world_sample(void)
{
    memset(&g_stub_world, 0, sizeof(g_stub_world));
    g_stub_world.has_nvme = 1; g_stub_world.nvme_gb = 465;
    g_stub_world.has_gpu = 1; g_stub_world.has_wifi = 1;
    g_stub_world.wifi_link = 1;
    g_stub_world.battery_pct = 60;
    g_stub_world.cpu_temp = 72;
}

const wubu_world_t *wubu_world_snapshot(void) { return &g_stub_world; }

int main(void)
{
    printf("=== wubu_game_session_test (the AGI learns via games) ===\n");

    wubu_game_session_test_reset();

    /* 1. begin */
    wubu_game_session_begin("openarena", WUBU_GAME_LINUX);
    if (!wubu_game_session_active()) FAIL("not active after begin");

    /* 2. end appends the line */
    wubu_game_session_end();
    if (wubu_game_session_active()) FAIL("still active after end");

    FILE *f = fopen(wubu_game_session_log(), "r");
    if (!f) FAIL("ledger missing: %s", wubu_game_session_log());
    char line[1024];
    char last[1024] = "";
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        snprintf(last, sizeof(last), "%s", line);
    }
    fclose(f);

    if (strstr(last, "play|openarena|linux-elf|") == NULL)
        FAIL("ledger line lacks the game+format: %s", last);
    if (strstr(last, "hw[") == NULL || strstr(last, "heat[") == NULL)
        FAIL("ledger line lacks the world state: %s", last);
    printf("  PASS: the ledger line: %s\n", last);

    /* 3. a second play appends (the deltas are the training) */
    wubu_game_session_begin("halo_pc", WUBU_GAME_WIN32);
    wubu_game_session_end();
    f = fopen(wubu_game_session_log(), "r");
    int plays = 0;
    while (fgets(line, sizeof(line), f)) plays++;
    fclose(f);
    if (plays < 2) FAIL("plays = %d, want >= 2", plays);
    printf("  PASS: %d plays recorded (the deltas are the training)\n", plays);

    printf("=== ALL GAME-SESSION TESTS PASSED (games are training) ===\n");
    return 0;
}
