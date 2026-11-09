/*
 * wubu_gesture.h -- multitouch gesture recognition
 *
 * wubu_ptp.c answers "does a touchpad exist?".  Nothing answered "what is the
 * user doing with it?" -- the desktop had no gesture input at all.
 *
 * This module is the gesture engine.  It takes semantic contact reports
 * (id, x, y, down) plus a millisecond timestamp and recognises taps, double
 * taps, four-way swipes, pinch and rotation.  Keeping the input semantic
 * rather than raw evdev bytes means the recogniser is fully testable with
 * synthetic contact sequences and the evdev reader stays a thin adapter.
 *
 * Recogniser rules (deliberately conservative -- a false gesture is worse
 * than a missed one):
 *   tap           1 contact, < 12 px travel, < 400 ms
 *   double tap    two taps within 350 ms, < 24 px apart
 *   swipe         >= 2 contacts, >= 60 px dominant travel, dominant axis
 *                 carries >= 80 % of the distance
 *   pinch         2 contacts, separation changed by >= 20 %
 *   rotate        2 contacts, bearing changed by >= 25 degrees
 */

#ifndef WUBU_GESTURE_H
#define WUBU_GESTURE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    WUBU_GESTURE_NONE = 0,
    WUBU_GESTURE_TAP,
    WUBU_GESTURE_DOUBLE_TAP,
    WUBU_GESTURE_SWIPE_LEFT,
    WUBU_GESTURE_SWIPE_RIGHT,
    WUBU_GESTURE_SWIPE_UP,
    WUBU_GESTURE_SWIPE_DOWN,
    WUBU_GESTURE_PINCH_IN,
    WUBU_GESTURE_PINCH_OUT,
    WUBU_GESTURE_ROTATE_CW,
    WUBU_GESTURE_ROTATE_CCW,
} WubuGesture;

#define WUBU_GESTURE_MAX_CONTACTS 8

void wubu_gesture_init(void);
void wubu_gesture_reset(void);

/* Feed one contact report.  `now_ms` is a monotonically increasing clock. */
void wubu_gesture_contact(int id, int x, int y, bool down, uint32_t now_ms);

/* Poll for a completed gesture.  Non-blocking: returns 1 and sets *out when
 * one is ready, 0 otherwise.  A gesture is emitted at most once per poll. */
int  wubu_gesture_poll(WubuGesture *out);

/* Live state, for surfaces that want to show a gesture in progress. */
int   wubu_gesture_contact_count(void);
float wubu_gesture_pinch_scale(void);     /* 1.0 = unchanged since touch down */
float wubu_gesture_rotation_deg(void);    /* signed, degrees */

/* -- evdev adapter ------------------------------------------------ */

/* Open the first multitouch touchpad (/dev/input/event*).  Returns the fd,
 * or -1 when none is present or readable. */
int  wubu_gesture_open_touchpad(void);

/* Read pending events from an fd opened by wubu_gesture_open_touchpad(),
 * feed the engine, and poll.  Returns 1 when a gesture completed. */
int  wubu_gesture_poll_fd(int fd, WubuGesture *out);

/* Convenience: open once, then call this each frame. */
int  wubu_gesture_tick(WubuGesture *out);

#endif /* WUBU_GESTURE_H */
