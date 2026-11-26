/*
 * wubu_perf_profile.h -- kernel tunables carried from a real tuned distro.
 *
 * The profile is DATA (path, value, why). Callers can diff it against the running system,
 * apply it, or show it. See wubu_perf_profile.c for provenance and the read-back rule.
 */
#ifndef WUBU_PERF_PROFILE_H
#define WUBU_PERF_PROFILE_H

#include <stddef.h>

#define PERF_APPLIED    0
#define PERF_DENIED     1
#define PERF_UNREADABLE 2

int         wubu_perf_knob_count(void);
const char *wubu_perf_knob_path(int i);
const char *wubu_perf_knob_value(int i);
const char *wubu_perf_knob_why(int i);

/* 1 = already matches, 0 = differs, -1 = not present on this system. */
int  wubu_perf_matches(int i);
int  wubu_perf_read(const char *path, char *buf, size_t cap);
int  wubu_perf_apply(int i);
int  wubu_perf_apply_all(char *report, size_t cap);

#endif /* WUBU_PERF_PROFILE_H */
