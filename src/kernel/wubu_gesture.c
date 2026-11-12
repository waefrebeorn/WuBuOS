/*
 * wubu_gesture.c -- multitouch gesture recognition.
 *
 * See wubu_gesture.h for the rules.  The engine is pure logic over contact
 * reports; the evdev adapter at the bottom is the only part that touches the
 * filesystem, and it degrades to "no touchpad" rather than failing.
 */

#include "../egojar.h"
#include "wubu_gesture.h"

#include <math.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>

#include <linux/input.h>

/* -std=c11 does not expose M_PI (it is a POSIX/GNU extension). */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* linux/input.h gives the event codes but not the bitmap helpers. */
#ifndef BIT_WORD
#define BIT_WORD(b)   ((b) / (8 * sizeof(unsigned long)))
#endif
#ifndef test_bit
#define test_bit(b, arr) (((arr)[BIT_WORD(b)] >> ((b) % (8 * sizeof(unsigned long)))) & 1UL)
#endif

/* Monotonic millisecond clock for gesture timing. */
static uint32_t clock_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

/* Forward declarations: the recogniser and the evdev adapter call each other
 * through the contact feed. */
static void evaluate_lift(uint32_t now_ms);
static void clear_participants(const int *idx, int n);

/* -- Tunables -------------------------------------------------------- */

#define TAP_MAX_TRAVEL_PX   12.0f
#define TAP_MAX_MS          400u
#define DOUBLE_TAP_MAX_MS   350u
#define DOUBLE_TAP_MAX_PX   24.0f
#define SWIPE_MIN_PX        60.0f
#define SWIPE_DOMINANCE     0.80f   /* dominant axis carries >= 80 % */
#define PINCH_MIN_RATIO     0.20f     /* 20 % separation change */
#define ROTATE_MIN_DEG      25.0f

#define SWIPE_MIN_CONTACTS  2

/* -- Contact state --------------------------------------------------- */

typedef struct {
    bool   down;
    int    x, y;
    int    sx, sy;        /* position at touch down */
    uint32_t down_ms;
    bool   moved;         /* exceeded tap travel */
} GContact;

static GContact g_c[WUBU_GESTURE_MAX_CONTACTS];
static int      g_contact_count;

/* Gesture candidates accumulated while contacts are down. */
static float g_start_sep;       /* two-contact separation at down */
static float g_start_bearing;   /* two-contact bearing at down */
static bool  g_have_sep;

/* Live values, updated on every move so surfaces can show progress. */
static float g_live_pinch = 1.0f;
static float g_live_rotate = 0.0f;

/* Last completed tap, for double-tap detection. */
static bool    g_last_was_tap;
static uint32_t g_last_tap_ms;
static int     g_last_tap_x, g_last_tap_y;

/* Pending gesture, set when contacts all lift and poll() reports it. */
static WubuGesture g_pending;
static bool        g_pending_ready;

/* evdev adapter state. */
static int g_tp_fd = -1;
static int g_mt_slot;                 /* current MT slot */
static int g_slot_x[WUBU_GESTURE_MAX_CONTACTS];
static int g_slot_y[WUBU_GESTURE_MAX_CONTACTS];
static int g_abs_min_x, g_abs_max_x, g_abs_min_y, g_abs_max_y;

void wubu_gesture_init(void) {
    memset(g_c, 0, sizeof(g_c));
    g_contact_count = 0;
    g_have_sep = false;
    g_live_pinch = 1.0f;
    g_live_rotate = 0.0f;
    g_last_was_tap = false;
    g_pending = WUBU_GESTURE_NONE;
    g_pending_ready = false;
    g_tp_fd = -1;
    g_mt_slot = 0;
}

void wubu_gesture_reset(void) {
    wubu_gesture_init();
}

/* -- Geometry helpers ------------------------------------------------ */

static float dist(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    return sqrtf(dx * dx + dy * dy);
}

static float bearing_deg(float ax, float ay, float bx, float by) {
    return atan2f(by - ay, bx - ax) * 180.0f / (float)M_PI;
}

static float norm_deg(float d) {
    while (d > 180.0f)  d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

/* -- Contact bookkeeping --------------------------------------------- */

static GContact *find_contact(int id) {
    if (id < 0 || id >= WUBU_GESTURE_MAX_CONTACTS) return NULL;
    return &g_c[id];
}

/* Order two contacts by slot index so the pinch baseline and the lift
 * evaluation see the same pair orientation. Without this a horizontal pair
 * reads as a 180-degree rotation depending on which finger landed last. */
static void order_pair(GContact *a, GContact *b, GContact **lo, GContact **hi) {
    if (a < b) { *lo = a; *hi = b; }
    else       { *lo = b; *hi = a; }
}

void wubu_gesture_contact(int id, int x, int y, bool down, uint32_t now_ms) {
    GContact *c = find_contact(id);
    if (!c) return;

    if (down && !c->down) {
        /* Touch down. */
        c->down = true;
        c->x = c->sx = x;
        c->y = c->sy = y;
        c->down_ms = now_ms;
        c->moved = false;
        g_contact_count++;

        /* Two-contact baseline for pinch/rotate. */
        if (g_contact_count == 2) {
            GContact *other = NULL;
            for (int i = 0; i < WUBU_GESTURE_MAX_CONTACTS; i++) {
                if (&g_c[i] != c && g_c[i].down) { other = &g_c[i]; break; }
            }
            if (other) {
                GContact *lo, *hi;
                order_pair(c, other, &lo, &hi);
                g_start_sep = dist((float)lo->x, (float)lo->y,
                                   (float)hi->x, (float)hi->y);
                g_start_bearing = bearing_deg((float)lo->x, (float)lo->y,
                                              (float)hi->x, (float)hi->y);
                g_have_sep = (g_start_sep > 1.0f);
                g_live_pinch = 1.0f;
                g_live_rotate = 0.0f;
            }
        }
    } else if (down && c->down) {
        /* Move. */
        c->x = x;
        c->y = y;
        if (dist((float)x, (float)y, (float)c->sx, (float)c->sy) > TAP_MAX_TRAVEL_PX)
            c->moved = true;

        /* Keep the live pinch/rotate values current for surfaces that render
         * progress while the gesture is in flight. */
        if (g_contact_count == 2 && g_have_sep) {
            GContact *other = NULL;
            for (int i = 0; i < WUBU_GESTURE_MAX_CONTACTS; i++) {
                if (&g_c[i] != c && g_c[i].down) { other = &g_c[i]; break; }
            }
            if (other) {
                GContact *lo, *hi;
                order_pair(c, other, &lo, &hi);
                float sep = dist((float)lo->x, (float)lo->y,
                                 (float)hi->x, (float)hi->y);
                float bear = bearing_deg((float)lo->x, (float)lo->y,
                                         (float)hi->x, (float)hi->y);
                g_live_pinch  = (g_start_sep > 0.0f) ? sep / g_start_sep : 1.0f;
                g_live_rotate = norm_deg(bear - g_start_bearing);
            }
        }
    } else if (!down && c->down) {
        /* Lift. The release report still carries the final position, so take
         * it before evaluating -- otherwise the last move position is used
         * and a fast flick loses its final step. */
        c->x = x;
        c->y = y;
        c->down = false;
        if (g_contact_count > 0) g_contact_count--;

        /* When the last contact lifts, evaluate the gesture. */
        if (g_contact_count == 0) {
            g_live_pinch = 1.0f;
            g_live_rotate = 0.0f;
            evaluate_lift(now_ms);
        }
    }
}

/* -- Recognition ----------------------------------------------------- */

static void evaluate_lift(uint32_t now_ms) {
    g_pending = WUBU_GESTURE_NONE;
    g_pending_ready = true;          /* always emit something (maybe NONE) */

    /* Collect the contacts that just lifted (down == false, but we still have
     * their last positions).  We kept the last two in a small local set. */
    int idx[WUBU_GESTURE_MAX_CONTACTS];
    int n = 0;
    for (int i = 0; i < WUBU_GESTURE_MAX_CONTACTS; i++) {
        /* A contact that participated: it has a valid down_ms and is now up.
         * g_c[i].down is false for all of them at this point, so track
         * participation with a sentinel: down_ms != 0. */
        if (g_c[i].down_ms != 0) idx[n++] = i;
    }
    if (n == 0) return;

    /* Two-contact gestures: pinch or rotate. */
    if (n >= 2 && g_have_sep) {
        GContact *a = &g_c[idx[0]];
        GContact *b = &g_c[idx[1]];
        float sep = dist((float)a->x, (float)a->y, (float)b->x, (float)b->y);
        float bear = bearing_deg((float)a->x, (float)a->y,
                                 (float)b->x, (float)b->y);

        float ratio = (g_start_sep > 0.0f) ? (sep / g_start_sep - 1.0f) : 0.0f;
        float d_bear = norm_deg(bear - g_start_bearing);

        if (fabsf(ratio) >= PINCH_MIN_RATIO) {
            g_pending = (ratio > 0.0f) ? WUBU_GESTURE_PINCH_OUT
                                       : WUBU_GESTURE_PINCH_IN;
            clear_participants(idx, n);
            return;
        }
        if (fabsf(d_bear) >= ROTATE_MIN_DEG) {
            g_pending = (d_bear > 0.0f) ? WUBU_GESTURE_ROTATE_CW
                                        : WUBU_GESTURE_ROTATE_CCW;
            clear_participants(idx, n);
            return;
        }
    }

    /* Single-contact (or coalesced) swipe. */
    if (n >= SWIPE_MIN_CONTACTS) {
        float dx = 0.0f, dy = 0.0f;
        for (int i = 0; i < n; i++) {
            GContact *c = &g_c[idx[i]];
            dx += (float)(c->x - c->sx);
            dy += (float)(c->y - c->sy);
        }
        dx /= (float)n;
        dy /= (float)n;

        float ax = fabsf(dx), ay = fabsf(dy);
        float total = dist(0.0f, 0.0f, dx, dy);
        if (total >= SWIPE_MIN_PX &&
            (ax > ay ? ax : ay) >= SWIPE_DOMINANCE * total) {
            if (ax >= ay)
                g_pending = (dx > 0.0f) ? WUBU_GESTURE_SWIPE_RIGHT
                                        : WUBU_GESTURE_SWIPE_LEFT;
            else
                g_pending = (dy > 0.0f) ? WUBU_GESTURE_SWIPE_DOWN
                                        : WUBU_GESTURE_SWIPE_UP;
            clear_participants(idx, n);
            return;
        }
    }

    /* Tap: the primary contact barely moved and was brief. */
    GContact *c = &g_c[idx[0]];
    float travel = dist((float)c->x, (float)c->y, (float)c->sx, (float)c->sy);
    uint32_t dur = now_ms - c->down_ms;
    if (travel <= TAP_MAX_TRAVEL_PX && dur <= TAP_MAX_MS) {
        if (g_last_was_tap &&
            (now_ms - g_last_tap_ms) <= DOUBLE_TAP_MAX_MS &&
            dist((float)c->x, (float)c->y,
                 (float)g_last_tap_x, (float)g_last_tap_y) <= DOUBLE_TAP_MAX_PX) {
            g_pending = WUBU_GESTURE_DOUBLE_TAP;
            g_last_was_tap = false;
        } else {
            g_pending = WUBU_GESTURE_TAP;
            g_last_was_tap = true;
            g_last_tap_ms = now_ms;
            g_last_tap_x = c->x;
            g_last_tap_y = c->y;
        }
    } else {
        g_last_was_tap = false;
    }

    clear_participants(idx, n);
}

static void clear_participants(const int *idx, int n) {
    for (int i = 0; i < n; i++)
        g_c[idx[i]].down_ms = 0;     /* sentinel: no longer a participant */
}

/* -- Polling --------------------------------------------------------- */

int wubu_gesture_poll(WubuGesture *out) {
    if (!g_pending_ready) return 0;
    g_pending_ready = false;
    if (out) *out = g_pending;
    return 1;
}

int wubu_gesture_contact_count(void) { return g_contact_count; }

float wubu_gesture_pinch_scale(void) { return g_live_pinch; }

float wubu_gesture_rotation_deg(void) { return g_live_rotate; }

/* -- evdev adapter --------------------------------------------------- */

int wubu_gesture_open_touchpad(void) {
    DIR *d = opendir("/dev/input");
    if (!d) return -1;

    char path[256];
    struct dirent *e;
    int found = -1;

    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "event", 5) != 0) continue;
        snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        /* A multitouch touchpad advertises ABS_MT_POSITION_X/Y. */
        unsigned long absbits[BIT_WORD(ABS_CNT)] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) < 0) {
            close(fd);
            continue;
        }
        bool has_mt_x = test_bit(ABS_MT_POSITION_X, absbits);
        bool has_mt_y = test_bit(ABS_MT_POSITION_Y, absbits);
        if (!has_mt_x || !has_mt_y) { close(fd); continue; }

        /* Range, for normalisation. */
        struct input_absinfo ai;
        g_abs_min_x = g_abs_min_y = 0;
        g_abs_max_x = g_abs_max_y = 1;
        if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ai) == 0) {
            g_abs_min_x = ai.minimum; g_abs_max_x = ai.maximum;
        }
        if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ai) == 0) {
            g_abs_min_y = ai.minimum; g_abs_max_y = ai.maximum;
        }

        found = fd;
        break;
    }
    closedir(d);
    return found;
}

static int scale_x(int v) {
    int span = g_abs_max_x - g_abs_min_x;
    if (span <= 0) span = 1;
    return (v - g_abs_min_x) * 1000 / span;
}

static int scale_y(int v) {
    int span = g_abs_max_y - g_abs_min_y;
    if (span <= 0) span = 1;
    return (v - g_abs_min_y) * 1000 / span;
}

int wubu_gesture_poll_fd(int fd, WubuGesture *out) {
    struct input_event ev;
    uint32_t now = (uint32_t)(clock_ms());

    while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_ABS) {
            switch (ev.code) {
                case ABS_MT_SLOT:
                    g_mt_slot = ev.value;
                    break;
                case ABS_MT_TRACKING_ID:
                    if (g_mt_slot < WUBU_GESTURE_MAX_CONTACTS) {
                        int id = g_mt_slot;
                        if (ev.value >= 0) {
                            if (!g_c[id].down)
                                wubu_gesture_contact(id, g_slot_x[id],
                                                     g_slot_y[id], true, now);
                            else
                                g_c[id].down = true;   /* re-contact */
                        } else {
                            wubu_gesture_contact(id, g_slot_x[id],
                                                 g_slot_y[id], false, now);
                        }
                    }
                    break;
                case ABS_MT_POSITION_X:
                    if (g_mt_slot < WUBU_GESTURE_MAX_CONTACTS)
                        g_slot_x[g_mt_slot] = scale_x(ev.value);
                    break;
                case ABS_MT_POSITION_Y:
                    if (g_mt_slot < WUBU_GESTURE_MAX_CONTACTS)
                        g_slot_y[g_mt_slot] = scale_y(ev.value);
                    break;
                default: break;
            }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            /* A report boundary: report positions for active slots. */
            for (int i = 0; i < WUBU_GESTURE_MAX_CONTACTS; i++) {
                if (g_c[i].down) {
                    wubu_gesture_contact(i, g_slot_x[i], g_slot_y[i],
                                         true, now);
                }
            }
        }
    }
    return wubu_gesture_poll(out);
}

int wubu_gesture_tick(WubuGesture *out) {
    if (g_tp_fd < 0) {
        g_tp_fd = wubu_gesture_open_touchpad();
        if (g_tp_fd < 0) return 0;
    }
    return wubu_gesture_poll_fd(g_tp_fd, out);
}
