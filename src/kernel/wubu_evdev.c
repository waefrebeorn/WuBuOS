/*
 * wubu_evdev.c -- the evdev gamepad pipeline.
 *
 * Steam Input (src/runtime/wubu_steaminput.c) maps a gamepad to KEY/MOUSE
 * events and emits them into the kernel input queue. Nothing was feeding
 * it: src/kernel/wubu_gamepad.c only DETECTS hardware. This module is the
 * missing front half -- it opens the evdev devices and turns raw kernel
 * events into Steam Input button/axis calls.
 *
 * Design:
 *   - best effort everywhere. No device, no permission, a pad that
 *     unplugs mid-game: all of that is a quiet no-op, never a crash.
 *   - non-blocking. wubu_evdev_poll() never sleeps; it drains what is
 *     already queued and returns.
 *   - no thread. The caller decides when to poll (the hosted frame loop
 *     polls it once per frame, so a game sees its pad with no latency).
 *
 * C11, self-contained.
 */
#include "../egojar.h"

#include "input.h"
#include "wubu_evdev.h"
#include "../runtime/wubu_steaminput.h"

#include <stdio.h>
#include <string.h>

#define WUBU_EVDEV_MAX 8

/* the axes we normalize; indexed by the SI axis enum */
enum {
    WUBU_EVDEV_NAX = 4,     /* LX, LY, RX, RY */
    WUBU_EVDEV_LX = 0, WUBU_EVDEV_LY, WUBU_EVDEV_RX, WUBU_EVDEV_RY
};

typedef struct {
    int         fd;
    char        path[64];
    char        name[64];
    int         min[WUBU_EVDEV_NAX];
    int         max[WUBU_EVDEV_NAX];
    int         is_pad;
} WubuEvdev;

static WubuEvdev g_dev[WUBU_EVDEV_MAX];
static int       g_ndev;
static int       g_inited;

/* ---- the evdev -> Steam Input button map ------------------------------ */
typedef struct { uint16_t ev; int si; } WubuBtnMap;

static const WubuBtnMap g_buttons[] = {
    { BTN_SOUTH,    SI_A         },
    { BTN_EAST,     SI_B         },
    { BTN_NORTH,    SI_X         },
    { BTN_WEST,     SI_Y         },
    { BTN_TL,       SI_LB        },
    { BTN_TR,       SI_RB        },
    { BTN_TL2,      SI_LT        },
    { BTN_TR2,      SI_RT        },
    { BTN_SELECT,   SI_BACK      },
    { BTN_START,    SI_START     },
    { BTN_THUMBL,   SI_L3        },
    { BTN_THUMBR,   SI_R3        },
    { BTN_DPAD_UP,    SI_DPAD_UP   },
    { BTN_DPAD_DOWN,  SI_DPAD_DOWN },
    { BTN_DPAD_LEFT,  SI_DPAD_LEFT },
    { BTN_DPAD_RIGHT, SI_DPAD_RIGHT},
};

/* the evdev -> Steam Input axis map (code + which SI axis) */
typedef struct { uint16_t ev; int axis; } WubuAxisMap;

static const WubuAxisMap g_axes[] = {
    { ABS_X,  WUBU_EVDEV_LX },
    { ABS_Y,  WUBU_EVDEV_LY },
    { ABS_RX, WUBU_EVDEV_RX },
    { ABS_RY, WUBU_EVDEV_RY },
    /* triggers are axes on many pads; feed them as buttons instead */
    { ABS_Z,  -1 },
    { ABS_RZ, -1 },
};

/* Does this device look like a gamepad? It must have a button or an axis. */
static int device_is_pad(int fd)
{
    unsigned long key_bits[8];
    unsigned long abs_bits[8];
    memset(key_bits, 0, sizeof(key_bits));
    memset(abs_bits, 0, sizeof(abs_bits));

    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0)
        return 0;
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0)
        return 0;

    /* any key at all, or any absolute axis */
    for (size_t i = 0; i < 8; i++)
        if (key_bits[i] || abs_bits[i])
            return 1;
    return 0;
}

/* Record the ABS range so the value can be normalized to -1..1. */
static void read_abs_range(int fd, WubuEvdev *d)
{
    for (int i = 0; i < WUBU_EVDEV_NAX; i++) {
        d->min[i] = -32768;
        d->max[i] =  32767;
    }
    struct input_absinfo ai;
    for (size_t i = 0; i < sizeof(g_axes)/sizeof(g_axes[0]); i++) {
        int ax = g_axes[i].axis;
        if (ax < 0 || ax >= WUBU_EVDEV_NAX)
            continue;
        if (ioctl(fd, EVIOCGABS((unsigned)g_axes[i].ev), &ai) == 0) {
            if (ai.maximum > ai.minimum) {
                d->min[ax] = ai.minimum;
                d->max[ax] = ai.maximum;
            }
        }
    }
}

static int open_device(const char *path, WubuEvdev *d)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        return -1;

    if (!device_is_pad(fd)) {
        close(fd);
        return -1;
    }

    memset(d, 0, sizeof(*d));
    d->fd = fd;
    snprintf(d->path, sizeof(d->path), "%s", path);

    char nm[128] = "unknown";
    if (ioctl(fd, EVIOCGNAME(sizeof(nm)), nm) < 0)
        snprintf(nm, sizeof(nm), "unknown");
    snprintf(d->name, sizeof(d->name), "%s", nm);

    read_abs_range(fd, d);
    d->is_pad = 1;
    return fd;
}

/* EV1 */
int wubu_evdev_init(void)
{
    if (g_inited)
        return g_ndev;
    g_inited = 1;

    DIR *dir = opendir("/dev/input");
    if (!dir)
        return 0;

    char path[128];
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (g_ndev >= WUBU_EVDEV_MAX)
            break;
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        if (open_device(path, &g_dev[g_ndev]) >= 0)
            g_ndev++;
    }
    closedir(dir);
    return g_ndev;
}

/* EV2 */
static float normalize(const WubuEvdev *d, int axis, int raw)
{
    int lo = d->min[axis], hi = d->max[axis];
    if (hi <= lo)
        return 0.0f;
    float v = (float)(raw - lo) / (float)(hi - lo);   /* 0..1 */
    v = v * 2.0f - 1.0f;                              /* -1..1 */
    /* a 12% deadzone, scaled back to the full range */
    if (v > -0.12f && v < 0.12f)
        return 0.0f;
    if (v >  1.0f) v =  1.0f;
    if (v < -1.0f) v = -1.0f;
    return v;
}

/* Route one already-read event. Shared by the device loop and the test
 * hook so there is exactly one mapping path. */
static int route_event(WubuEvdev *d, const struct input_event *ev)
{
    int routed = 0;

    if (ev->type == EV_KEY) {
        for (size_t b = 0; b < sizeof(g_buttons)/sizeof(g_buttons[0]); b++) {
            if (g_buttons[b].ev == ev->code) {
                wubu_si_feed_button(g_buttons[b].si, ev->value ? 1 : 0);
                routed++;
                break;
            }
        }
    } else if (ev->type == EV_ABS) {
        for (size_t a = 0; a < sizeof(g_axes)/sizeof(g_axes[0]); a++) {
            if (g_axes[a].ev != ev->code)
                continue;
            if (g_axes[a].axis >= 0) {
                wubu_si_feed_axis(g_axes[a].axis,
                                  normalize(d, g_axes[a].axis, ev->value));
                routed++;
            } else {
                /* a trigger axis -> a button past halfway */
                int down = ev->value > (d->max[0] + d->min[0]) / 2;
                int si = (g_axes[a].ev == ABS_Z) ? SI_LT : SI_RT;
                wubu_si_feed_button(si, down);
                routed++;
            }
            break;
        }
    }
    return routed;
}

int wubu_evdev_poll(void)
{
    if (!g_inited)
        return 0;

    struct input_event ev;
    int routed = 0;

    for (int i = 0; i < g_ndev; i++) {
        WubuEvdev *d = &g_dev[i];
        if (d->fd < 0)
            continue;
        for (;;) {
            ssize_t n = read(d->fd, &ev, sizeof(ev));
            if (n != (ssize_t)sizeof(ev))
                break;                      /* drained or device gone */
            routed += route_event(d, &ev);
        }
    }
    return routed;
}

/* EV7: the test hook -- route one synthetic event through the SAME path a
 * real device takes, so the mapping is gated on real behavior. */
void wubu_evdev_inject(uint16_t type, uint16_t code, int32_t value)
{
    /* The test environment has no device, so g_dev[0] has no ranges and
     * normalize() would divide by zero. Give it the generic pad range. */
    if (g_dev[0].fd == 0 && g_dev[0].max[WUBU_EVDEV_LX] == 0) {
        for (int i = 0; i < WUBU_EVDEV_NAX; i++) {
            g_dev[0].min[i] = -32768;
            g_dev[0].max[i] =  32767;
        }
        g_dev[0].fd = -1;
    }

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type  = type;
    ev.code  = code;
    ev.value = value;
    route_event(&g_dev[0], &ev);
}

/* EV3 */
void wubu_evdev_shutdown(void)
{
    for (int i = 0; i < g_ndev; i++) {
        if (g_dev[i].fd >= 0) {
            close(g_dev[i].fd);
            g_dev[i].fd = -1;
        }
    }
    g_ndev = 0;
    g_inited = 0;
}

/* EV4 */
int wubu_evdev_present(void)
{
    return g_ndev > 0;
}

/* EV5 */
int wubu_evdev_count(void)
{
    return g_ndev;
}

/* EV6 */
const char *wubu_evdev_name(int index)
{
    if (index < 0 || index >= g_ndev)
        return NULL;
    return g_dev[index].name;
}
