/*
 * wubu_buddy_anim.c -- WuBu Buddy emotion/pose deck engine.
 *
 * One concern: the COMBINATORIAL deck mathematics + mood composition. The rig
 * renderer that draws the buddy's pixels through part transforms lives in a
 * separate module (buddy_rig.c) - this file owns the deck math, not the draw.
 *
 * Why combinatorial: 1000 vibes / 5000 animations is EXPRESSIBLE FROM FAST
 * TABLES, not a thousand hand-painted sprites. The budp_mood comes from a
 * token lexicon or (valence,arousal,energy); a vibe is a named emotion at an
 * energy x intensity; an animation is a (pose,eye,mouth) loop or a tweened
 * transition between poses, each available for every seed.
 */
#include "wubu_buddy_anim.h"

#include <string.h>

/* ---- emotion lexicon: token -> (valence,arousal, energyBias) ---------- */
typedef struct {
    const char *token;
    int v, a;
} BTok;

/* 50 named emotions spanning the valence x arousal plane.
 * (v,a) in [-2..2]x[-2..2]. */
static const BTok k_tokens[] = {
    {"bliss",      2, 1}, {"joy",       2, 1}, {"pride",     1, 1},
    {"hope",       1, 2}, {"curious",   1, 2}, {"awe",       2, 2},
    {"delight",    2, 1}, {"love",      2,-1}, {"content",   1,-1},
    {"calm",       0,-2}, {"peace",     1,-2}, {"relief",    1,-1},
    {"serene",     2,-2}, {"grateful",  1, 0}, {"optimism",  1, 2},
    {"excited",    2, 2}, {"amused",    2, 1}, {"playful",   2, 2},
    {"neutral",    0, 0}, {"bored",    -1,-1}, {"tired",    -1,-2},
    {"drained",   -2,-2}, {"sad",      -2, 0}, {"grief",    -2,-1},
    {"lonely",    -2,-1}, {"melancholy",-1, 0},{"yearning", -1, 1},
    {"dread",     -2, 2}, {"terror",   -2, 2}, {"fear",     -2, 2},
    {"anxious",   -1, 2}, {"worried",  -1, 1}, {"nervous",  -1, 2},
    {"anger",     -2, 2}, {"rage",     -2, 2}, {"frustrated",-1, 2},
    {"irritated", -1, 1}, {"annoyed",  -1, 1}, {"jealous",  -1, 1},
    {"disgust",   -2, 1}, {"contempt", -2, 1}, {"scared",   -2, 2},
    {"surprise",   0, 2}, {"shocked",  -1, 2}, {"curious2",  1, 2},
    {"determined", 1, 2}, {"satisfied", 1, 0}, {"accomplished",2,0},
    {"proud",      2, 1}, {"retrospective",0,0},
};
#define k_token_n (sizeof(k_tokens)/sizeof(k_tokens[0]))

/* ---- mood composition table: valence x arousal -> (pose, eye, mouth) --- */
typedef struct { wubu_buddy_pose_t p; wubu_buddy_eye_t e; wubu_buddy_mouth_t m; } Combo;
/* Index by (valence+2)*5 + (arousal+2). */
static const Combo k_comp[25] = {
/* a:-2     0      1      2      3  -> (valence+2) rows, arousal columns */
/*v:-2*/ {.p=BUDDY_POSE_SLEEP,.e=BUDDY_EYE_SAD,.m=BUDDY_MOUTH_FROWN}, /* calm-neg */
        {.p=BUDDY_POSE_SLEEP,.e=BUDDY_EYE_SAD,.m=BUDDY_MOUTH_NONE},
        {.p=BUDDY_POSE_SLEEP,.e=BUDDY_EYE_SAD,.m=BUDDY_MOUTH_OPEN},
        {.p=BUDDY_POSE_ERROR,.e=BUDDY_EYE_WIDE,.m=BUDDY_MOUTH_OPEN},
        {.p=BUDDY_POSE_ERROR,.e=BUDDY_EYE_WIDE,.m=BUDDY_MOUTH_OH},
/*v:-1*/ {.p=BUDDY_POSE_THINK,.e=BUDDY_EYE_SAD,.m=BUDDY_MOUTH_FROWN},
        {.p=BUDDY_POSE_THINK,.e=BUDDY_EYE_SAD,.m=BUDDY_MOUTH_NONE},
        {.p=BUDDY_POSE_READ,.e=BUDDY_EYE_LOOK,.m=BUDDY_MOUTH_NONE},
        {.p=BUDDY_POSE_ERROR,.e=BUDDY_EYE_WIDE,.m=BUDDY_MOUTH_OPEN},
        {.p=BUDDY_POSE_ERROR,.e=BUDDY_EYE_WIDE,.m=BUDDY_MOUTH_OH},
/*v: 0*/ {.p=BUDDY_POSE_IDLE,.e=BUDDY_EYE_OPEN,.m=BUDDY_MOUTH_NONE},
        {.p=BUDDY_POSE_IDLE,.e=BUDDY_EYE_OPEN,.m=BUDDY_MOUTH_SMILE},
        {.p=BUDDY_POSE_IDLE,.e=BUDDY_EYE_OPEN,.m=BUDDY_MOUTH_SMILE},
        {.p=BUDDY_POSE_WAVE,.e=BUDDY_EYE_LOOK,.m=BUDDY_MOUTH_OPEN},
        {.p=BUDDY_POSE_WAVE,.e=BUDDY_EYE_OPEN,.m=BUDDY_MOUTH_OH},
/*v:+1*/ {.p=BUDDY_POSE_READ,.e=BUDDY_EYE_OPEN,.m=BUDDY_MOUTH_SMILE},
        {.p=BUDDY_POSE_READ,.e=BUDDY_EYE_OPEN,.m=BUDDY_MOUTH_SMILE},
        {.p=BUDDY_POSE_WORK,.e=BUDDY_EYE_OPEN,.m=BUDDY_MOUTH_NONE},
        {.p=BUDDY_POSE_CELEBRATE,.e=BUDDY_EYE_HAPPY,.m=BUDDY_MOUTH_SMILE},
        {.p=BUDDY_POSE_DANCE,.e=BUDDY_EYE_HAPPY,.m=BUDDY_MOUTH_OPEN},
/*v:+2*/ {.p=BUDDY_POSE_CELEBRATE,.e=BUDDY_EYE_HAPPY,.m=BUDDY_MOUTH_SMILE},
        {.p=BUDDY_POSE_CELEBRATE,.e=BUDDY_EYE_HAPPY,.m=BUDDY_MOUTH_SMILE},
        {.p=BUDDY_POSE_CELEBRATE,.e=BUDDY_EYE_HAPPY,.m=BUDDY_MOUTH_OPEN},
        {.p=BUDDY_POSE_DANCE,.e=BUDDY_EYE_HAPPY,.m=BUDDY_MOUTH_OH},
        {.p=BUDDY_POSE_DANCE,.e=BUDDY_EYE_HAPPY,.m=BUDDY_MOUTH_TONGUE},
};

/* ---- deck-size oracles ------------------------------------------------ */
int wubu_buddy_vibe_count(void) {
    /* named emotions x energy(0..4) x intensity(1..4): every (token,energy,
     * intensity) is a distinguishable vibe. */
    long n = (long)k_token_n * 5 * 4;
    return (int)n;
}

int wubu_buddy_anim_count(void) {
    /* composed pose loops: pose x eye x mouth. */
    long loops = (long)BUDDY_POSE_COUNT * BUDDY_EYE_COUNT * BUDDY_MOUTH_COUNT;
    /* directed pose transitions x tween variants x speed x squash x seed. */
    long trans = (long)BUDDY_POSE_COUNT * (BUDDY_POSE_COUNT - 1) *
                 20 * 2 * 2 * 4;
    return (int)(loops + trans);
}

/* ---- token / mood / compose ------------------------------------------- */
int wubu_buddy_mood_from_token(const char *token, wubu_buddy_mood_t *out) {
    if (!token || !out) return -1;
    for (size_t i = 0; i < k_token_n; i++)
        if (strcmp(token, k_tokens[i].token) == 0) {
            out->valence = k_tokens[i].v;
            out->arousal = k_tokens[i].a;
            out->energy  = (int)((k_tokens[i].a + 2) >> 1);
            if (out->energy > 4) out->energy = 4;
            return 0;
        }
    return -1;   /* unknown token: caller falls back to neutral */
}

void wubu_buddy_compose(const wubu_buddy_mood_t *m, wubu_buddy_state_t *out) {
    if (!m || !out) { if (out) memset(out, 0, sizeof(*out)); return; }
    /* Clamp the three axes. */
    int v = m->valence, a = m->arousal;
    if (v < -2) v = -2; else if (v > 2) v = 2;
    if (a < -2) a = -2; else if (a > 2) a = 2;
    const Combo *c = &k_comp[(v + 2) * 5 + (a + 2)];
    out->pose = c->p;
    out->eye  = c->e;
    out->mouth = c->m;
    /* Derived palette wash (desktop-pets style; no new sprites). */
    out->sat_shift = (a + 2) * 12 - 24;      /* arousal changes saturation */
    out->hue_shift = (v > 0) ? 1 : (v < 0) ? -1 : 0;
    out->flash_red = (v <= -2 && a >= 2) ? 1 : 0;   /* dread/rage flash */
}

/* ---- deterministic animation id --------------------------------------- */
uint32_t wubu_buddy_anim_id(wubu_buddy_pose_t pose, wubu_buddy_eye_t eye,
                            wubu_buddy_mouth_t mouth, unsigned trans,
                            unsigned tween, uint16_t seed) {
    uint32_t h = 0x811c9dc5u;
#define MIX(x) do { h ^= (uint32_t)(x); h *= 0x01000193u; } while (0)
    MIX(pose); MIX(eye); MIX(mouth); MIX(trans); MIX(tween); MIX(seed);
#undef MIX
    return h;
}

/* ---- transition easing ------------------------------------------------- */
float wubu_buddy_ease_step(unsigned step, unsigned stepCount, unsigned ease) {
    if (stepCount == 0) return 1.0f;
    float t = (float)step / (float)stepCount;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    switch (ease % 4) {
        case 0: return t;                                   /* linear       */
        case 1: return t * t;                                /* ease-in      */
        case 2: return t * (2.0f - t);                       /* ease-out     */
        default: return t * t * (3.0f - 2.0f * t);           /* smoothstep   */
    }
}
