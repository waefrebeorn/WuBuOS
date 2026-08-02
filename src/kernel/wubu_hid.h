/*
 * wubu_hid.h  --  WuBuOS Unified HID Layer (GameInput-style)
 *
 * ONE event model for every device -- keyboard, mouse, gamepad, and
 * whatever the future USB HID stack finds.  A single ring of unified
 * events with a common time base (the 100 Hz tick); consumers poll one
 * API and filter by device.  Feeders (PS/2 now, xHCI HID next) call
 * wubu_hid_feed_* -- no consumer cares which device produced an event.
 *
 * Freestanding: fixed ring, no malloc.
 */
#ifndef WUBU_HID_H
#define WUBU_HID_H

#include <stdint.h>
#include <stdbool.h>

/* -- Device ids ----------------------------------------------------- */

enum {
    WUBU_DEV_KEYBOARD = 0,
    WUBU_DEV_MOUSE    = 1,
    WUBU_DEV_GAMEPAD  = 2,
    WUBU_DEV_COUNT    = 3,
};

/* -- Event kinds ---------------------------------------------------- */

enum {
    WUBU_EV_KEY      = 0,   /* key press/release                    */
    WUBU_EV_MOUSE    = 1,   /* move / button / scroll                */
    WUBU_EV_GAMEPAD  = 2,   /* button / axis change                  */
};

/* -- Event payloads ------------------------------------------------- */

typedef struct {
    uint32_t keycode;     /* translated key (see input.h) */
    bool     down;        /* true=press, false=release    */
    uint32_t mods;        /* MOD_SHIFT/CTRL/ALT/WIN       */
} WubuInputKey;

typedef struct {
    int x, y;             /* position                     */
    int dx, dy;           /* delta                        */
    int buttons;          /* bit0=left, 1=right, 2=mid    */
    int scroll;           /* wheel delta                  */
} WubuInputMouse;

typedef struct {
    uint32_t buttons;     /* mask: 16 buttons             */
    int16_t  axes[8];     /* sticks/triggers (centered)   */
} WubuInputGamepad;

/* -- Unified event -------------------------------------------------- */

typedef struct {
    uint8_t  device;      /* WUBU_DEV_*                   */
    uint8_t  kind;        /* WUBU_EV_*                    */
    uint32_t ts_ms;       /* common time base (tick*10)   */
    union {
        WubuInputKey    key;
        WubuInputMouse  mouse;
        WubuInputGamepad pad;
    } u;
} WubuInputEvent;

/* -- API ------------------------------------------------------------ */

void wubu_hid_init(void);

/* Feeders (drivers call these; PS/2 now, USB HID next). */
void wubu_hid_feed_key(uint32_t keycode, bool down, uint32_t mods);
void wubu_hid_feed_mouse(int x, int y, int dx, int dy, int buttons, int scroll);
void wubu_hid_feed_gamepad(uint32_t buttons, const int16_t *axes, int naxes);

/* Consumers: poll the NEXT event from any device (0 = none). */
int  wubu_hid_poll(WubuInputEvent *out);

/* Per-device filter mask (bitmask of WUBU_EV_*); 0 = all kinds.
 * Returns the previous mask. */
uint32_t wubu_hid_filter(uint8_t device, uint32_t kind_mask);

/* Master switch: disable a device entirely (0 = enabled).
 * Returns the previous state. */
bool wubu_hid_disable(uint8_t device, bool disabled);

/* Stats: events delivered since boot, per device. */
uint32_t wubu_hid_stats(uint8_t device);

/* Ring depth + overflow counter (diagnostics). */
int      wubu_hid_queued(void);
uint32_t wubu_hid_overflow(void);

#endif /* WUBU_HID_H */
