/*
 * wubu_gamemode_test.c -- the game mode / perf governor test.
 *
 * Builds a FAKE sysfs tree under /tmp:
 *   /tmp/gmtest/devices/system/cpu/cpufreq/policy0/scaling_governor = ondemand
 *   /tmp/gmtest/module/nvidia/parameters/boost = 0
 * and asserts:
 *   1. init records the current governor ('ondemand')
 *   2. activate writes 'performance' to the fake policies + boost 1
 *   3. deactivate RESTORES 'ondemand' + boost 0 (never guesses)
 *   4. the active flag toggles correctly
 */
#include "wubu_gamemode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#define ROOT "/tmp/gmtest"

static int mkpath(const char *p)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "mkdir -p '%s'", p);
    return system(buf);
}

static int read_file(const char *p, char *out, size_t cap)
{
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == ' '))
        out[--n] = '\0';
    return 1;
}

static int write_file(const char *p, const char *v)
{
    FILE *f = fopen(p, "w");
    if (!f) return 0;
    fprintf(f, "%s", v);
    fclose(f);
    return 1;
}

static char *gov_path(int i)
{
    static char buf[512];
    snprintf(buf, sizeof(buf),
             ROOT "/devices/system/cpu/cpufreq/policy%d/scaling_governor", i);
    return buf;
}

#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    printf("=== wubu_gamemode_test (the SteamOS game mode) ===\n");
    system("rm -rf " ROOT);

    /* the fake sysfs: two policies on 'ondemand', nvidia boost 0 */
    mkpath(ROOT "/devices/system/cpu/cpufreq/policy0");
    mkpath(ROOT "/devices/system/cpu/cpufreq/policy1");
    mkpath(ROOT "/module/nvidia/parameters");
    write_file(gov_path(0), "ondemand");
    write_file(gov_path(1), "ondemand");
    write_file(ROOT "/module/nvidia/parameters/boost", "0");

    wubu_gamemode_set_sysroot(ROOT);

    /* 1. init records the current state */
    wubu_gamemode_init();
    wubu_gamemode_view_t v;
    wubu_gamemode_get(&v);
    if (v.active) FAIL("active right after init");
    if (strcmp(v.prev_governor, "ondemand") != 0)
        FAIL("prev_governor = '%s', want ondemand", v.prev_governor);
    printf("  PASS: init records the current governor ('%s')\n", v.prev_governor);

    /* 2. activate writes performance to every policy + boost 1 */
    if (wubu_gamemode_activate() != 0) FAIL("activate rc");
    char buf[128];
    if (!read_file(gov_path(0), buf, sizeof(buf)) || strcmp(buf, "performance") != 0)
        FAIL("policy0 = '%s', want performance", buf);
    if (!read_file(gov_path(1), buf, sizeof(buf)) || strcmp(buf, "performance") != 0)
        FAIL("policy1 = '%s', want performance", buf);
    if (!read_file(ROOT "/module/nvidia/parameters/boost", buf, sizeof(buf)) ||
        strcmp(buf, "1") != 0)
        FAIL("boost = '%s', want 1", buf);
    if (!wubu_gamemode_active()) FAIL("active flag off after activate");
    printf("  PASS: activate switches the governors + the nvidia boost\n");

    /* 3. deactivate restores EXACTLY what init recorded */
    if (wubu_gamemode_deactivate() != 0) FAIL("deactivate rc");
    if (!read_file(gov_path(0), buf, sizeof(buf)) || strcmp(buf, "ondemand") != 0)
        FAIL("policy0 = '%s', want ondemand (restored)", buf);
    if (!read_file(ROOT "/module/nvidia/parameters/boost", buf, sizeof(buf)) ||
        strcmp(buf, "0") != 0)
        FAIL("boost = '%s', want 0 (restored)", buf);
    if (wubu_gamemode_active()) FAIL("active flag on after deactivate");
    printf("  PASS: deactivate restores the recorded state\n");

    /* 4. the restore never guesses: a host with NO cpufreq still
     * deactivates cleanly (the state is tracked, the writes are
     * best-effort) */
    system("rm -rf " ROOT);
    mkpath(ROOT "/devices/system/cpu/cpufreq/policy0");
    wubu_gamemode_init();
    wubu_gamemode_activate();
    wubu_gamemode_deactivate();
    printf("  PASS: activate/deactivate on a bare sysfs is clean\n");

    system("rm -rf " ROOT);
    printf("=== ALL GAMEMODE TESTS PASSED (the SteamOS game mode) ===\n");
    return 0;
}
