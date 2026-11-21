/*
 * wubu_fantml.c -- kernel-owned fan + thermal routing.
 *
 * The old probe only checked hardcoded `/sys/class/hwmon/hwmon0/fan1_input`
 * and stored booleans.  On this laptop the fans are published by
 * `msi_wmi_platform` (hwmon4), so the old code reported no fan at all.
 * Now we scan hwmon drivers by name, read REAL RPM/PWM values, and identify
 * the source.  `WUBU_FAN_SYSFS` can point at a fixture root for deterministic
 * host gates; absent/unreadable sensors yield -1, never a fake zero.
 */
#include "../egojar.h"
#include "wubu_fantml.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static int g_have_temp = 0;
static int g_fan_rpm = -1;
static int g_fan_pwm = -1;
static int g_fan_count = 0;
static char g_source[64] = "none";

/* Raw open/read: the hosted test tree links a small libc shim whose FILE*
 * layer is not available to kernel modules. */
static long read_long_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char buf[64] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    return strtol(buf, NULL, 10);
}

static void read_name_file(const char *base, char *out, size_t cap)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/name", base);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, out, (size_t)(cap - 1));
    close(fd);
    if (n > 0) {
        out[n] = '\0';
        out[strcspn(out, "\n")] = '\0';
    }
}

void wubu_fantml_probe_root(const char *root)
{
    if (!root || !*root) root = "/sys/class/hwmon";
    g_have_temp = 0;
    g_fan_rpm = g_fan_pwm = -1;
    g_fan_count = 0;
    snprintf(g_source, sizeof(g_source), "none");

#ifndef WUBU_HOSTED
    /* Bare metal has no sysfs; hardware fan control belongs to the EC/WMI
     * owner, not this hosted reader.  Fail closed. */
    return;
#else
    /* No opendir(): hosted kernel tests link a small arena allocator whose
     * malloc is not initialised, so opendir() can fail after opening the fd.
     * Raw open of a bounded hwmonN range is deterministic and freestanding. */
    for (int slot = 0; slot < 32; slot++) {
        char base[600];
        snprintf(base, sizeof(base), "%s/hwmon%d", root, slot);
        char name[64] = {0};
        read_name_file(base, name, sizeof(name));
        if (!name[0]) continue;

        char path[640];
        for (int i = 1; i <= 8; i++) {
            snprintf(path, sizeof(path), "%s/fan%d_input", base, i);
            long rpm = read_long_file(path);
            if (rpm < 0) continue;
            g_fan_count++;
            if (rpm > g_fan_rpm || g_fan_rpm < 0) {
                g_fan_rpm = (int)rpm;
                snprintf(g_source, sizeof(g_source), "%s:fan%d", name, i);
            }
        }
        for (int i = 1; i <= 4; i++) {
            snprintf(path, sizeof(path), "%s/pwm%d", base, i);
            long pwm = read_long_file(path);
            if (pwm >= 0 && g_fan_pwm < 0) g_fan_pwm = (int)pwm;
        }
        for (int i = 1; i <= 8; i++) {
            snprintf(path, sizeof(path), "%s/temp%d_input", base, i);
            if (read_long_file(path) >= 0) { g_have_temp = 1; break; }
        }
    }
#endif
}

void wubu_fantml_probe(void)
{
#ifndef MYSEED_METAL
    /* Hosted: the env override pins a fixture root for deterministic gates. */
    const char *root = getenv("WUBU_FAN_SYSFS");
#else
    /* Bare metal: no environment exists. Probe the real sysfs root; absent
     * sensors stay -1, never a fabricated zero. */
    const char *root = NULL;
#endif
    wubu_fantml_probe_root(root);
}

int wubu_fantml_present(void)
{
    return g_fan_count > 0 || g_fan_pwm >= 0;
}

int wubu_fantml_fan_rpm(void) { return g_fan_rpm; }
int wubu_fantml_fan_pwm(void) { return g_fan_pwm; }
int wubu_fantml_fan_count(void) { return g_fan_count; }
const char *wubu_fantml_source(void) { return g_source; }

const char *wubu_fantml_status_str(int temp_c)
{
    if (temp_c < 0) return "unknown";
    if (temp_c < 50) return "cool";
    if (temp_c < 70) return "normal";
    if (temp_c < 85) return "warm";
    if (temp_c < 100) return "hot";
    return "critical";
}

int wubu_fantml_fan_pct(int rpm, int max_rpm)
{
    if (max_rpm <= 0 || rpm < 0) return 0;
    return (rpm * 100) / max_rpm;
}

void wubu_fantml_summary(char *out, size_t cap)
{
    snprintf(out, cap, "fantml[temp=%d rpm=%d pwm=%d n=%d src=%s]",
             g_have_temp, g_fan_rpm, g_fan_pwm, g_fan_count, g_source);
}
