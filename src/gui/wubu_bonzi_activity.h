/*
 * wubu_bonzi_activity.h -- the companion's activity feedback (Track B6).
 *
 * Maps an activity state (idle/thinking/working/alert) to animation
 * modifiers (blink interval, eye-open scale, bob speed) the draw loop
 * blends with the idle bob, so the orb feels present, not a black box.
 * Pure + dependency-light.
 */
#ifndef WUBU_BONZI_ACTIVITY_H
#define WUBU_BONZI_ACTIVITY_H

typedef enum {
    WUBU_ACT_IDLE = 0,     /* resting, breathing */
    WUBU_ACT_THINKING,     /* the Brain is reasoning */
    WUBU_ACT_WORKING,      /* executing a tool / task */
    WUBU_ACT_ALERT         /* something needs attention */
} wubu_bonzi_activity_t;

typedef struct {
    int   blink_ms;   /* blink interval for this state */
    float eye_scale;  /* multiplier on eye openness (1.0 = neutral) */
    float bob_scale;  /* multiplier on bob speed (1.0 = neutral) */
} wubu_bonzi_anim_mods_t;

/* Animation modifiers for an activity state. */
wubu_bonzi_anim_mods_t wubu_bonzi_activity_mods(wubu_bonzi_activity_t act);

/* "idle" / "thinking" / "working" / "alert" */
const char *wubu_bonzi_activity_name(wubu_bonzi_activity_t act);

#endif /* WUBU_BONZI_ACTIVITY_H */
