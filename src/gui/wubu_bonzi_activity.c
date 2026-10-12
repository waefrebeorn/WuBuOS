/*
 * wubu_bonzi_activity.c -- the companion's activity feedback (Track B6).
 *
 * A desktop companion should feel PRESENT, not a black box: when it's
 * thinking, its eyes narrow and it bobs faster; when it's working, it looks
 * focused; when alert, wide-eyed; when idle, it breathes. This module maps
 * an activity state to the animation modifiers (blink interval, eye-open
 * scale, bob speed) the draw loop blends with the idle bob.
 *
 * Pure + dependency-light: the companion sets its current activity; the
 * draw loop queries the modifiers. Unit-testable with no rendering.
 *
 * C11, minimal includes.
 */

#include "wubu_bonzi_activity.h"

/* Idle baselines (the resting breathing rate). */
#define ACT_IDLE_BLINK_MS   2600
#define ACT_IDLE_BOB_SCALE  1.0f

/* Per-state animation modifiers. */
wubu_bonzi_anim_mods_t wubu_bonzi_activity_mods(wubu_bonzi_activity_t act) {
    wubu_bonzi_anim_mods_t m;
    switch (act) {
    case WUBU_ACT_THINKING:
        /* narrowed eyes, faster bob -- visibly cogitating */
        m.blink_ms = 900;  m.eye_scale = 0.55f; m.bob_scale = 1.8f; break;
    case WUBU_ACT_WORKING:
        /* focused: steady, slightly faster, eyes mostly open */
        m.blink_ms = 1600; m.eye_scale = 0.9f;  m.bob_scale = 1.3f; break;
    case WUBU_ACT_ALERT:
        /* wide-eyed, quick -- something needs attention */
        m.blink_ms = 1200; m.eye_scale = 1.15f; m.bob_scale = 1.5f; break;
    case WUBU_ACT_IDLE:
    default:
        m.blink_ms = ACT_IDLE_BLINK_MS; m.eye_scale = 1.0f; m.bob_scale = ACT_IDLE_BOB_SCALE; break;
    }
    return m;
}

const char *wubu_bonzi_activity_name(wubu_bonzi_activity_t act) {
    switch (act) {
    case WUBU_ACT_THINKING: return "thinking";
    case WUBU_ACT_WORKING:  return "working";
    case WUBU_ACT_ALERT:    return "alert";
    case WUBU_ACT_IDLE: default: return "idle";
    }
}
