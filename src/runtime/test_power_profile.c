/*
 * test_power_profile.c -- the power profile engine against a FAKE sysfs tree.
 *
 * Every assertion here would pass on a real machine by accident (the files exist and the values
 * are already correct), so the test builds its own tree under WUBU_POWER_ROOT and drives the
 * engine through three states a real machine can be in: writable, privilege-denied, and absent.
 *
 * C11.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wubu_power_profile.h"

static int g_pass, g_fail;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) { g_pass++; }                                               \
        else { g_fail++; fprintf(stderr, "  FAIL: %s\n", (msg)); }            \
    } while (0)

static const char *g_root = "/tmp/wubu-power-test";

static void mkfile(const char *rel, const char *val, mode_t mode) {
    char p[512], dir[512];
    snprintf(p, sizeof p, "%s/%s", g_root, rel);
    snprintf(dir, sizeof dir, "%s", p);
    for (char *s = dir + 1; *s; s++)
        if (*s == '/') { *s = '\0'; mkdir(dir, 0755); *s = '/'; }
    FILE *f = fopen(p, "w");
    if (!f) { perror(p); exit(1); }
    fprintf(f, "%s\n", val);
    fclose(f);
    chmod(p, mode);
}

static void rmrf(const char *p) {
    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", p);
    if (system(cmd) != 0) fprintf(stderr, "warn: could not clean %s\n", p);
}

static void build_tree(mode_t gov_mode) {
    rmrf(g_root);
    mkfile("sys/firmware/acpi/platform_profile", "balanced", 0644);
    mkfile("sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "schedutil", gov_mode);
    mkfile("sys/devices/system/cpu/cpu1/cpufreq/scaling_governor", "schedutil", gov_mode);
}

int main(void) {
    setenv("WUBU_POWER_ROOT", g_root, 1);

    printf("+--------------------------------------------------------------+\n");
    printf("|  Power profiles (power-saver / balanced / performance)        |\n");
    printf("+--------------------------------------------------------------+\n");

    /* -- the table is real data, not one hardcoded mode -- */
    CHECK(wubu_power_profile_count() == 3, "three profiles");
    CHECK(wubu_power_profile_def(0) && strcmp(wubu_power_profile_def(0)->governor, "powersave") == 0,
          "power-saver maps to the powersave governor");
    CHECK(wubu_power_profile_def(2) && strcmp(wubu_power_profile_def(2)->platform, "performance") == 0,
          "performance maps to the performance platform profile");
    CHECK(wubu_power_profile_def(3) == NULL, "out-of-range index is refused");

    /* -- read the machine as it is -- */
    build_tree(0644);
    char buf[128];
    CHECK(wubu_power_read_governor(0, buf, sizeof buf) == 0 && strcmp(buf, "schedutil") == 0,
          "reads the governor without the trailing newline");
    CHECK(wubu_power_profile_current() == WUBU_POWER_BALANCED,
          "schedutil reads back as the balanced profile");

    /* -- set: written AND verified -- */
    WubuPowerSetResult r = wubu_power_set(WUBU_POWER_PERF);
    CHECK(r == WUBU_POWER_OK, "performance applies");
    CHECK(wubu_power_read_governor(0, buf, sizeof buf) == 0 && strcmp(buf, "performance") == 0,
          "cpu0 governor is performance after the set");
    CHECK(wubu_power_read_governor(1, buf, sizeof buf) == 0 && strcmp(buf, "performance") == 0,
          "cpu1 governor is performance too (every policy, not just cpu0)");
    CHECK(wubu_power_read_platform(buf, sizeof buf) == 0 && strcmp(buf, "performance") == 0,
          "platform profile is performance");
    CHECK(wubu_power_profile_current() == WUBU_POWER_PERF,
          "current() agrees with the files after the set");

    /* -- already correct is OK, not a failure -- */
    CHECK(wubu_power_set(WUBU_POWER_PERF) == WUBU_POWER_OK, "setting the current profile is OK");

    /* -- no privilege: DENIED, never a silent no-op -- */
    build_tree(0444);
    r = wubu_power_set(WUBU_POWER_SAVER);
    CHECK(r == WUBU_POWER_DENIED, "a read-only governor reports DENIED");
    CHECK(wubu_power_read_governor(0, buf, sizeof buf) == 0 && strcmp(buf, "schedutil") == 0,
          "and the value is unchanged (no half-apply)");

    /* -- nothing to control: REFUSED, not OK -- */
    rmrf(g_root);
    mkdir(g_root, 0755);
    r = wubu_power_set(WUBU_POWER_SAVER);
    CHECK(r == WUBU_POWER_REFUSED, "a machine with no cpufreq reports REFUSED");
    CHECK(strcmp(wubu_power_result_str(WUBU_POWER_REFUSED), "REFUSED") == 0, "result names itself");
    CHECK(wubu_power_profile_current() == WUBU_POWER_UNKNOWN, "no governor reads as UNKNOWN");

    rmrf(g_root);
    printf("\n  Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
