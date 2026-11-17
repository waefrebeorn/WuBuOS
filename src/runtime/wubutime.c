/*
 * WuBuTIME -- time service.
 *
 * Clock source is our own wubu_posix layer.  The offset between CLOCK_MONOTONIC
 * and CLOCK_REALTIME is captured at create() so the rest of the system can
 * convert with a single addition instead of issuing two syscalls per timestamp.
 */
#include "wubutime.h"
#include "wubu_posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WUBUTIME_MAX_ALARMS 16

struct wubutime_alarm {
    uint64_t at;
    wubutime_cb cb;
    void *arg;
};

struct wubutime {
    int64_t offset_ns;            /* wall = mono + offset */
    double tsc_hz;                /* measured, 0 if unavailable */
    struct wubutime_alarm alarms[WUBUTIME_MAX_ALARMS];
    int nalarms;
};

static uint64_t ts_to_ns(const struct wubu_timespec *ts)
{
    return (uint64_t)ts->tv_sec * 1000000000ull + (uint64_t)ts->tv_nsec;
}

static void ns_to_ts(uint64_t ns, struct wubu_timespec *ts)
{
    ts->tv_sec  = (long)(ns / 1000000000ull);
    ts->tv_nsec = (long)(ns % 1000000000ull);
}

#if defined(__x86_64__)
static uint64_t rdtsc_now(void)
{
    unsigned int lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#else
static uint64_t rdtsc_now(void) { return 0; }
#endif

int wubutime_create(wubutime **out)
{
    if (!out) return 0;
    struct wubutime *t = calloc(1, sizeof *t);
    if (!t) return 0;

    struct wubu_timespec mono, real;
    if (wubu_clock_gettime(WUBU_CLOCK_MONOTONIC, &mono) != 0 ||
        wubu_clock_gettime(WUBU_CLOCK_REALTIME, &real) != 0) {
        free(t);
        return 0;
    }
    uint64_t m = ts_to_ns(&mono), r = ts_to_ns(&real);
    t->offset_ns = (int64_t)r - (int64_t)m;
    wubutime_calibrate_ppm(t, 2000000L); /* measure, don't assume */
    *out = t;
    return 1;
}

void wubutime_destroy(wubutime *t) { free(t); }

uint64_t wubutime_mono_ns(const wubutime *t)
{
    struct wubu_timespec ts;
    if (!t || wubu_clock_gettime(WUBU_CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ts_to_ns(&ts);
}

uint64_t wubutime_wall_ns(const wubutime *t)
{
    if (!t) return 0;
    return (uint64_t)((int64_t)wubutime_mono_ns(t) + t->offset_ns);
}

int64_t  wubutime_offset_ns(const wubutime *t) { return t ? t->offset_ns : 0; }

uint64_t wubutime_mono_to_wall(const wubutime *t, uint64_t mono_ns)
{
    return (uint64_t)((int64_t)mono_ns + wubutime_offset_ns(t));
}

uint64_t wubutime_wall_to_mono(const wubutime *t, uint64_t wall_ns)
{
    return (uint64_t)((int64_t)wall_ns - wubutime_offset_ns(t));
}

double wubutime_tsc_hz(const wubutime *t) { return t ? t->tsc_hz : 0.0; }

/* Measure the TSC against the kernel clock.  Returns ppm disagreement between
 * elapsed-TSC-seconds and elapsed-kernel-seconds. */
double wubutime_calibrate_ppm(wubutime *t, long sleep_ns)
{
    if (!t) return 0.0;
    /* Both clocks must bracket the SAME interval, otherwise the ratio is
     * meaningless (and collapses when the kernel sample lands microseconds
     * apart at the end). */
    uint64_t tsc0 = rdtsc_now();
    uint64_t mono0 = wubutime_mono_ns(t);

    struct wubu_timespec req, rem;
    ns_to_ts((uint64_t)sleep_ns, &req);
    memset(&rem, 0, sizeof rem);
    wubu_nanosleep(&req, &rem);

    uint64_t tsc1 = rdtsc_now();
    uint64_t mono1 = wubutime_mono_ns(t);
    uint64_t ticks = tsc1 - tsc0;
    uint64_t ns = mono1 > mono0 ? mono1 - mono0 : 1;

    double hz = (double)ticks / ((double)ns / 1e9);
    t->tsc_hz = hz;
    double ratio = ((double)ticks / hz) / ((double)ns / 1e9);
    return (ratio - 1.0) * 1e6;
}

/* --------------------------------------------------------- civil calendar */

static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
static int is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

uint64_t wubutime_from_civil(int year, int mon, int day,
                             int hour, int min, int sec)
{
    if (mon < 1) mon = 1;
    if (mon > 12) mon = 12;
    if (day < 1) day = 1;
    int dim = mdays[mon-1] + ((mon == 2 && is_leap(year)) ? 1 : 0);
    if (day > dim) day = dim;

    int64_t days = 0;
    for (int y = 1970; y < year; ++y) days += is_leap(y) ? 366 : 365;
    for (int y = year; y < 1970; --y) days -= is_leap(y) ? 366 : 365;
    for (int m = 1; m < mon; ++m)
        days += mdays[m-1] + ((m == 2 && is_leap(year)) ? 1 : 0);
    days += day - 1;

    int64_t secs = days * 86400 + (int64_t)hour * 3600 + (int64_t)min * 60 + sec;
    return (uint64_t)secs * 1000000000ull;
}

void wubutime_civil(const wubutime *t, uint64_t wall_ns,
                    int *year, int *mon, int *day,
                    int *hour, int *min, int *sec, int *wday)
{
    (void)t;
    int64_t secs = (int64_t)(wall_ns / 1000000000ull);
    int64_t days = secs / 86400;
    int64_t rem  = secs % 86400;
    if (rem < 0) { rem += 86400; --days; }

    int y = 1970;
    while (1) {
        int64_t dy = is_leap(y) ? 366 : 365;
        if (days >= dy) { days -= dy; ++y; continue; }
        if (days < 0)   { --y; days += is_leap(y) ? 366 : 365; continue; }
        break;
    }
    int mo = 1;
    while (mo <= 12) {
        int dm = mdays[mo-1] + ((mo == 2 && is_leap(y)) ? 1 : 0);
        if (days < dm) break;
        days -= dm; ++mo;
    }
    int dd = (int)days + 1;

    if (year) *year = y;
    if (mon)  *mon  = mo;
    if (day)  *day  = dd;
    if (hour) *hour = (int)(rem / 3600);
    if (min)  *min  = (int)((rem % 3600) / 60);
    if (sec)  *sec  = (int)(rem % 60);
    if (wday) *wday = (int)(((secs / 86400) % 7 + 11) % 7); /* 1970-01-01 = Thu */
}

size_t wubutime_iso8601(char *buf, size_t cap, uint64_t wall_ns)
{
    if (!buf || cap < 21) return 0;
    int Y, Mo, D, H, Mi, S, WD;
    wubutime_civil(NULL, wall_ns, &Y, &Mo, &D, &H, &Mi, &S, &WD);
    int n = snprintf(buf, cap, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                     Y, Mo, D, H, Mi, S);
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}

/* ---------------------------------------------------------------- alarms */

int wubutime_alarm_add(wubutime *t, uint64_t at_mono_ns, wubutime_cb cb, void *arg)
{
    if (!t || !cb || t->nalarms >= WUBUTIME_MAX_ALARMS) return 0;
    int i = t->nalarms++;
    t->alarms[i].at = at_mono_ns;
    t->alarms[i].cb = cb;
    t->alarms[i].arg = arg;
    /* insertion sort: earliest first */
    while (i > 0 && t->alarms[i].at < t->alarms[i-1].at) {
        struct wubutime_alarm tmp = t->alarms[i];
        t->alarms[i] = t->alarms[i-1];
        t->alarms[i-1] = tmp;
        --i;
    }
    return 1;
}

int wubutime_alarm_poll(wubutime *t, uint64_t now_mono_ns)
{
    if (!t) return 0;
    int fired = 0;
    while (t->nalarms > 0 && t->alarms[0].at <= now_mono_ns) {
        struct wubutime_alarm a = t->alarms[0];
        memmove(&t->alarms[0], &t->alarms[1],
                sizeof a * (size_t)(t->nalarms - 1));
        t->nalarms--;
        if (a.cb) a.cb(a.arg);
        ++fired;
    }
    return fired;
}

int wubutime_alarm_count(const wubutime *t) { return t ? t->nalarms : 0; }
