/*
 * WuBuTIME -- time service.
 *
 * Freestanding: it includes only its own header and our wubu_posix surface, so
 * it compiles with -nostdinc like the POSIX layer does.  Allocation comes from
 * a fixed instance pool (a time service is long-lived and bounded), string
 * formatting is done by hand, and the clock source is CLOCK_MONOTONIC /
 * CLOCK_REALTIME read through our own syscalls.
 */
#include "wubutime.h"
#include "wubu_posix.h"

#define WUBUTIME_MAX_ALARMS 16
#define WUBUTIME_POOL       4

struct wubutime_alarm {
    uint64_t at;
    wubutime_cb cb;
    void *arg;
};

struct wubutime {
    int used;
    int64_t offset_ns;            /* wall = mono + offset */
    double tsc_hz;                /* measured, 0 if unavailable */
    struct wubutime_alarm alarms[WUBUTIME_MAX_ALARMS];
    int nalarms;
};

static struct wubutime pool[WUBUTIME_POOL];

/* ------------------------------------------------ freestanding helpers */

static void *wt_memset(void *dst, int v, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    while (n--) *p++ = (unsigned char)v;
    return dst;
}

static void *wt_memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

static size_t wt_strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static int wt_i32(int v, char *buf, size_t cap, int width)
{
    char tmp[8];
    int n = 0;
    if (v < 0) return 0;               /* calendar fields are non-negative */
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 8);
    while (n < width) tmp[n++] = '0';
    if ((size_t)n + 1 > cap) return 0;
    for (int i = 0; i < n; ++i) buf[i] = tmp[n-1-i];
    buf[n] = '\0';
    return n;
}

/* ------------------------------------------------------------------ clock */

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
    int slot = -1;
    for (int i = 0; i < WUBUTIME_POOL; ++i)
        if (!pool[i].used) { slot = i; break; }
    if (slot < 0) return 0;

    struct wubutime *t = &pool[slot];
    wt_memset(t, 0, sizeof *t);

    struct wubu_timespec mono, real;
    if (wubu_clock_gettime(WUBU_CLOCK_MONOTONIC, &mono) != 0 ||
        wubu_clock_gettime(WUBU_CLOCK_REALTIME, &real) != 0) {
        return 0;
    }
    uint64_t m = ts_to_ns(&mono), r = ts_to_ns(&real);
    t->offset_ns = (int64_t)r - (int64_t)m;
    wubutime_calibrate_ppm(t, 2000000L); /* measure, don't assume */
    t->used = 1;
    *out = t;
    return 1;
}

void wubutime_destroy(wubutime *t)
{
    if (t) t->used = 0;
}

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

/* Measure the TSC against the kernel clock.  Both samples must bracket the SAME
 * interval, otherwise the ratio collapses. */
double wubutime_calibrate_ppm(wubutime *t, long sleep_ns)
{
    if (!t) return 0.0;
    uint64_t tsc0 = rdtsc_now();
    uint64_t mono0 = wubutime_mono_ns(t);

    struct wubu_timespec req, rem;
    ns_to_ts((uint64_t)sleep_ns, &req);
    wt_memset(&rem, 0, sizeof rem);
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
    for (;;) {
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

    char tmp[8];
    size_t p = 0;
    int n = wt_i32(Y, tmp, sizeof tmp, 4);
    if (!n || p + (size_t)n >= cap) return 0;
    for (int i = 0; i < n; ++i) buf[p++] = tmp[i];
    buf[p++] = '-';
    n = wt_i32(Mo, tmp, sizeof tmp, 2); for (int i = 0; i < n; ++i) buf[p++] = tmp[i];
    buf[p++] = '-';
    n = wt_i32(D, tmp, sizeof tmp, 2);  for (int i = 0; i < n; ++i) buf[p++] = tmp[i];
    buf[p++] = 'T';
    n = wt_i32(H, tmp, sizeof tmp, 2);  for (int i = 0; i < n; ++i) buf[p++] = tmp[i];
    buf[p++] = ':';
    n = wt_i32(Mi, tmp, sizeof tmp, 2); for (int i = 0; i < n; ++i) buf[p++] = tmp[i];
    buf[p++] = ':';
    n = wt_i32(S, tmp, sizeof tmp, 2);  for (int i = 0; i < n; ++i) buf[p++] = tmp[i];
    buf[p++] = 'Z';
    buf[p] = '\0';
    return p;
}

int wubutime_from_iso8601(const char *s, uint64_t *wall_ns)
{
    if (!s || !wall_ns || wt_strlen(s) < 19) return 0;
    int v[6];
    static const int sep[6] = {4, 2, 2, 2, 2, 2};
    int pos = 0;
    for (int f = 0; f < 6; ++f) {
        if (f > 0) {
            char expect = (f <= 2) ? '-' : (f == 3) ? 'T' : ':';
            if (s[pos++] != expect) return 0;
        }
        int val = 0;
        for (int k = 0; k < sep[f]; ++k) {
            char c = s[pos++];
            if (c < '0' || c > '9') return 0;
            val = val * 10 + (c - '0');
        }
        v[f] = val;
    }
    if (v[1] < 1 || v[1] > 12 || v[2] < 1 || v[2] > 31) return 0;
    if (v[3] > 23 || v[4] > 59 || v[5] > 59) return 0;
    *wall_ns = wubutime_from_civil(v[0], v[1], v[2], v[3], v[4], v[5]);
    return 1;
}

/* ---------------------------------------------------------------- alarms */

int wubutime_alarm_add(wubutime *t, uint64_t at_mono_ns, wubutime_cb cb, void *arg)
{
    if (!t || !cb || t->nalarms >= WUBUTIME_MAX_ALARMS) return 0;
    int i = t->nalarms++;
    t->alarms[i].at = at_mono_ns;
    t->alarms[i].cb = cb;
    t->alarms[i].arg = arg;
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
        wt_memmove(&t->alarms[0], &t->alarms[1],
                   sizeof a * (size_t)(t->nalarms - 1));
        t->nalarms--;
        if (a.cb) a.cb(a.arg);
        ++fired;
    }
    return fired;
}

int wubutime_alarm_count(const wubutime *t) { return t ? t->nalarms : 0; }
