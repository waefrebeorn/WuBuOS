/*
 * wubu_agi_play_test.c -- the AGI-playing bridge test.
 *
 * Asserts:
 *   1. the policy: hot/throttled -> strafe (cool off)
 *   2. the policy: low battery -> back off
 *   3. a tick pushes a REAL input event (the input queue)
 *   4. the action histogram grows
 *   5. start/stop bounds the game-session ledger
 */
#include "wubu_agi_play.h"
#include "wubu_game_session.h"
#include "../kernel/input.h"
#include "../kernel/wubu_kvfs.h"
#include <stdio.h>
#include <string.h>

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

/* the input stubs — the real input.c links the kernel queue; the
 * test records the pushes */
static int g_key_pushes = 0;
static int g_mouse_pushes = 0;

void input_key_push(KeyEvent ev) { (void)ev; g_key_pushes++; }
void input_mouse_push(MouseEvent ev) { (void)ev; g_mouse_pushes++; }

/* the world stubs — the real wubu_world.c pulls the driver registry;
 * the AGI-play test needs the snapshot's shape */
static wubu_world_t g_stub_world;

void wubu_world_sample(void)
{
    memset(&g_stub_world, 0, sizeof(g_stub_world));
    g_stub_world.cpu_temp = 55;
    g_stub_world.battery_pct = 80;
    g_stub_world.battery_charging = 1;
    g_stub_world.wifi_link = 1;
}

const wubu_world_t *wubu_world_snapshot(void) { return &g_stub_world; }

int main(void)
{
    printf("=== wubu_agi_play_test (the AGI plays the games) ===\n");

    wubu_agi_play_test_reset();

    /* 1. the policy: hot -> strafe */
    wubu_world_t w;
    memset(&w, 0, sizeof(w));
    w.cpu_temp = 92; w.throttled = 1;
    if (wubu_agi_play_policy(&w) != AGI_ACT_STRAFE_L)
        FAIL("hot policy");
    printf("  PASS: throttled (92C) -> strafe_l (cool off)\n");

    /* 2. the policy: low battery -> back off */
    memset(&w, 0, sizeof(w));
    w.cpu_temp = 50;
    w.battery_pct = 15; w.battery_charging = 0;
    if (wubu_agi_play_policy(&w) != AGI_ACT_MOVE_BWD)
        FAIL("battery policy");
    printf("  PASS: low battery (15%) -> move_bwd (back off)\n");

    /* 3. a tick pushes a REAL input event */
    wubu_agi_play_start("openarena", WUBU_GAME_LINUX);
    int a = wubu_agi_play_tick();
    if (a < 0 || a >= AGI_ACT_NONE) FAIL("tick rc");
    if (g_key_pushes == 0 && g_mouse_pushes == 0)
        FAIL("no input pushed");
    printf("  PASS: the tick pushes a real input event (key %d, mouse %d)\n",
           g_key_pushes, g_mouse_pushes);

    /* 4. the histogram grows across ticks (one wander step is NONE,
     * which the histogram excludes — 11 ticks = 10 counted) */
    for (int i = 0; i < 10; i++) wubu_agi_play_tick();
    int total = 0;
    for (int i = 0; i < AGI_ACT_NONE; i++) total += wubu_agi_play_actions(i);
    if (total != 10) FAIL("histogram total = %d, want 10 (one idle step)", total);
    printf("  PASS: %d actions across 11 ticks\n", total);

    /* 5. start/stop bounds the game session */
    wubu_agi_play_stop();
    if (wubu_agi_play_tick() != -1) FAIL("tick after stop");
    printf("  PASS: stop bounds the play (no ticks after)\n");

    /* 6. the AGI LEARNS into the KV-FS (the training stream writes
     * /kv/world/tick_<N> as 4-packed floats: action, temp, battery, wifi).
     * The Brain reads these over 9P at /n/kv/world/. */
    wubu_agi_play_start("openarena", WUBU_GAME_LINUX);
    /* reset the KVFS to a clean state for verification */
    if (g_wubu_kvfs) { wubu_kvfs_free(g_wubu_kvfs); g_wubu_kvfs = NULL; }
    if (g_wubu_kv_base) { free(g_wubu_kv_base); g_wubu_kv_base = NULL; }
    g_wubu_kv_capacity = 0;
    wubu_kvfs_kernel_init(256, 4096);
    wubu_kvfs_mount(g_wubu_kvfs, "/kv/world", 0, 256);
    /* run 5 ticks + learn */
    for (int i = 0; i < 5; i++) {
        wubu_agi_play_tick();
        wubu_agi_play_learn();
    }
    /* verify the tick_4 vector landed in the KV tensor */
    float vec[4] = {0};
    int rc = wubu_kvfs_read(g_wubu_kvfs, "/kv/world/tick_4", g_wubu_kv_base, vec, 4);
    if (rc != 0) FAIL("kvfs read tick_4 failed");
    printf("  PASS: KV-FS learn writes tick_4 = [act=%g temp=%g bat=%g net=%g]\n",
           vec[0], vec[1], vec[2], vec[3]);
    if (vec[0] < 0 || vec[0] >= AGI_ACT_NONE) FAIL("tick_4 bad action label %g", vec[0]);

    wubu_agi_play_stop();
    /* cleanup */
    if (g_wubu_kvfs) { wubu_kvfs_free(g_wubu_kvfs); g_wubu_kvfs = NULL; }
    if (g_wubu_kv_base) { free(g_wubu_kv_base); g_wubu_kv_base = NULL; }

    printf("=== ALL AGI-PLAY TESTS PASSED (the AGI plays + learns) ===\n");
    return 0;
}
