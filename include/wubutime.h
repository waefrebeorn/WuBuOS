#ifndef WUBUTIME_H
#define WUBUTIME_H

#include <stddef.h>
#include <stdint.h>

/*
 * WuBuTIME -- the time service.
 *
 * Owns one concern: turning raw clock readings into a coherent calendar, and
 * keeping it honest.  It reads through our own wubu_posix surface (no libc),
 * keeps an epoch offset so monotonic and wall clocks can be converted with one
 * addition, exposes an alarm wheel for the shell/desktop, and calibrates the
 * TSC against CLOCK_MONOTONIC so drift is measured rather than assumed.
 */

typedef struct wubutime wubutime;

int  wubutime_create(wubutime **out);
void wubutime_destroy(wubutime *t);

/* Both are nanoseconds since their respective epochs. */
uint64_t wubutime_mono_ns(const wubutime *t);
uint64_t wubutime_wall_ns(const wubutime *t);

/* Lockstep conversion: wall = mono + offset; mono = wall - offset. */
int64_t  wubutime_offset_ns(const wubutime *t);
uint64_t wubutime_mono_to_wall(const wubutime *t, uint64_t mono_ns);
uint64_t wubutime_wall_to_mono(const wubutime *t, uint64_t wall_ns);

/* TSC frequency in Hz as measured against CLOCK_MONOTONIC.  Returns 0 when the
 * invariant TSC cannot be read on this target. */
double   wubutime_tsc_hz(const wubutime *t);
/* Measure once over `sleep_ns` of real time.  Returns ppm of disagreement
 * between the TSC-derived elapsed time and the kernel's own reading. */
double   wubutime_calibrate_ppm(wubutime *t, long sleep_ns);

/* Civil (proleptic Gregorian) from a wall-clock ns value. */
void     wubutime_civil(const wubutime *t, uint64_t wall_ns,
                        int *year, int *mon, int *day,
                        int *hour, int *min, int *sec, int *wday);
/* Inverse of wubutime_civil, in ns since the Unix epoch. */
uint64_t wubutime_from_civil(int year, int mon, int day,
                             int hour, int min, int sec);
/* ISO-8601 UTC, e.g. "2026-11-17T14:03:21Z".  Truncates safely. */
size_t   wubutime_iso8601(char *buf, size_t cap, uint64_t wall_ns);

typedef void (*wubutime_cb)(void *arg);

/* One-shot alarms on the monotonic clock, earliest-first. */
int      wubutime_alarm_add(wubutime *t, uint64_t at_mono_ns,
                            wubutime_cb cb, void *arg);
int      wubutime_alarm_poll(wubutime *t, uint64_t now_mono_ns);
int      wubutime_alarm_count(const wubutime *t);

#endif /* WUBUTIME_H */
