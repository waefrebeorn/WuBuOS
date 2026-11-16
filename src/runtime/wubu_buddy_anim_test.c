#include "wubu_buddy_anim.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, passes = 0;
#define T(cond, msg) do { \
    if (cond) { passes++; } \
    else { fails++; printf("FAIL: %s\n", msg); } \
} while (0)

int main(void) {
    printf("[wubu_buddy_anim] deck oracle\n");

    int vibes = wubu_buddy_vibe_count();
    int anims = wubu_buddy_anim_count();
    printf("  vibes=%d anims=%d\n", vibes, anims);

    T(vibes >= 1000, "vibe deck must express >= 1000 distinct vibes/emotions");
    T(anims >= 5000, "animation deck must express >= 5000 distinct animations");

    /* Emotion tokens resolve to a composed render state. */
    wubu_buddy_mood_t m;
    wubu_buddy_state_t s;
    T(wubu_buddy_mood_from_token("joy", &m) == 0, "token 'joy' resolves");
    T(wubu_buddy_mood_from_token("bliss", &m) == 0 && m.valence == 2,
      "bliss -> valence +2");
    T(wubu_buddy_mood_from_token("dread", &m) == 0 && m.valence == -2,
      "dread -> valence -2");
    T(wubu_buddy_mood_from_token("definitely-not-an-emotion", &m) != 0,
      "unknown token fails (caller falls back to neutral)");

    wubu_buddy_mood_t hi = { .valence = 2, .arousal = 2, .energy = 4 };
    wubu_buddy_compose(&hi, &s);
    T(s.pose == BUDDY_POSE_DANCE, "high-positive mood -> dance pose");
    T(s.flash_red == 0, "positive mood no red flash");

    wubu_buddy_mood_t low = { .valence = -2, .arousal = 1, .energy = 3 };
    wubu_buddy_compose(&low, &s);
    T(s.pose == BUDDY_POSE_ERROR, "negative mood -> error pose");

    /* Deterministic ids: same args -> same id, different args differ. */
    uint32_t id1 = wubu_buddy_anim_id(BUDDY_POSE_CELEBRATE, BUDDY_EYE_HAPPY,
                                      BUDDY_MOUTH_OPEN, 0, 1, 42);
    uint32_t id2 = wubu_buddy_anim_id(BUDDY_POSE_CELEBRATE, BUDDY_EYE_HAPPY,
                                      BUDDY_MOUTH_OPEN, 0, 1, 42);
    uint32_t id3 = wubu_buddy_anim_id(BUDDY_POSE_WORK, BUDDY_EYE_HAPPY,
                                      BUDDY_MOUTH_OPEN, 0, 1, 42);
    T(id1 == id2, "anim id deterministic");
    T(id1 != id3, "different pose -> different id");

    /* Easing is monotonic in the smooth range. */
    float e0 = wubu_buddy_ease_step(0, 10, 3);
    float e5 = wubu_buddy_ease_step(5, 10, 3);
    float e9 = wubu_buddy_ease_step(9, 10, 3);
    T(e0 <= e5 && e5 <= e9 && e9 <= 1.0f, "smoothstep monotonic up to <=1");

    printf("[wubu_buddy_anim] %d pass, %d fail\n", passes, fails);
    return fails ? 1 : 0;
}
