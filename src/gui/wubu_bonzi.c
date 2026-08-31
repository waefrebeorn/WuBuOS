/*
 * wubu_bonzi.c -- WuBuOS WuBu Buddy desktop mascot (AGI gateway).
 *
 * WuBu-style: a purple companion that sits on the desktop as a chrome-less
 * desktop element (drawn by the WM render loop above windows, like the a11y
 * cluster). Drawn entirely with vbe_* pixel primitives (house discipline,
 * no image assets):
 *   - rounded purple body with lighter belly + squash/stretch on idle bob
 *   - white eyes that blink with Anticipation -> Close -> Open -> Relax stages
 *   - smile arc that lifts when alert, flattens when blinking
 *   - arms at the sides
 *   - a speech bubble greeting that fades in/out
 *   - idle bob animation (vertical cosine oscillation) + soft drop shadow
 *
 * Animation follows Disney's 12 principles:
 *   1. Squash & stretch      -- body squashes at bob extremes
 *   2. Anticipation          -- eyes squeeze before closing
 *   3. Slow in / slow out    -- smoothstep + cosine easing everywhere
 *   4. Stretch & squash      -- vertical scale tracks the bob
 *   5. Exaggeration          -- 3px bob (not 1px) for visibility
 *   6. Follow-through        -- 20ms post-blink relaxation
 *
 * Clicking the buddy (or its bubble) opens the AGI HolyD terminal near it --
 * the GUI is the gateway to the AGI.
 *
 * C11, opaque API (wubu_bonzi.h), no god headers. Self-contained drawing.
 */

#include "wubu_bonzi.h"
#include "../kernel/vbe.h"
#ifdef WUBU_NO_LIBM
#  include "../kernel/wubu_math.h"  /* provides M_PI, fabsf, cosf, etc. */
#else
#  include <math.h>                 /* provides fabsf, cosf, etc. */
   /* M_PI is a GNU extension; define fallback if unavailable. */
#  ifndef M_PI
#    define M_PI 3.14159265358979323846
#  endif
#endif
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_holyd_term.h"

#include <string.h>
#include <stdio.h>

/* -- Palette (fixed; the buddy is a brand element, not theme-churnable) -- */
#define BONZI_BODY        0xFF7B3FA0  /* friendly purple */
#define BONZI_BODY_DARK   0xFF5E2E7C  /* shading */
#define BONZI_OUTLINE     0xFF3D1F52  /* crisp silhouette rim */
#define BONZI_BELLY       0xFFC9A6E8  /* lighter belly */
#define BONZI_EYE_WHITE   0xFFFFFFFF
#define BONZI_EYE         0xFF1A1A1A
#define BONZI_BLUSH       0xFFE8A0B8  /* cute cheeks */
#define BONZI_MOUTH       0xFF3B1E54
#define BONZI_BUBBLE      0xFFFFFDE7  /* speech bubble cream */
#define BONZI_BUBBLE_EDGE 0xFF8A7B4D
#define BONZI_TEXT        0xFF222222
#define BONZI_SHADOW      0x000000

/* -- Geometry --------------------------------------------------------- */
#define BONZI_W     64
#define BONZI_H     64
#define BONZI_R     22            /* body radius */
#define BONZI_CX    32            /* body center x in window */
#define BONZI_CY    36            /* body center y in window */
#define BONZI_BUB_W 180
#define BONZI_BUB_H 34
/* Bubble sits ABOVE the buddy (the default buddy spot is the bottom-left
 * corner; a left-side bubble would run off-screen).  Centered horizontally
 * over the buddy, clamped into view in draw().  180px fits 20 glyphs so
 * the greeting is never truncated ("Hi! I'm WuBu Buddy!" = 19 chars). */
#define BONZI_BUB_X ((BONZI_W - BONZI_BUB_W) / 2)
#define BONZI_BUB_Y (-BONZI_H - BONZI_BUB_H - 6)

/* -- Animation timing (theatrical: slow-in/slow-out, natural rhythms) --
   Classic Disney-style timing ratios (12-field @ 24fps reference):
     - Blink: 100ms total, with 20ms anticipation (pre-squeeze) +
       60ms close/open (split 50/50) + 20ms follow-through (post-relaxation)
     - Idle bob: 1.6s period (resting breathing rate for a living creature)
     - Speech bubble: fades in/out with the bob cycle */

#define BONZI_BLINK_CYCLE_MS  2600  /* idle between blinks (~2.6s) */
#define BONZI_BLINK_PRE_MS     20   /* anticipation: eyes squeeze before closing */
#define BONZI_BLINK_CLOSE_MS   60   /* eyes close+open (split 50/50) */
#define BONZI_BLINK_POST_MS    20   /* follow-through: post-blink relaxation */
#define BONZI_BLINK_TOTAL_MS  (BONZI_BLINK_PRE_MS + BONZI_BLINK_CLOSE_MS + BONZI_BLINK_POST_MS)  /* 100ms */

/* Idle bob: cosine-wave vertical oscillation with squash/stretch. */
#define BONZI_BOB_PERIOD_MS   1600
#define BONZI_BOB_AMPLITUDE     3    /* pixels of vertical travel */
#define BONZI_BOB_SQUASH     0.88f  /* body height scale at bob extremes */

/* Speech bubble: fades in ONCE over the first ~500ms after init, then
 * holds.  (Do NOT tie bubble alpha to the bob cycle — that made the
 * bubble strobe out and back in every bob period.)
 * Alpha is ~94% (240/255): a speech bubble is a SOLID comic-style balloon,
 * not a translucent panel — a 70-alpha blend over the dark wallpaper was
 * nearly invisible. */
#define BONZI_BUBBLE_FADE_MS  500
#define BONZI_BUBBLE_ALPHA    240.0f

/* -- Easing helpers (canonical Disney slow-in/slow-out) -- */

/* Smoothstep: classic cubic Hermite interpolation [0,1] -> [0,1]. */
static inline float smoothstep(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    return t * t * (3.f - 2.f * t);
}

/* Ease-out-cubic: fast start, slow finish (gravity-like). */
static inline float ease_out_cubic(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    float f = 1.f - t;
    return 1.f - f * f * f;
}

/* Cosine ping-pong oscillator in [0,1]. */
static inline float bob_wave(float phase) {
    return (1.f - cosf(phase * 2.f * (float)M_PI)) * 0.5f;
}

/* -- Sprite geometry helpers ------------------------------------------ */

static bool in_body(int dx, int dy) {
    /* Body: a squashed circle (ellipse) + two ear circles. */
    int ex = (dx * 8) / 10;   /* squash x to 80% -> wide body */
    if (ex * ex + dy * dy <= BONZI_R * BONZI_R) return true;
    /* ears: circles at top-left and top-right */
    int e1x = -BONZI_R + 4, e1y = -BONZI_R + 4;
    if ((dx - e1x) * (dx - e1x) + (dy - e1y) * (dy - e1y) <= 8 * 8) return true;
    int e2x = BONZI_R - 4,  e2y = -BONZI_R + 4;
    if ((dx - e2x) * (dx - e2x) + (dy - e2y) * (dy - e2y) <= 8 * 8) return true;
    return false;
}

static bool in_belly(int dx, int dy) {
    int ex = (dx * 8) / 10;
    return ex * ex + (dy + 4) * (dy + 4) <= (BONZI_R - 8) * (BONZI_R - 8);
}

/* Eye with variable openness for smooth blink.
 * eye_open: 1.0 = fully open, 0.0 = fully closed.
 * Uses arc-drawing to produce a natural eyelid that squeezes the eye. */
static void draw_eye_open(int cx, int cy, float eye_open) {
    if (eye_open > 0.95f) {
        /* Fully open: white + dark pupil with a sparkle highlight. */
        vbe_fill_circle(cx, cy, 4, BONZI_EYE_WHITE);
        vbe_fill_circle(cx + 1, cy, 2, BONZI_EYE);
        vbe_set_pixel(cx, cy - 1, BONZI_EYE_WHITE);   /* sparkle */
        vbe_set_pixel(cx + 1, cy - 1, BONZI_EYE_WHITE);
    } else {
        /* Partially or fully closed: draw a closing eyelid.
         * The lid height shrinks as eye_open goes 1->0. */
        int lid_h = (int)(eye_open * 4.0f);  /* 4px open -> 0px closed */
        if (lid_h < 1) lid_h = 1;
        vbe_fill_circle(cx, cy, 4, BONZI_EYE_WHITE);
        /* Dark upper lid descending over the eye. */
        vbe_fill_rect(cx - 4, cy - lid_h / 2, 8, lid_h, BONZI_EYE);
    }
}

/* -- State ------------------------------------------------------------ */

static bool g_bonzi_enabled = false;
static int  g_clock_ms = 0;       /* animation clock (ms) */
static int  g_bx = 0, g_by = 0;   /* mascot top-left on desktop */
static bool g_grab = false;       /* mouse press landed on the buddy */

/* Bubble text lives in fixed buffers so wubu_bonzi_set_bubble (Balloon
 * Help / context tips) can retarget it without dangling pointers. */
static char g_bubble_line1[40];
static char g_bubble_line2[40];

/* -- Animation computation --------------------------------------------
 * Single-source-of-truth: all frame parameters computed from g_clock_ms.
 * This makes the animation deterministic and stateless (no hidden state
 * between frames), which is critical for testability. */

typedef struct {
    float eye_open;     /* 1.0 = open, 0.0 = closed */
    float bob_y;        /* vertical offset from rest, in pixels */
    float bob_squash;   /* vertical scale factor (1.0 = neutral) */
    float bubble_alpha; /* 0.0 = invisible, 70 = fully faded in */
} BonziAnim;

static BonziAnim compute_animation(int clock_ms) {
    BonziAnim a = {1.0f, 0.0f, 1.0f, 0.0f};

    /* --- Idle bob: cosine-wave vertical oscillation with squash/stretch --
     * Bob rises at the top of the cycle (inhale), reaches peak, then
     * descends (exhale). Slow-in/slow-out via cosine easing. */
    float bob_phase = fmodf((float)clock_ms / (float)BONZI_BOB_PERIOD_MS, 1.0f);
    float bob = bob_wave(bob_phase);  /* 0 at bottom, 1 at top */
    a.bob_y = -smoothstep(bob) * (float)BONZI_BOB_AMPLITUDE;
    /* Squash at extremes (top/bottom of bob), stretch at midpoint. */
    float squash_factor = 1.0f - fabsf(0.5f - bob) * 2.0f;
    a.bob_squash = 1.0f - (1.0f - BONZI_BOB_SQUASH) * squash_factor;

    /* --- Blink: 4-Stage with anticipation + follow-through ---
     * Stage 0 (PRE, 20ms): eyes squeeze to 70% (anticipation)
     * Stage 1 (CLOSE 30ms): eyes close 1->0 (ease-out)
     * Stage 2 (OPEN 30ms):  eyes open 0->1 (ease-in)
     * Stage 3 (POST, 20ms): full relaxation (follow-through) */
    int t = clock_ms % BONZI_BLINK_CYCLE_MS;
    if (t < BONZI_BLINK_TOTAL_MS) {
        if (t < BONZI_BLINK_PRE_MS) {
            /* Anticipation: subtle squeeze. */
            float s = smoothstep((float)t / (float)BONZI_BLINK_PRE_MS);
            a.eye_open = 1.0f - s * 0.3f;
        } else if (t < BONZI_BLINK_PRE_MS + BONZI_BLINK_CLOSE_MS / 2) {
            /* Closing: ease-out (fast start, slow finish). */
            float inner = (float)(t - BONZI_BLINK_PRE_MS) / (float)(BONZI_BLINK_CLOSE_MS / 2);
            a.eye_open = 1.0f - ease_out_cubic(inner);
        } else if (t < BONZI_BLINK_PRE_MS + BONZI_BLINK_CLOSE_MS) {
            /* Opening: ease-in (slow start, fast finish). */
            float inner = (float)(t - BONZI_BLINK_PRE_MS - BONZI_BLINK_CLOSE_MS / 2) / (float)(BONZI_BLINK_CLOSE_MS / 2);
            a.eye_open = ease_out_cubic(inner);
        } else {
            /* Post-blink: full open (relaxation). */
            a.eye_open = 1.0f;
        }
    }

    /* --- Speech bubble: one-time fade-in, then hold ---
     * Fades in once over the first BONZI_BUBBLE_FADE_MS after init and
     * stays (a bob-tied alpha strobes every bob period — wrong). */
    if (clock_ms < BONZI_BUBBLE_FADE_MS)
        a.bubble_alpha = smoothstep((float)clock_ms / (float)BONZI_BUBBLE_FADE_MS)
                         * BONZI_BUBBLE_ALPHA;
    else
        a.bubble_alpha = BONZI_BUBBLE_ALPHA;

    return a;
}

/* -- Drawing ---------------------------------------------------------- */

void wubu_bonzi_draw(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_bonzi_enabled) return;

    BonziAnim a = compute_animation(g_clock_ms);
    int ox = g_bx;
    int oy = g_by + (int)a.bob_y;  /* smooth vertical bob */

    bool eyes_open = a.eye_open > 0.1f;
    int eye_offset = (int)((1.0f - a.eye_open) * 2.0f);  /* eyes drift up when closing */

    /* 1. Outline: crisp 1px dark rim around the body silhouette, painted
     *    BEFORE the shadow so the shadow (translucent) doesn't smear over it.
     *    Checks 8-neighbourhood for a fuller rim than 4-connectivity. */
    for (int dy = -BONZI_R - 8; dy <= BONZI_R + 8; dy++) {
        for (int dx = -BONZI_R - 8; dx <= BONZI_R + 8; dx++) {
            if (!in_body(dx, dy)) continue;
            static const int nb[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};
            for (int k = 0; k < 8; k++) {
                int nx = dx + nb[k][0], ny = dy + nb[k][1];
                if (in_body(nx, ny)) continue;          /* interior */
                int sx = ox + BONZI_CX + nx;
                int sy = oy + BONZI_CY + (int)((float)ny * a.bob_squash);
                if (sx >= 0 && sy >= 0 && sx < fb_w && sy < fb_h)
                    vbe_set_pixel(sx, sy, BONZI_OUTLINE);
            }
        }
    }

    /* 2. Soft drop shadow (translucent so the outline below stays visible). */
    vbe_blend_rect(ox + 3, oy + 3, BONZI_W, BONZI_H, BONZI_SHADOW, 50);
    /* Body: iterate the sprite box, apply squash/stretch on Y. */
    for (int dy = -BONZI_R - 8; dy <= BONZI_R + 8; dy++) {
        int sdy = (int)((float)dy * a.bob_squash);
        for (int dx = -BONZI_R - 8; dx <= BONZI_R + 8; dx++) {
            int sx = ox + BONZI_CX + dx, sy = oy + BONZI_CY + sdy;
            if (sx < 0 || sy < 0 || sx >= fb_w || sy >= fb_h) continue;
            if (in_body(dx, dy)) {
                if (in_belly(dx, dy)) vbe_set_pixel(sx, sy, BONZI_BELLY);
                else if (dy > BONZI_R - 6) vbe_set_pixel(sx, sy, BONZI_BODY_DARK);
                else vbe_set_pixel(sx, sy, BONZI_BODY);
            }
        }
    }

    /* Face. Eyes at y=-8, x=+/-8 relative to center.
     * Eyes drift up slightly during blink (anticipation of closure). */
    int ex = ox + BONZI_CX, ey = oy + BONZI_CY + eye_offset;
    draw_eye_open(ex - 8, ey - 8, a.eye_open);
    draw_eye_open(ex + 8, ey - 8, a.eye_open);

    /* Mouth: smile that lifts when alert (eyes open),
     * flattens during blink. */
    int mouth_y = ey + 4 + (eyes_open ? 0 : 4);
    vbe_fill_rect(ex - 5, mouth_y, 10, 2, BONZI_MOUTH);
    vbe_fill_rect(ex - 2, mouth_y + 2, 4, 1, BONZI_MOUTH);

    /* Blush cheeks: visible pink stripes under/around the eyes (the tiny
     * 4x3 was too faint to read at 4x zoom). */
    vbe_fill_rect(ex - 14, ey + 1, 5, 4, BONZI_BLUSH);
    vbe_fill_rect(ex + 9,  ey + 1, 5, 4, BONZI_BLUSH);
    vbe_blend_rect(ex - 12, ey, 3, 3, BONZI_BLUSH, 180);
    vbe_blend_rect(ex + 11, ey, 3, 3, BONZI_BLUSH, 180);

    /* Nose: tiny dot. */
    vbe_fill_rect(ex, ey - 1, 2, 2, BONZI_BODY_DARK);

    /* Arms: at the sides. */
    vbe_fill_rect(ex - BONZI_R - 3, ey + 4, 4, 10, BONZI_BODY);
    vbe_fill_rect(ex + BONZI_R - 1, ey + 4, 4, 10, BONZI_BODY);
    vbe_fill_circle(ex - BONZI_R - 1, ey + 14, 3, BONZI_BODY_DARK);
    vbe_fill_circle(ex + BONZI_R + 1, ey + 14, 3, BONZI_BODY_DARK);

    /* Speech bubble: above the buddy, clamped on-screen, tail pointing
     * DOWN toward the buddy's head (real alpha blend). */
    if (a.bubble_alpha > 5.0f) {
        int bx = ox + BONZI_BUB_X, by = oy + BONZI_BUB_Y;
        if (bx < 4) bx = 4;                        /* keep on-screen */
        if (bx + BONZI_BUB_W > fb_w - 4) bx = fb_w - 4 - BONZI_BUB_W;
        int alpha = (int)a.bubble_alpha;
        vbe_blend_rect(bx, by, BONZI_BUB_W, BONZI_BUB_H, BONZI_BUBBLE, alpha);
        vbe_rect_rounded(bx, by, BONZI_BUB_W, BONZI_BUB_H, 6, BONZI_BUBBLE_EDGE);
        /* Tail: small downward triangle from the bubble's bottom center
         * toward the buddy's head. */
        int tail_cx = bx + BONZI_BUB_W / 2;
        vbe_blend_rect(tail_cx - 4, by + BONZI_BUB_H - 1, 8, 2, BONZI_BUBBLE, alpha);
        vbe_blend_rect(tail_cx - 2, by + BONZI_BUB_H + 1, 4, 2, BONZI_BUBBLE, alpha);
        /* Bubble text (truncate to the 180px bubble: ~20 glyphs/line).
         * Truncate into LOCAL copies — never mutate the stored strings. */
        char t1[24], t2[24];
        strncpy(t1, g_bubble_line1, 20); t1[20] = '\0';
        strncpy(t2, g_bubble_line2, 20); t2[20] = '\0';
        vbe_draw_text(bx + 8, by + 5, t1, BONZI_TEXT, 1);
        vbe_draw_text(bx + 8, by + 17, t2, BONZI_TEXT, 1);
    }
}

/* -- Mouse ------------------------------------------------------------ */

bool wubu_bonzi_mouse(int x, int y, int btn, int kind) {
    (void)btn;
    if (!g_bonzi_enabled) return false;
    int in_body_box = (x >= g_bx && x < g_bx + BONZI_W &&
                       y >= g_by && y < g_by + BONZI_H);
    int in_bubble = (x >= g_bx + BONZI_BUB_X && x < g_bx + BONZI_BUB_X + BONZI_BUB_W &&
                     y >= g_by + BONZI_BUB_Y && y < g_by + BONZI_BUB_Y + BONZI_BUB_H);
    bool over = in_body_box || in_bubble;

    if (kind == 1) {
        /* Press: grab only if it lands on the buddy.  A press elsewhere
         * must NOT be claimed (an icon drag crossing the buddy's box on a
         * later move would otherwise be swallowed). */
        if (!over) return false;
        g_grab = true;
        return true;
    }
    if (kind == 2) {
        bool was_grab = g_grab;
        g_grab = false;
        if (over && was_grab) wubu_bonzi_open_agi();   /* click */
        return over && was_grab;
    }
    /* Move: consume only while a grab started on the buddy. */
    return g_grab;
}

/* -- Balloon Help (System 7 lesson): retarget the bubble text ---------- */

void wubu_bonzi_set_bubble(const char *l1, const char *l2) {
    if (l1) { strncpy(g_bubble_line1, l1, sizeof(g_bubble_line1) - 1);
              g_bubble_line1[sizeof(g_bubble_line1) - 1] = '\0'; }
    if (l2) { strncpy(g_bubble_line2, l2, sizeof(g_bubble_line2) - 1);
              g_bubble_line2[sizeof(g_bubble_line2) - 1] = '\0'; }
}

/* -- AGI gateway ------------------------------------------------------ */

void wubu_bonzi_open_agi(void) {
    DosGuiWindow *w = dosgui_wm_spawn_holyd_term(280, 180, 520, 380);
    if (w) dosgui_wm_set_focus(w);
}

/* -- Lifecycle -------------------------------------------------------- */

void wubu_bonzi_set_enabled(bool on) {
    g_bonzi_enabled = on;
    if (!on) g_grab = false;
}
bool wubu_bonzi_is_enabled(void)     { return g_bonzi_enabled; }

void wubu_bonzi_tick(int dt_ms) {
    g_clock_ms += dt_ms;
    if (g_clock_ms > 1000000) g_clock_ms %= 1000000;
}

bool wubu_bonzi_init(int x, int y) {
    g_bx = x; g_by = y;
    g_bonzi_enabled = true;
    g_grab = false;
    g_clock_ms = 0;
    wubu_bonzi_set_bubble("Hi! I'm WuBu Buddy!",
                          "Click me for the AGI!");
    return true;
}

int wubu_bonzi_x(void) { return g_bx; }
int wubu_bonzi_y(void) { return g_by; }
int wubu_bonzi_w(void) { return BONZI_W; }
int wubu_bonzi_h(void) { return BONZI_H; }