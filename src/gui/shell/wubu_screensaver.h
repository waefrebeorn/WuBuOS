/*
 * wubu_screensaver.h -- idle screen saver
 *
 * No idle behaviour existed: the desktop stayed fully lit forever.  This
 * module tracks idle time, engages after a timeout, and wakes on any input.
 * The saver is a dimmed screen with the clock -- enough to see the time
 * without a full blank, matching the classic Win98 "Blank" style.
 */

#ifndef WUBU_SCREENSAVER_H
#define WUBU_SCREENSAVER_H

#include <stdint.h>
#include <stdbool.h>

/* Engage after this much input inactivity (default 5 minutes). */
void wubu_screensaver_set_timeout_ms(uint32_t ms);
uint32_t wubu_screensaver_timeout_ms(void);

/* Call when input arrives -- resets the idle clock and wakes the saver. */
void wubu_screensaver_activity(void);

/* Advance the idle clock.  Call once per rendered frame with the frame time. */
void wubu_screensaver_tick(uint32_t delta_ms);

bool wubu_screensaver_active(void);

/* Draw the saver if active. */
void wubu_screensaver_render(uint32_t *fb, int fb_w, int fb_h);

#endif /* WUBU_SCREENSAVER_H */
