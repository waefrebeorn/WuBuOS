/*
 * wubu_buddy emotion core -- behaviour gates.
 *
 * These are not assert-nothing checks: they prove the sensor read lands, the
 * affect stays inside its documented range, and the thermal channel actually
 * moves the state (i.e. the buddy really is linked to the machine).
 */
#include "wubu_buddy_emotion.h"

#include <stdio.h>
#include <stdlib.h>

static int fails;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL %s\n", msg); ++fails; } \
} while (0)

static float mood_lo(float m)
{
    if (m == WUBU_BUDDY_MOOD_CALM)    return 0.0f;
    if (m == WUBU_BUDDY_MOOD_JOY)     return 1.0f;
    if (m == WUBU_BUDDY_MOOD_EXCITED) return 2.0f;
    if (m == WUBU_BUDDY_MOOD_ALERT)   return 3.0f;
    if (m == WUBU_BUDDY_MOOD_HOT)     return 4.0f;
    if (m == WUBU_BUDDY_MOOD_SLEEPY)  return 5.0f;
    return -1.0f;
}

int main(void)
{
    wubu_buddy_emotion *e = NULL;

    /* NULL must never crash: render code polls unconditionally. */
    wubu_buddy_emotion_poll(NULL, 0.5f);
    CHECK(wubu_buddy_emotion_create(&e) == 1, "create returns handle");
    CHECK(e != NULL, "handle non-NULL");

    /* Baseline poll: everything defined, nothing NaN-ish. */
    wubu_buddy_emotion_poll(e, 0.5f);
    float en = wubu_buddy_emotion_energy(e);
    float st = wubu_buddy_emotion_stress(e);
    float wa = wubu_buddy_emotion_warmth(e);
    float ht = wubu_buddy_emotion_heat(e);
    float ld = wubu_buddy_emotion_load(e);
    float mo = wubu_buddy_emotion_mood(e);

    CHECK(en >= 0.0f && en <= 1.0f, "energy clamped [0,1]");
    CHECK(st >= 0.0f && st <= 1.0f, "stress clamped [0,1]");
    CHECK(wa >= 0.0f && wa <= 1.0f, "warmth clamped [0,1]");
    CHECK(ht >= 0.0f && ht <= 1.0f, "heat clamped [0,1]");
    CHECK(ld >= 0.0f && ld <= 1.0f, "load clamped [0,1]");
    CHECK(mood_lo(mo) >= 0.0f, "mood is a known constant");

    /* Thermal coupling: force the GPU channel hot and let the smoother chase it.
     * If this does not move, the buddy is decorative rather than reactive. */
    const char *set = getenv("WUBU_BUDDY_EMOTION_TEST_FORCE");
    if (set) {
        setenv("WUBU_BUDDY_GPU_TEMP", "98", 1);
        float before = wubu_buddy_emotion_energy(e);
        for (int i = 0; i < 40; ++i)
            wubu_buddy_emotion_poll(e, 0.5f);
        float after = wubu_buddy_emotion_energy(e);
        CHECK(after > before + 0.10f,
              "hot machine raises energy (linked, not scripted)");
        CHECK(wubu_buddy_emotion_gpu_c(e) == 98.0f,
              "GPU override is read verbatim");
    }

    /* Load normalisation: load can never exceed one full core budget. */
    for (int i = 0; i < 20; ++i)
        wubu_buddy_emotion_poll(e, 0.25f);
    CHECK(wubu_buddy_emotion_load(e) <= 1.0f, "normalised load never exceeds 1");

    /* Determinism: identical inputs, identical outputs. */
    wubu_buddy_emotion *a = NULL, *b = NULL;
    wubu_buddy_emotion_create(&a);
    wubu_buddy_emotion_create(&b);
    for (int i = 0; i < 5; ++i) {
        wubu_buddy_emotion_poll(a, 0.5f);
        wubu_buddy_emotion_poll(b, 0.5f);
    }
    CHECK(wubu_buddy_emotion_energy(a) == wubu_buddy_emotion_energy(b),
          "same sensor state -> same energy");
    CHECK(wubu_buddy_emotion_mood(a) == wubu_buddy_emotion_mood(b),
          "same sensor state -> same mood");

    wubu_buddy_emotion_destroy(a);
    wubu_buddy_emotion_destroy(b);
    wubu_buddy_emotion_destroy(e);

    if (fails) {
        printf("[wubu_buddy_emotion] %d fail\n", fails);
        return 1;
    }
    printf("[wubu_buddy_emotion] %s\n",
           set ? "thermal coupling proven" : "all pass (no forced run)");
    return 0;
}
