/*
 * wubu_osd.h -- System volume / mute / brightness state + on-screen display
 *
 * There was no global audio level at all: wubu_sound_init() takes a volume
 * per engine, and nothing stored "the system volume".  This module owns the
 * one system volume, the mute flag and the display brightness, draws the
 * overlay shown when either changes, and fades it back out.
 *
 * Self-contained: it draws with the VBE layer and reads nothing else.
 */

#ifndef WUBU_OSD_H
#define WUBU_OSD_H

#include <stdint.h>
#include <stdbool.h>

/* -- System audio level ------------------------------------------- */

void   wubu_osd_set_volume(float v);      /* 0.0 .. 1.0, clamped */
float  wubu_osd_volume(void);
void   wubu_osd_set_muted(bool muted);
bool   wubu_osd_muted(void);

/* -- Display brightness ------------------------------------------- */

void   wubu_osd_set_brightness(float v);  /* 0.2 .. 1.0, clamped */
float  wubu_osd_brightness(void);

/* -- Overlay ------------------------------------------------------ */

/* Show the overlay (call after changing a level). */
void   wubu_osd_notify(void);

/* Advance the fade. Call once per rendered frame. */
void   wubu_osd_tick(void);

/* Draw the overlay if it is still visible. */
void   wubu_osd_render(uint32_t *fb, int fb_w, int fb_h);

/* True while the overlay is on screen. */
bool   wubu_osd_is_visible(void);

#endif /* WUBU_OSD_H */
