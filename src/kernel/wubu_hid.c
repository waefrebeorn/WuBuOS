/*
 * wubu_hid.c  --  WuBuOS Unified HID Layer (GameInput-style)
 *
 * A single ring of unified events, common time base (g_tick * 10 ms).
 * Drivers feed; consumers poll; filters hide kinds per device.
 *
 * Freestanding: fixed ring, no malloc, no hosted APIs.
 */

#include "wubu_hid.h"

/* kernel tick accessor (tasking.h: task_tick_count) */
uint64_t task_tick_count(void);

#define WUBU_HID_RING 64

static WubuInputEvent g_ring[WUBU_HID_RING];
static int            g_head;     /* next write slot */
static int            g_tail;     /* oldest unconsumed slot */
static int            g_count;
static uint32_t       g_filter[WUBU_DEV_COUNT];
static bool           g_disabled[WUBU_DEV_COUNT];
static uint32_t       g_stats[WUBU_DEV_COUNT];
static uint32_t       g_overflow;

static uint32_t now_ms(void)
{
    return (uint32_t)(task_tick_count() * 10u);
}

void wubu_hid_init(void)
{
    g_head = 0;
    g_tail = 0;
    g_count = 0;
    g_overflow = 0;
    for (int i = 0; i < WUBU_DEV_COUNT; i++) {
        g_filter[i] = 0;
        g_disabled[i] = false;
        g_stats[i] = 0;
    }
}

static void push(WubuInputEvent *ev)
{
    ev->ts_ms = now_ms();
    g_stats[ev->device & 0xFFu]++;
    if (g_count < WUBU_HID_RING) {
        g_ring[g_head] = *ev;
        g_head = (g_head + 1) % WUBU_HID_RING;
        g_count++;
    } else {
        g_ring[g_head] = *ev;          /* overwrite oldest */
        g_head = (g_head + 1) % WUBU_HID_RING;
        g_overflow++;
    }
}

void wubu_hid_feed_key(uint32_t keycode, bool down, uint32_t mods)
{
    WubuInputEvent ev;
    ev.device = WUBU_DEV_KEYBOARD;
    ev.kind   = WUBU_EV_KEY;
    ev.u.key.keycode = keycode;
    ev.u.key.down    = down;
    ev.u.key.mods    = mods;
    push(&ev);
}

void wubu_hid_feed_mouse(int x, int y, int dx, int dy, int buttons, int scroll)
{
    WubuInputEvent ev;
    ev.device = WUBU_DEV_MOUSE;
    ev.kind   = WUBU_EV_MOUSE;
    ev.u.mouse.x = x; ev.u.mouse.y = y;
    ev.u.mouse.dx = dx; ev.u.mouse.dy = dy;
    ev.u.mouse.buttons = buttons;
    ev.u.mouse.scroll = scroll;
    push(&ev);
}

void wubu_hid_feed_gamepad(uint32_t buttons, const int16_t *axes, int naxes)
{
    WubuInputEvent ev;
    ev.device = WUBU_DEV_GAMEPAD;
    ev.kind   = WUBU_EV_GAMEPAD;
    ev.u.pad.buttons = buttons;
    for (int i = 0; i < 8; i++)
        ev.u.pad.axes[i] = (axes && i < naxes) ? axes[i] : 0;
    push(&ev);
}

int wubu_hid_poll(WubuInputEvent *out)
{
    if (g_count <= 0) return 0;

    /* FIFO-with-drops: scan from the tail; filtered events are dropped by
     * advancing the position (they get overwritten by future feeds). */
    int pos = g_tail;
    int total = g_count;               /* scan the original occupancy */
    for (int n = 0; n < total; n++) {
        if (g_disabled[g_ring[pos].device]) {
            pos = (pos + 1) % WUBU_HID_RING;       /* device off */
            g_count--;
            continue;
        }
        uint32_t mask = g_filter[g_ring[pos].device];
        if (mask && !(mask & (1u << g_ring[pos].kind))) {
            pos = (pos + 1) % WUBU_HID_RING;       /* drop filtered */
            g_count--;
            continue;
        }
        if (out) *out = g_ring[pos];
        pos = (pos + 1) % WUBU_HID_RING;           /* consume */
        g_count--;
        g_tail = pos;
        return 1;
    }
    g_tail = pos;                                  /* all dropped */
    return 0;
}

uint32_t wubu_hid_filter(uint8_t device, uint32_t kind_mask)
{
    if (device >= WUBU_DEV_COUNT) return 0;
    uint32_t old = g_filter[device];
    g_filter[device] = kind_mask;
    return old;
}

bool wubu_hid_disable(uint8_t device, bool disabled)
{
    if (device >= WUBU_DEV_COUNT) return false;
    bool old = g_disabled[device];
    g_disabled[device] = disabled;
    return old;
}

uint32_t wubu_hid_stats(uint8_t device)
{
    if (device >= WUBU_DEV_COUNT) return 0;
    return g_stats[device];
}

int wubu_hid_queued(void)   { return g_count; }
uint32_t wubu_hid_overflow(void) { return g_overflow; }
