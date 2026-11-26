/*
 * wubu_perf_profile.c -- the performance profile, taken from a real tuned distro.
 *
 * WHY THIS EXISTS
 *   "Make the OS fast" is not a requirement anyone can check. CachyOS ships a measured set of
 *   kernel tunables; this module carries the SAME values, with the reason for each, and exposes
 *   read / diff / apply so the OS can say what it would change instead of asserting it is tuned.
 *
 * PROVENANCE
 *   Values harvested from the CachyOS reference image:
 *     /usr/lib/sysctl.d/70-cachyos-settings.conf   (inside the ISO rootfs)
 *   Nothing here is invented; the test checks the table against that file's numbers.
 *
 * WHAT IT WILL NOT DO
 *   Write a value it cannot read back. Applying needs privileges; without them the call reports
 *   DENIED rather than returning success and leaving the system untuned but claiming otherwise.
 *
 * C11, minimal includes, no libm.
 */
#include "wubu_perf_profile.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

typedef struct {
    const char *path;      /* /proc/sys path */
    const char *value;     /* what the profile wants (as sysctl would write it) */
    const char *why;
} PerfKnob;

/* Every entry traces to 70-cachyos-settings.conf. Order mirrors the file. */
static const PerfKnob g_knobs[] = {
    { "/proc/sys/vm/swappiness",            "100",
      "zram-backed swap: swapping to RAM is cheap, so use it before dropping cache" },
    { "/proc/sys/vm/vfs_cache_pressure",    "50",
      "keep dentry/inode cache longer -- the desktop re-walks directories constantly" },
    { "/proc/sys/vm/dirty_ratio",           "10",
      "bound the dirty page ceiling so a copy cannot stall a frame" },
    { "/proc/sys/vm/page-cluster",          "0",
      "read one page at a time: fewer speculative reads wasted on startup" },
    { "/proc/sys/vm/dirty_background_ratio","3",
      "start background writeback early" },
    { "/proc/sys/kernel/nmi_watchdog",      "0",
      "the NMI watchdog costs a perf counter and buys nothing on a desktop" },
    { "/proc/sys/net/core/netdev_max_backlog", "4096",
      "deeper backlog so a burst on the link is not dropped before the socket wakes" },
    { "/proc/sys/kernel/kptr_restrict",     "2",
      "kernel pointers hidden from unprivileged readers" },
    { "/proc/sys/kernel/printk",            "3 3 3 3",
      "quiet console: log spam is frame jitter" },
};

#define PERF_KNOB_COUNT ((int)(sizeof g_knobs / sizeof g_knobs[0]))

int wubu_perf_knob_count(void) { return PERF_KNOB_COUNT; }

const char *wubu_perf_knob_path(int i)  { return (i >= 0 && i < PERF_KNOB_COUNT) ? g_knobs[i].path : NULL; }
const char *wubu_perf_knob_value(int i) { return (i >= 0 && i < PERF_KNOB_COUNT) ? g_knobs[i].value : NULL; }
const char *wubu_perf_knob_why(int i)   { return (i >= 0 && i < PERF_KNOB_COUNT) ? g_knobs[i].why : NULL; }

/* Read the current value into buf, trimmed. Returns 0 on success. */
int wubu_perf_read(const char *path, char *buf, size_t cap) {
    if (!path || !buf || cap < 2) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, (int)cap, f)) { fclose(f); return -1; }
    fclose(f);
    size_t n = strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == ' ' || buf[n - 1] == '\t')) buf[--n] = '\0';
    /* collapse internal runs of whitespace so "3 3 3 3" compares equal to the declared form */
    char *w = buf;
    for (char *r = buf; *r; r++) {
        if (*r == '\t') *r = ' ';
        if (*r == ' ' && w > buf && w[-1] == ' ') continue;
        *w++ = *r;
    }
    *w = '\0';
    return 0;
}

int wubu_perf_matches(int i) {
    if (i < 0 || i >= PERF_KNOB_COUNT) return -1;
    char cur[64];
    if (wubu_perf_read(g_knobs[i].path, cur, sizeof cur) != 0) return -1;
    return strcmp(cur, g_knobs[i].value) == 0 ? 1 : 0;
}

/* Apply one knob. Returns 0 applied, PERF_DENIED when the write is refused by the kernel,
 * PERF_UNREADABLE when the knob is not present on this system at all. */
int wubu_perf_apply(int i) {
    if (i < 0 || i >= PERF_KNOB_COUNT) return PERF_UNREADABLE;
    char cur[64];
    if (wubu_perf_read(g_knobs[i].path, cur, sizeof cur) != 0) return PERF_UNREADABLE;
    FILE *f = fopen(g_knobs[i].path, "w");
    if (!f) return (errno == EACCES || errno == EPERM) ? PERF_DENIED : PERF_UNREADABLE;
    int ok = fprintf(f, "%s", g_knobs[i].value) > 0;
    fclose(f);
    if (!ok) return PERF_DENIED;
    /* Verify by reading back: a write that did not take is not an applied setting. */
    if (!wubu_perf_matches(i)) return PERF_DENIED;
    return PERF_APPLIED;
}

int wubu_perf_apply_all(char *report, size_t cap) {
    int changed = 0, denied = 0, missing = 0;
    for (int i = 0; i < PERF_KNOB_COUNT; i++) {
        int m = wubu_perf_matches(i);
        if (m == 1) continue;
        int r = wubu_perf_apply(i);
        if (r == PERF_APPLIED) changed++;
        else if (r == PERF_DENIED) denied++;
        else missing++;
    }
    if (report && cap)
        snprintf(report, cap, "perf: %d applied, %d denied, %d unavailable",
                 changed, denied, missing);
    return changed;
}
