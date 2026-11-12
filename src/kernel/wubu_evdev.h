/*
 * wubu_evdev.h -- the evdev gamepad pipeline (feeds Steam Input).
 */
#ifndef WUBU_EVDEV_H
#define WUBU_EVDEV_H

/* EV1: scan /dev/input/event* and open every device that looks like a
 * gamepad (it has a button or an absolute axis). Returns the number
 * opened. Safe to call twice; a second call is a no-op. */
int  wubu_evdev_init(void);

/* EV2: drain every pending event from every open device and route it
 * through the Steam Input mapping table. Returns the events routed.
 * Non-blocking: returns 0 immediately when nothing is pending. */
int  wubu_evdev_poll(void);

/* EV3: close every device. */
void wubu_evdev_shutdown(void);

/* EV4: is at least one gamepad open? */
int  wubu_evdev_present(void);

/* EV5: how many devices are open? */
int  wubu_evdev_count(void);

/* EV6: the path of the Nth open device, or NULL. */
const char *wubu_evdev_name(int index);

/* EV7: the test hook -- route one synthetic event through the same path a
 * real device takes. No device needs to exist. */
void wubu_evdev_inject(uint16_t type, uint16_t code, int32_t value);

#endif
