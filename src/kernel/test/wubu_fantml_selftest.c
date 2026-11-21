/*
 * wubu_fantml_selftest.c -- deterministic fan/thermal owner gate.
 *
 * A synthetic hwmon tree pins every input before asserting. This proves the
 * old hardcoded hwmon0 probe could not have found the MSI WMI fans and that
 * the new reader reports exact values plus a named source.
 */
#include "wubu_fantml.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static int passes, fails;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } \
    else { printf("  ok: %s\n", msg); passes++; } \
} while (0)

static void write_file(const char *path, const char *text)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { fprintf(stderr, "cannot write %s\n", path); exit(2); }
    ssize_t n = write(fd, text, strlen(text));
    (void)n;
    close(fd);
}

int main(void)
{
    const char *root = "build/testbin/fanfixture_hwmon4";
    mkdir("build", 0755);
    mkdir("build/testbin", 0755);
    mkdir(root, 0755);
    char hw[512], empty[512];
    snprintf(hw, sizeof(hw), "%s/hwmon4", root);
    snprintf(empty, sizeof(empty), "%s/hwmon0", root);
    mkdir(hw, 0755); mkdir(empty, 0755);
    char src[600];
    snprintf(src, sizeof(src), "%s/name", hw);
    write_file(src, "msi_wmi_platform\n");
    char p[600];
    snprintf(p, sizeof(p), "%s/fan1_input", hw); write_file(p, "2448\n");
    snprintf(p, sizeof(p), "%s/fan2_input", hw); write_file(p, "1867\n");
    snprintf(p, sizeof(p), "%s/fan3_input", hw); write_file(p, "0\n");
    snprintf(p, sizeof(p), "%s/pwm1", hw); write_file(p, "128\n");
    snprintf(p, sizeof(p), "%s/temp1_input", hw); write_file(p, "51000\n");
    snprintf(p, sizeof(p), "%s/name", empty); write_file(p, "empty_driver\n");

    wubu_fantml_probe_root(root);

    printf("=== FANTML deterministic gate ===\n");
    CHECK(wubu_fantml_present() == 1, "fan present from non-hwmon0-agnostic scan");
    CHECK(wubu_fantml_fan_rpm() == 2448, "primary RPM is the max active fan");
    CHECK(wubu_fantml_fan_count() == 3, "three fan inputs counted");
    CHECK(wubu_fantml_fan_pwm() == 128, "PWM duty read verbatim");
    CHECK(strcmp(wubu_fantml_source(), "msi_wmi_platform:fan1") == 0,
          "source names driver and index");
    CHECK(wubu_fantml_fan_pct(2448, 4800) == 51, "fan percentage");
    CHECK(strcmp(wubu_fantml_status_str(-1), "unknown") == 0,
          "absent temperature is unknown, not cool");

    char buf[256];
    wubu_fantml_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "rpm=2448") != NULL, "summary carries real RPM");
    CHECK(strstr(buf, "msi_wmi_platform") != NULL, "summary carries source");

    /* Negative control: no fan inputs at all must fail closed with -1. */
    const char *root2 = "build/testbin/fanfixture_empty";
    mkdir(root2, 0755);
    char eh[512];
    snprintf(eh, sizeof(eh), "%s/hwmon0", root2);
    mkdir(eh, 0755);
    snprintf(p, sizeof(p), "%s/name", eh); write_file(p, "no_fans_here\n");
    wubu_fantml_probe_root(root2);
    CHECK(wubu_fantml_fan_rpm() == -1, "no fan input yields -1, not 0");
    CHECK(wubu_fantml_present() == 0, "no fan means not present");

    printf("=== FANTML TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails ? 1 : 0;
}
