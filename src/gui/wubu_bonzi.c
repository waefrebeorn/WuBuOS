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
 * Clicking the buddy (or its bubble) opens the AGI HolyC terminal near it --
 * the GUI is the gateway to the AGI.
 *
 * C11, opaque API (wubu_bonzi.h), no god headers. Self-contained drawing.
 */

#include "wubu_bonzi.h"
#include "wubu_mascot.h"
#include "wubu_buddy_sprite.h"
#include "wubu_buddy_states.h"
#include "wubu_buddy_states2.h"
#include "wubu_buddy_states3.h"
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
#include "../gui/wm.h"
#include "shell/wm_holyc_term.h"

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
#define BONZI_W     WUBU_BUDDY_SPRITE_SIZE
#define BONZI_H     WUBU_BUDDY_SPRITE_SIZE
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
static int  g_state = WUBU_BUDDY_STATE_DEFAULT;

void wubu_bonzi_set_state(int state) {
    if (state < 0 || state >= WUBU_BUDDY_STATE_COUNT) state = WUBU_BUDDY_STATE_DEFAULT;
    g_state = state;
}
int wubu_bonzi_state(void) { return g_state; }

static const unsigned char *buddy_state_rgba(void) {
    switch (g_state) {
    case WUBU_BUDDY_STATE_WORK:    return BUDDY_STATE_WORK;
    case WUBU_BUDDY_STATE_ERROR:   return BUDDY_STATE_ERROR;
    case WUBU_BUDDY_STATE_SUCCESS: return BUDDY_STATE_SUCCESS;
    case WUBU_BUDDY_STATE_IDLE:    return BUDDY_STATE_IDLE;
    case WUBU_BUDDY_STATE_OVERHEAT: return BUDDY_STATE_OVERHEAT;
    case WUBU_BUDDY_STATE_BUSY:    return BUDDY_STATE_BUSY;
    case WUBU_BUDDY_STATE_RELAXED: return BUDDY_STATE_RELAXED;
    case WUBU_BUDDY_STATE_SURF:    return BUDDY_STATE_SURF;
    case WUBU_BUDDY_STATE_RACE:    return BUDDY_STATE_RACE;
    case WUBU_BUDDY_STATE_READ:    return BUDDY_STATE_READ;
    default:                       return wubu_buddy_rgba;
    }
}
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

/* Solid body under the baked clip art.  The clipart's white fur was encoded
 * as transparent (mascot_encode treats near-white as background), so the
 * buddy rendered as a faint ghost with no legs.  Drawing the body opaquely
 * first gives the sprite something solid to sit on; the sprite's cap,
 * scarf, eye and outlines still show through. */
static void buddy_draw_body(int ox, int oy) {
    const uint32_t fur   = 0xFFFFFFFF;   /* white fur */
    const uint32_t belly = 0xFFF2F2FA;   /* near-white belly */
    const uint32_t ink   = 0xFF1A1A2E;   /* dark outline */

    /* feet + legs: the part the old sprite lost entirely */
    vbe_fill_circle(ox + 34, oy + 84, 8, fur);
    vbe_fill_circle(ox + 62, oy + 84, 8, fur);
    vbe_ellipse(ox + 34, oy + 84, 8, 8, ink);
    vbe_ellipse(ox + 62, oy + 84, 8, 8, ink);
    vbe_fill_circle(ox + 38, oy + 74, 7, fur);
    vbe_fill_circle(ox + 58, oy + 74, 7, fur);

    /* body + belly */
    vbe_fill_circle(ox + 48, oy + 62, 22, fur);
    vbe_ellipse(ox + 48, oy + 62, 22, 22, ink);
    vbe_fill_circle(ox + 48, oy + 68, 12, belly);

    /* arms */
    vbe_fill_circle(ox + 26, oy + 58, 8, fur);
    vbe_fill_circle(ox + 70, oy + 58, 8, fur);
    vbe_ellipse(ox + 26, oy + 58, 8, 8, ink);
    vbe_ellipse(ox + 70, oy + 58, 8, 8, ink);

    /* ears */
    vbe_fill_rect_rounded(ox + 30, oy + 2, 12, 22, 6, fur);
    vbe_fill_rect_rounded(ox + 54, oy + 2, 12, 22, 6, fur);
    vbe_rect_rounded(ox + 30, oy + 2, 12, 22, 6, ink);
    vbe_rect_rounded(ox + 54, oy + 2, 12, 22, 6, ink);

    /* head */
    vbe_fill_circle(ox + 48, oy + 32, 18, fur);
    vbe_ellipse(ox + 48, oy + 32, 18, 18, ink);
}

void wubu_bonzi_draw(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_bonzi_enabled) return;

    BonziAnim a = compute_animation(g_clock_ms);
    int ox = g_bx;
    int oy = g_by + (int)a.bob_y;  /* smooth vertical bob */

    /* The body first (opaque), then the baked clipart over it. */
    buddy_draw_body(ox, oy);
    wubu_mascot_blit_rgba(ox, oy, buddy_state_rgba(), BUDDY_STATE_SIZE);

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
    DosGuiWindow *w = wm_spawn_holyc_term(280, 180, 520, 380);
    if (w) wm_set_focus(w);
}

/* -- Lifecycle -------------------------------------------------------- */

void wubu_bonzi_set_enabled(bool on) {
    g_bonzi_enabled = on;
    if (!on) g_grab = false;
}
bool wubu_bonzi_is_enabled(void)     { return g_bonzi_enabled; }

/* Activity feed: the caller (the frame loop) reports input and work so the
 * buddy shows the pose that matches the machine -- busy while things are
 * happening, asleep once it has been quiet a while. Explicit moods set by
 * the agent (SUCCESS/ERROR) hold until the next activity. */
static unsigned long long g_last_activity_ms;
static unsigned long long g_last_mood_ms;
static int g_mood_override;          /* WORK/SUCCESS/ERROR, -1 = none */

void wubu_bonzi_activity(void) {
    g_last_activity_ms = g_clock_ms;
    if (g_mood_override == WUBU_BUDDY_STATE_WORK) return;
    g_state = WUBU_BUDDY_STATE_WORK;
    g_last_mood_ms = g_clock_ms;
    g_mood_override = WUBU_BUDDY_STATE_WORK;
}

void wubu_bonzi_mood(int state) {
    if (state != WUBU_BUDDY_STATE_SUCCESS && state != WUBU_BUDDY_STATE_ERROR)
        return;
    g_state = state;
    g_mood_override = state;
    g_last_mood_ms = g_clock_ms;
}

#define BUDDY_IDLE_AFTER_MS 6000

void wubu_bonzi_tick(int dt_ms) {
    g_clock_ms += dt_ms;
    if (g_clock_ms > 1000000) g_clock_ms %= 1000000;

    /* An explicit mood holds for a moment, then the buddy goes back to
     * reflecting activity. */
    if (g_mood_override == WUBU_BUDDY_STATE_SUCCESS ||
        g_mood_override == WUBU_BUDDY_STATE_ERROR) {
        if (g_clock_ms - g_last_mood_ms > 4000) {
            g_mood_override = -1;
            g_state = WUBU_BUDDY_STATE_DEFAULT;
        }
        return;
    }
    if (g_clock_ms - g_last_activity_ms > BUDDY_IDLE_AFTER_MS &&
        g_state != WUBU_BUDDY_STATE_IDLE) {
        g_state = WUBU_BUDDY_STATE_IDLE;
    }
}

bool wubu_bonzi_init(int x, int y) {
    g_bx = x; g_by = y;
    g_bonzi_enabled = true;
    g_grab = false;
    g_clock_ms = 0;
    g_last_activity_ms = 0;
    g_last_mood_ms = 0;
    g_mood_override = -1;
    wubu_bonzi_set_bubble("Hi! I'm WuBu Buddy!",
                          "Click me for the AGI!");
    return true;
}

int wubu_bonzi_x(void) { return g_bx; }
int wubu_bonzi_y(void) { return g_by; }
int wubu_bonzi_w(void) { return BONZI_W; }
int wubu_bonzi_h(void) { return BONZI_H; }
