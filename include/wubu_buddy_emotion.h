#ifndef WUBU_BUDDY_EMOTION_H
#define WUBU_BUDDY_EMOTION_H

#include <stddef.h>

/*
 * wubu_buddy emotion core.
 *
 * One concern: turn machine state into a continuous affect that the render
 * shader can consume.  No I/O policy lives here beyond reading the sensors;
 * the render side gets four floats and does not know where they came from.
 *
 * vitals -> affect mapping (all clamped to [0,1]):
 *   heat   = worst of CPU / GPU / SSD excess temperature
 *   load   = 1-min load average normalised by core count
 *   energy = smoothed drive, follows load then heat
 *   stress = rises with sustained heat, decays slowly (never snaps)
 *   warmth = slow EMA of package temperature
 */

#define WUBU_BUDDY_MOOD_CALM     0.0f
#define WUBU_BUDDY_MOOD_JOY      1.0f
#define WUBU_BUDDY_MOOD_EXCITED  2.0f
#define WUBU_BUDDY_MOOD_ALERT    3.0f
#define WUBU_BUDDY_MOOD_HOT      4.0f
#define WUBU_BUDDY_MOOD_SLEEPY   5.0f

typedef struct wubu_buddy_emotion wubu_buddy_emotion;

/* Returns 0 on success.  Never fails on missing hardware: every channel has a
 * documented fallback so a laptop without NVMe still produces a valid affect. */
int  wubu_buddy_emotion_create(wubu_buddy_emotion **out);
void wubu_buddy_emotion_destroy(wubu_buddy_emotion *e);

/* Sample the machine now.  dt is seconds since the previous poll and is used
 * only for smoothing; pass 0.5-2.0 for a live view. */
void wubu_buddy_emotion_poll(wubu_buddy_emotion *e, float dt);

float wubu_buddy_emotion_energy(const wubu_buddy_emotion *e); /* liveliness */
float wubu_buddy_emotion_stress(const wubu_buddy_emotion *e); /* strain     */
float wubu_buddy_emotion_warmth(const wubu_buddy_emotion *e); /* body heat  */
float wubu_buddy_emotion_heat(const wubu_buddy_emotion *e);   /* raw thermal */
float wubu_buddy_emotion_load(const wubu_buddy_emotion *e);   /* CPU load   */
float wubu_buddy_emotion_mood(const wubu_buddy_emotion *e);   /* one of the constants */

/* Raw readings for diagnostics / HUD text.  Temperatures in degrees C. */
float wubu_buddy_emotion_cpu_c(const wubu_buddy_emotion *e);
float wubu_buddy_emotion_gpu_c(const wubu_buddy_emotion *e);
float wubu_buddy_emotion_ssd_c(const wubu_buddy_emotion *e);

#endif /* WUBU_BUDDY_EMOTION_H */
