/*
 * wubu_agi_play.c -- the AGI PLAYS the games (the perception-action
 * training loop).
 *
 * The second goal: "fix the agi be able to play the games as
 * training too and also learning via running game". This is the
 * bridge:
 *
 *   1. PERCEIVE  — wubu_world_sample() (the driver registry's live
 *                  snapshot: the hardware, the heat, the battery)
 *   2. DECIDE    — a real policy: the AGI picks an action from the
 *                  world state (a scripted explorer today — the
 *                  trained policy plugs in here later)
 *   3. ACT       — the action becomes REAL input events (input_key_
 *                  push / input_mouse_push) — the AGI drives the
 *                  game through the same queue as the user
 *   4. LEARN     — each (world -> action) pair appends to the play
 *                  ledger (wubu_game_session): the experience stream
 *
 * The loop is the training substrate: every tick is a labeled
 * (perception, action) example the model learns from. C11.
 */
#include "wubu_agi_play.h"
#include "wubu_game_session.h"
#include "wubu_world.h"
#include "../kernel/wubu_kvfs.h"
#include "../kernel/input.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* the actions the AGI can take (the enum lives in wubu_agi_play.h) */

/* the scancodes (PC set-1) */
#define SC_W    0x11
#define SC_S    0x1F
#define SC_A    0x1E
#define SC_D    0x20
#define SC_SPC  0x39

typedef struct {
    int  active;
    int  ticks;
    int  actions[AGI_ACT_NONE];
    uint64_t last_tick_ms;
} AgiPlayState;

static AgiPlayState g_play;

/* the action names (the ledger labels) */
static const char *action_name(int a)
{
    switch (a) {
    case AGI_ACT_MOVE_FWD:  return "move_fwd";
    case AGI_ACT_MOVE_BWD:  return "move_bwd";
    case AGI_ACT_STRAFE_L:  return "strafe_l";
    case AGI_ACT_STRAFE_R:  return "strafe_r";
    case AGI_ACT_JUMP:      return "jump";
    case AGI_ACT_FIRE:      return "fire";
    default:                return "none";
    }
}

/* AP1: start the AGI play session (the game session begins too). */
void wubu_agi_play_start(const char *game, WUBU_GAME_KIND kind)
{
    memset(&g_play, 0, sizeof(g_play));
    /* Initialize the KV-FS (the AGI's training substrate) if not live.
     * The kernel KV-FS writes world-state deltas into /kv/world/tick_<N>
     * and the Brain reads them over 9P at /n/kv/. */
    if (!g_wubu_kvfs) {
        wubu_kvfs_namespace_init();
    }
    g_play.active = 1;
    wubu_game_session_begin(game, kind);
}

/* AP2: stop — the game session ends (the ledger line lands). */
void wubu_agi_play_stop(void)
{
    if (!g_play.active) return;
    wubu_game_session_end();
    g_play.active = 0;
}

/* AP3: the policy — choose an action from the world state. The
 * scripted explorer: hot -> strafe (cool off), low battery -> back
 * off, else wander. The trained policy replaces this later. */
int wubu_agi_play_policy(const wubu_world_t *w)
{
    if (!w) return AGI_ACT_NONE;
    if (w->throttled || w->cpu_temp >= 80)  return AGI_ACT_STRAFE_L;
    if (w->battery_pct < 20 && !w->battery_charging)
        return AGI_ACT_MOVE_BWD;
    /* wander: a deterministic pattern (tick-driven, no RNG) */
    switch (g_play.ticks % 8) {
    case 0: case 4: return AGI_ACT_MOVE_FWD;
    case 1: return AGI_ACT_JUMP;
    case 2: return AGI_ACT_STRAFE_R;
    case 3: return AGI_ACT_FIRE;
    case 5: return AGI_ACT_STRAFE_L;
    case 6: return AGI_ACT_MOVE_BWD;
    default: return AGI_ACT_NONE;
    }
}

/* AP4: act — push the REAL input events for the action. */
static void agi_act(int action)
{
    KeyEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.kind = KEY_EVENT_DOWN;
    switch (action) {
    case AGI_ACT_MOVE_FWD: ev.scancode = SC_W; break;
    case AGI_ACT_MOVE_BWD: ev.scancode = SC_S; break;
    case AGI_ACT_STRAFE_L: ev.scancode = SC_A; break;
    case AGI_ACT_STRAFE_R: ev.scancode = SC_D; break;
    case AGI_ACT_JUMP:     ev.scancode = SC_SPC; break;
    case AGI_ACT_FIRE: {
        MouseEvent m;
        memset(&m, 0, sizeof(m));
        m.buttons = 1;
        input_mouse_push(m);
        return;
    }
    default: return;
    }
    input_key_push(ev);
}

/* AP5: one tick of the loop. Returns the action taken. */
int wubu_agi_play_tick(void)
{
    if (!g_play.active) return -1;
    wubu_world_sample();
    const wubu_world_t *w = wubu_world_snapshot();
    int action = wubu_agi_play_policy(w);
    agi_act(action);
    g_play.actions[action]++;
    g_play.ticks++;
    g_play.last_tick_ms = (uint64_t)time(NULL) * 1000;
    return action;
}

/* AP6: the experience ledger — a (world -> action) vector written into
 * the KV-FS (/kv/world/tick_<N>) as 4 packed floats so the Brain
 * (wubuwizard) reads the FULL training stream over 9P at /n/kv/world/.
 *
 *   float[0] = action (0..6, the enum index)  -- the label
 *   float[1] = cpu_temp
 *   float[2] = battery_pct (0..100, or 255 if N/A)
 *   float[3] = wifi_link (0..1, quantized)   -- the reward channel
 *
 * Each tick is a self-describing leaf file in the namespace. The Brain's
 * KV reader (wubu_kvfs_handle_read) pulls them with zero string ops on
 * the hot path — resolve once, memcpy per tick.
 */
void wubu_agi_play_learn(void)
{
    if (!g_play.active) return;
    wubu_world_sample();
    const wubu_world_t *w = wubu_world_snapshot();
    if (!g_wubu_kvfs || !g_wubu_kv_base) {
        /* KV-FS not initialized — fall through to text log as fallback. */
        FILE *f = fopen(wubu_game_session_log(), "a");
        if (f) {
            fprintf(f, "learn|%s|cpu:%dC bat:%d%% %s net:%d -> %s\n",
                    wubu_game_session_active() ? "play" : "idle",
                    w ? (int)w->cpu_temp : 0,
                    w ? (int)w->battery_pct : 0,
                    w && w->battery_charging ? "chg" : "disc",
                    w ? w->wifi_link : 0,
                    action_name(wubu_agi_play_policy(w)));
            fclose(f);
        }
        return;
    }

    /* Build the world-state path for this tick: /kv/world/tick_<N> */
    char path[96];
    snprintf(path, sizeof(path), "/kv/world/tick_%d", g_play.ticks);

    /* Ensure the /kv/world mount covers the write. The kernel init
     * pre-mounts /kv/in and /kv/world over disjoint block ranges. */
    int action = wubu_agi_play_policy(w);
    float vec[4] = {
        (float)action,
        (float)(w ? w->cpu_temp : 0),
        (float)(w ? w->battery_pct : 255),
        (float)(w ? w->wifi_link : 0)
    };
    /* write 4 floats (16 bytes) at /kv/world/tick_<N> */
    wubu_kvfs_write(g_wubu_kvfs, path, g_wubu_kv_base, vec, 4);

    /* Also keep the text ledger for debugging (append mode, low-freq). */
    FILE *f = fopen(wubu_game_session_log(), "a");
    if (f) {
        fprintf(f, "kvlearn|%s|t%d a=%d cpu=%dC bat=%d%% net=%d\n",
                wubu_game_session_active() ? "play" : "idle",
                g_play.ticks, action,
                w ? (int)w->cpu_temp : 0,
                w ? (int)w->battery_pct : 0,
                w ? w->wifi_link : 0);
        fclose(f);
    }
}

/* AP7: the test hooks */
void wubu_agi_play_test_reset(void)
{
    memset(&g_play, 0, sizeof(g_play));
}
int wubu_agi_play_actions(int action) { return g_play.actions[action]; }
int wubu_agi_play_ticks(void) { return g_play.ticks; }
