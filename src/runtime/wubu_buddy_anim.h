/*
 * wubu_buddy_anim.h -- WuBu Buddy emotion/pose deck engine (opaque).
 *
 * The deck is COMBINATORIAL, not a sprite dump: a small rig (parts), a base
 * pose deck, an expression deck (eyes x mouth per the live-emoji param
 * model), an emotion matrix (valence x arousal x energy -> pose+expression+
 * palette wash), and tweened FSM transitions. The engine EXPRESSES the
 * 1000-vibe / 5000-animation space; it does not ship one sprite per state.
 */
#ifndef WUBU_BUDDY_ANIM_H
#define WUBU_BUDDY_ANIM_H

#include <stdint.h>

/* Base action deck. Each pose is a set of part transforms; the rig renders
 * the buddy's own pixels through them. */
typedef enum {
    BUDDY_POSE_IDLE, BUDDY_POSE_BLINK, BUDDY_POSE_WORK, BUDDY_POSE_THINK,
    BUDDY_POSE_CELEBRATE, BUDDY_POSE_ERROR, BUDDY_POSE_READ, BUDDY_POSE_WALK,
    BUDDY_POSE_RUN, BUDDY_POSE_SLEEP, BUDDY_POSE_EAT, BUDDY_POSE_WAVE,
    BUDDY_POSE_DANCE, BUDDY_POSE_DONE,
    BUDDY_POSE_COUNT
} wubu_buddy_pose_t;

/* Expression deck: independent eye lay * mouth lay (live-emoji params). */
typedef enum {
    BUDDY_EYE_OPEN, BUDDY_EYE_BLINK, BUDDY_EYE_HAPPY, BUDDY_EYE_WINK,
    BUDDY_EYE_WIDE, BUDDY_EYE_SAD, BUDDY_EYE_LOOK, BUDDY_EYE_X,
    BUDDY_EYE_COUNT
} wubu_buddy_eye_t;

typedef enum {
    BUDDY_MOUTH_NONE, BUDDY_MOUTH_SMILE, BUDDY_MOUTH_OPEN, BUDDY_MOUTH_FROWN,
    BUDDY_MOUTH_OH, BUDDY_MOUTH_TONGUE,
    BUDDY_MOUTH_COUNT
} wubu_buddy_mouth_t;

/* The three-axis mood. valence/arousal in [-2..2], energy in [0..4]. */
typedef struct {
    int valence;   /* -2 dread .. 0 .. +2 bliss   */
    int arousal;   /* -2 calm  .. 0 .. +2 frantic */
    int energy;    /* 0 drained .. 4 bursting     */
} wubu_buddy_mood_t;

/* Resolve a palette wash / expression tint from a mood (desktop-pets style,
 * derived from the base palette -- no new sprites for sad/calm/alarm). */
typedef struct {
    int   sat_shift;      /* signed desaturation (-100..100) */
    int   hue_shift;      /* crude rgb rotate in -2..2        */
    int   flash_red;      /* error/flash wash                    */
    wubu_buddy_pose_t  pose;
    wubu_buddy_eye_t   eye;
    wubu_buddy_mouth_t mouth;
} wubu_buddy_state_t;

/* Deck size oracle. These are the numbers the pet-deck must never shrink
 * below (a selftest asserts them). */
int wubu_buddy_vibe_count(void);   /* >= 1000 distinct vibes/emotions expressible */
int wubu_buddy_anim_count(void);   /* >= 5000 distinct animations expressible    */

/* Map a named emotion token ("joy", "dread", "calm", ...) to a mood. */
int  wubu_buddy_mood_from_token(const char *token, wubu_buddy_mood_t *out);
/* Compose a render state from a mood. */
void wubu_buddy_compose(const wubu_buddy_mood_t *m, wubu_buddy_state_t *out);

/* A deterministic 32-bit animation id for (pose,eye,mouth,transition,tween,
 * seed) so every distinct tuple is a distinct, reproducible animation. */
uint32_t wubu_buddy_anim_id(wubu_buddy_pose_t pose, wubu_buddy_eye_t eye,
                            wubu_buddy_mouth_t mouth, unsigned trans,
                            unsigned tween, uint16_t seed);

/* Transition tweens: 0..steps of a smooth ease between two poses. Returns
 * the eased (already-interpolated) progress 0..1 for a part at step/stepCount. */
float wubu_buddy_ease_step(unsigned step, unsigned stepCount, unsigned ease);

#endif /* WUBU_BUDDY_ANIM_H */
