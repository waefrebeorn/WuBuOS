/*
 * wubu_parental.c -- see wubu_parental.h. One file read, one pure decision, one file write.
 *
 * C11.
 */
#include "wubu_parental.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strncasecmp: day names are matched case-insensitively */
#include <sys/stat.h>

static const char *const kDayNames[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

const char *wubu_parental_conf_path(void)
{
    /* NOT cached: a path captured before $WUBU_PARENTAL_CONF was set silently ignores the override, and
     * that is exactly how a test can pass while the product reads the wrong file. Two snprintf calls are
     * cheaper than that class of bug. */
    static char path[WUBU_PARENTAL_PATH_MAX];

    const char *env = getenv("WUBU_PARENTAL_CONF");
    if (env && env[0]) {
        snprintf(path, sizeof path, "%s", env);
    } else {
        snprintf(path, sizeof path, "/etc/wubu/parental.conf");
    }
    return path;
}

/* "HH:MM" -> minutes, or -1. */
static int parse_hhmm(const char *s)
{
    if (!s) return -1;
    int h = -1, m = -1;
    if (sscanf(s, "%d:%d", &h, &m) != 2) return -1;
    if (h < 0 || h > 24 || m < 0 || m > 59) return -1;
    if (h == 24 && m != 0) return -1;
    return h * 60 + m;
}

static unsigned parse_days(const char *s)
{
    if (!s || !s[0]) return WUBU_PARENTAL_ALL_DAYS;

    unsigned mask = 0;
    char buf[128];
    snprintf(buf, sizeof buf, "%s", s);

    char *save = buf;
    while (save && *save) {
        char *comma = strchr(save, ',');
        if (comma) *comma = '\0';

        while (*save == ' ') save++;
        size_t n = strlen(save);
        while (n > 0 && save[n - 1] == ' ') save[--n] = '\0';

        /* "mon" or "mon-fri": the writer emits a list, a human writes ranges, and both must load. */
        char *dash = strchr(save, '-');
        if (dash) {
            *dash = '\0';
            int a = -1, b = -1;
            for (int d = 0; d < 7; d++) {
                if (strncasecmp(save, kDayNames[d], 3) == 0) a = d;
                if (strncasecmp(dash + 1, kDayNames[d], 3) == 0) b = d;
            }
            if (a >= 0 && b >= 0) {
                for (int d = a; ; d = (d + 1) % 7) {
                    mask |= (1u << d);
                    if (d == b) break;
                }
                save = comma ? comma + 1 : NULL;
                continue;
            }
        }
        for (int d = 0; d < 7; d++) {
            if (strncasecmp(save, kDayNames[d], 3) == 0) { mask |= (1u << d); break; }
        }
        save = comma ? comma + 1 : NULL;
    }
    return mask ? mask : WUBU_PARENTAL_ALL_DAYS;
}

int wubu_parental_load(WubuParentalPolicy *p)
{
    if (!p) return -1;
    memset(p, 0, sizeof(*p));
    p->days = WUBU_PARENTAL_ALL_DAYS;
    p->start_min = 8 * 60;
    p->end_min = 20 * 60;

    const char *path = wubu_parental_conf_path();
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    snprintf(p->source, sizeof p->source, "%s", path);
    p->loaded = true;

    char line[256];
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        while (*key == ' ') key++;
        while (*val == ' ') val++;

        if (strcmp(key, "enabled") == 0) {
            p->enabled = (atoi(val) != 0);
        } else if (strcmp(key, "hours") == 0) {
            char *dash = strchr(val, '-');
            if (dash) {
                *dash = '\0';
                int a = parse_hhmm(val), b = parse_hhmm(dash + 1);
                if (a >= 0 && b >= 0) { p->start_min = a; p->end_min = b; }
            }
        } else if (strcmp(key, "days") == 0) {
            p->days = parse_days(val);
        }
    }
    fclose(f);
    return 0;
}

void wubu_parental_window_str(const WubuParentalPolicy *p, char *out, size_t cap)
{
    if (!out || cap == 0) return;
    if (!p) { out[0] = '\0'; return; }
    snprintf(out, cap, "%02d:%02d-%02d:%02d",
             p->start_min / 60, p->start_min % 60, p->end_min / 60, p->end_min % 60);
}

void wubu_parental_days_str(const WubuParentalPolicy *p, char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = '\0';
    unsigned days = p ? p->days : 0;
    if (days == WUBU_PARENTAL_ALL_DAYS) { snprintf(out, cap, "Every day"); return; }
    if (days == WUBU_PARENTAL_WEEKDAYS) { snprintf(out, cap, "Mon-Fri"); return; }
    if (days == WUBU_PARENTAL_WEEKEND)  { snprintf(out, cap, "Sat-Sun"); return; }

    size_t used = 0;
    for (int d = 0; d < 7; d++) {
        if (!(days & (1u << d))) continue;
        int wrote = snprintf(out + used, cap - used, "%s%s", used ? ", " : "", kDayNames[d]);
        if (wrote < 0) break;
        used += (size_t)wrote;
        if (used >= cap) break;
    }
    if (!out[0]) snprintf(out, cap, "No days");
}

bool wubu_parental_allowed(const WubuParentalPolicy *p, const struct tm *local, char *why, size_t cap)
{
    if (why && cap) why[0] = '\0';
    if (!p || !p->enabled) return true;
    if (!local) {
        /* No clock is not permission: a limit that fails open on a broken clock is not a limit. */
        if (why && cap) snprintf(why, cap, "the clock is unavailable, so the family limit cannot be checked");
        return false;
    }

    /* tm_wday: 0 = Sunday. The policy's bit 0 is Monday. */
    int bit = (local->tm_wday + 6) % 7;
    if (!(p->days & (1u << bit))) {
        char days[64];
        wubu_parental_days_str(p, days, sizeof days);
        if (why && cap) snprintf(why, cap, "today is not an allowed day (%s)", days);
        return false;
    }

    int now = local->tm_hour * 60 + local->tm_min;
    bool inside;
    if (p->start_min == p->end_min) {
        inside = true;                       /* a zero-length window means "all day", not "never" */
    } else if (p->start_min < p->end_min) {
        inside = (now >= p->start_min && now < p->end_min);
    } else {
        /* Overnight: a child's bedtime is not the same day it ends. */
        inside = (now >= p->start_min || now < p->end_min);
    }

    if (!inside) {
        char win[32];
        wubu_parental_window_str(p, win, sizeof win);
        if (why && cap) snprintf(why, cap, "outside the allowed hours (%s)", win);
        return false;
    }
    return true;
}

bool wubu_parental_normalize(WubuParentalPolicy *p)
{
    if (!p) return false;
    bool changed = false;

    if (p->start_min < 0 || p->start_min > 24 * 60) { p->start_min = 8 * 60; changed = true; }
    if (p->end_min < 0 || p->end_min > 24 * 60) { p->end_min = 20 * 60; changed = true; }
    if (!p->days) { p->days = WUBU_PARENTAL_ALL_DAYS; changed = true; }
    return changed;
}

int wubu_parental_save(const WubuParentalPolicy *p)
{
    if (!p) return -1;

    char dir[WUBU_PARENTAL_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", wubu_parental_conf_path());
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        if (dir[0]) (void)mkdir(dir, 0755);       /* EEXIST is fine */
    }

    FILE *f = fopen(wubu_parental_conf_path(), "w");
    if (!f) return -1;

    char win[32];
    wubu_parental_window_str(p, win, sizeof win);
    /* The file gets a LIST ("mon,tue,wed"), not the friendly name ("Mon-Fri"): what is written must be
     * what the reader can read back, or the round trip silently loses days. */
    char days[64];
    size_t used = 0;
    days[0] = '\0';
    for (int d = 0; d < 7; d++) {
        if (!(p->days & (1u << d))) continue;
        char low[4];
        snprintf(low, sizeof low, "%c%c%c", kDayNames[d][0] | 0x20, kDayNames[d][1] | 0x20,
                 kDayNames[d][2] | 0x20);
        int wrote = snprintf(days + used, sizeof days - used, "%s%s", used ? "," : "", low);
        if (wrote < 0) break;
        used += (size_t)wrote;
        if (used >= sizeof days) break;
    }
    fputs("# WuBuOS family limits -- hours are local time, days are mon..sun\n", f);
    fprintf(f, "enabled=%d\n", p->enabled ? 1 : 0);
    fprintf(f, "hours=%s\n", win);
    fprintf(f, "days=%s\n", days);
    fclose(f);
    return 0;
}
