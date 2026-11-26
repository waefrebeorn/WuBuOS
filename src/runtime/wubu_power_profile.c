/*
 * wubu_power_profile.c -- performance / balanced / power-saver, as measured state.
 *
 * WHY THIS EXISTS
 *   The reference desktop (CachyOS) ships power-profiles-daemon: one control that moves the whole
 *   machine between a battery-preserving profile and a latency-first one, exposed as a value the
 *   shell can show. This OS had a game-mode toggle and nothing else, so "performance" was not a
 *   thing the user could select.
 *
 * HOW IT STAYS HONEST
 *   Nothing here reports success because a write returned. Every set() reads the value back and
 *   compares; without privilege it reports DENIED rather than pretending. The sysfs root is
 *   overridable (WUBU_POWER_ROOT) so the test drives a fake tree and asserts the exact bytes the
 *   engine would write -- a path that cannot be exercised is a path nobody knows works.
 *
 * C11, minimal includes, no allocation.
 */
#include "wubu_power_profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CPUS 64

static const char *root(void) {
    const char *r = getenv("WUBU_POWER_ROOT");   /* test seam; checked every call */
    return (r && *r) ? r : "";
}

static const WubuPowerProfileDef kProfiles[] = {
    { WUBU_POWER_SAVER,   "power-saver", "low-power",  "powersave", "save battery first" },
    { WUBU_POWER_BALANCED, "balanced",   "balanced",   "schedutil", "responsive without burning the battery" },
    { WUBU_POWER_PERF,    "performance", "performance", "performance", "latency first: the frame wins" },
};

int wubu_power_profile_count(void) { return (int)(sizeof kProfiles / sizeof kProfiles[0]); }

const WubuPowerProfileDef *wubu_power_profile_def(int i) {
    if (i < 0 || i >= wubu_power_profile_count()) return NULL;
    return &kProfiles[i];
}

/* Read one small sysfs file. Returns bytes read, -1 when absent. Trims the trailing newline
 * because every comparison below is against the value, not the file's formatting. */
static int read_trim(const char *path, char *out, size_t cap) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) out[--n] = '\0';
    return (int)n;
}

int wubu_power_read_platform(char *out, size_t cap) {
    char p[512];
    snprintf(p, sizeof p, "%s/sys/firmware/acpi/platform_profile", root());
    return read_trim(p, out, cap) >= 0 ? 0 : -1;
}

int wubu_power_read_governor(int cpu, char *out, size_t cap) {
    char p[512];
    snprintf(p, sizeof p, "%s/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", root(), cpu);
    return read_trim(p, out, cap) >= 0 ? 0 : -1;
}

int wubu_power_profile_current(void) {
    char gov[64];
    if (wubu_power_read_governor(0, gov, sizeof gov) != 0) return WUBU_POWER_UNKNOWN;
    for (int i = 0; i < wubu_power_profile_count(); i++)
        if (strcmp(gov, kProfiles[i].governor) == 0) return kProfiles[i].id;
    return WUBU_POWER_UNKNOWN;
}

/* Write-then-verify. A file that does not exist is NOT an error: a machine may have no
 * platform_profile or no cpufreq at all (a VM, a desktop CPU pinned by firmware), and reporting
 * that as failure would make the control look broken on hardware where it is simply absent. */
static WubuPowerSetResult write_verify(const char *path, const char *want) {
    char got[128];
    if (read_trim(path, got, sizeof got) < 0) return WUBU_POWER_ABSENT;
    if (strcmp(got, want) == 0) return WUBU_POWER_OK;

    FILE *f = fopen(path, "w");
    if (!f) return WUBU_POWER_DENIED;
    int wrote = (fprintf(f, "%s\n", want) > 0);
    if (fclose(f) != 0 || !wrote) return WUBU_POWER_DENIED;

    if (read_trim(path, got, sizeof got) < 0) return WUBU_POWER_DENIED;
    return strcmp(got, want) == 0 ? WUBU_POWER_OK : WUBU_POWER_REFUSED;
}

WubuPowerSetResult wubu_power_set(int id) {
    const WubuPowerProfileDef *d = NULL;
    for (int i = 0; i < wubu_power_profile_count(); i++)
        if (kProfiles[i].id == id) d = &kProfiles[i];
    if (!d) return WUBU_POWER_REFUSED;

    char p[512];
    int touched = 0, ok = 0;

    snprintf(p, sizeof p, "%s/sys/firmware/acpi/platform_profile", root());
    WubuPowerSetResult r = write_verify(p, d->platform);
    if (r == WUBU_POWER_OK) { touched++; ok++; }
    else if (r == WUBU_POWER_DENIED) return r;      /* privilege: say so, do not half-apply */
    else if (r == WUBU_POWER_REFUSED) return r;

    for (int cpu = 0; cpu < MAX_CPUS; cpu++) {
        char gp[512];
        snprintf(gp, sizeof gp, "%s/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", root(), cpu);
        WubuPowerSetResult g = write_verify(gp, d->governor);
        if (g == WUBU_POWER_ABSENT) break;          /* cpu0 exists, so the tree ends here */
        if (g == WUBU_POWER_DENIED) return g;
        if (g == WUBU_POWER_REFUSED) return g;
        touched++; ok++;
    }

    /* Touched nothing at all: the profile exists in name only on this machine. Saying OK would
     * be the lie; REFUSED is the truth and the caller can show it. */
    return (touched == 0) ? WUBU_POWER_REFUSED : (ok == touched ? WUBU_POWER_OK : WUBU_POWER_REFUSED);
}

const char *wubu_power_result_str(WubuPowerSetResult r) {
    switch (r) {
        case WUBU_POWER_OK:      return "OK";
        case WUBU_POWER_DENIED:  return "DENIED";
        case WUBU_POWER_REFUSED: return "REFUSED";
        case WUBU_POWER_ABSENT:  return "ABSENT";
        default:                 return "UNKNOWN";
    }
}

int wubu_power_describe(char *out, size_t cap) {
    char plat[64] = "n/a", gov[64] = "n/a";
    if (wubu_power_read_platform(plat, sizeof plat) != 0) strcpy(plat, "n/a");
    if (wubu_power_read_governor(0, gov, sizeof gov) != 0) strcpy(gov, "n/a");
    int cur = wubu_power_profile_current();
    return snprintf(out, cap, "profile=%s platform=%s governor=%s",
                    cur == WUBU_POWER_UNKNOWN ? "custom" : wubu_power_profile_def(
                        cur == WUBU_POWER_SAVER ? 0 : cur == WUBU_POWER_BALANCED ? 1 : 2)->name,
                    plat, gov);
}
