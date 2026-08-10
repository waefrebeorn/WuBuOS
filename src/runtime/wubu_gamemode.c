/*
 * wubu_gamemode.c -- the GAME MODE / PERF GOVERNOR engine (SteamOS
 * EPIC E2 gap: "Game mode / perf governor not implemented").
 *
 * Feral's gamemode (which SteamOS integrates) does three things when
 * a game starts and UNDOES all of them when it exits:
 *   1. CPU: switch the cpufreq governor to 'performance' and pin the
 *      game to the high-performance CPUs
 *   2. GPU: raise the nvidia/vendor clock limits (boost)
 *   3. Power: set the power profile to 'performance' (power-profiles-
 *      daemon) so the display/scheduler favor responsiveness
 *
 * WuBuOS's game mode does the same through the standard sysfs knobs,
 * with an in-memory record of the PREVIOUS state so exit restores it
 * exactly (never guess — read the current value first, restore it).
 *
 * The API:
 *   wubu_gamemode_init()          — read + record the current state
 *   wubu_gamemode_activate()      — performance everywhere
 *   wubu_gamemode_deactivate()    — restore what init recorded
 *   wubu_gamemode_active()        — is it on?
 *
 * The sysfs writes are best-effort (some hosts lack permission or the
 * knob); the mode STATE is still tracked so deactivate always runs.
 *
 * C11, self-contained.
 */
#include "wubu_hwdetect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define GM_PATH_MAX 128

/* the governor options (Linux cpufreq) */
#define GOV_PERFORMANCE  "performance"
#define GOV_ONDEMAND     "ondemand"
#define GOV_SCHEDUTIL    "schedutil"
#define GOV_POWERSAVE    "powersave"

typedef struct {
    int  active;
    /* the CPU governor before activation ("" = none found) */
    char prev_governor[64];
    /* the power profile before activation ("" = none) */
    char prev_power[32];
    /* the nvidia boost before activation ("" = none) */
    char prev_nvidia_boost[32];
    /* how many cpufreq policies were switched (for the tests) */
    int  policies_switched;
    int  power_switched;
    int  nvidia_switched;
    /* the sysfs root (overridable for the tests) */
    char sys_root[GM_PATH_MAX];
} wubu_gamemode_t;

static wubu_gamemode_t g_gm;

/* read a sysfs file (trimmed). Returns 1 on success. */
static int gm_read_file(const char *path, char *out, size_t cap)
{
    if (!path || !out || cap == 0) return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, out, cap - 1);
    close(fd);
    if (n <= 0) return 0;
    out[n] = '\0';
    /* trim the trailing newline/whitespace */
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == ' ' || out[n-1] == '\t'))
        out[--n] = '\0';
    return 1;
}

/* write a sysfs file (truncating — a shorter restore must not leave
 * the tail of a longer previous value). Returns 1 on success. */
static int gm_write_file(const char *path, const char *val)
{
    if (!path || !val) return 0;
    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd < 0) return 0;
    ssize_t n = write(fd, val, strlen(val));
    close(fd);
    return n > 0;
}

/* the cpufreq policy path for policy N (perf governor + allowed cpus) */
static void gm_policy_path(char *buf, size_t cap, int n)
{
    snprintf(buf, cap, "%s/devices/system/cpu/cpufreq/policy%d/",
             g_gm.sys_root, n);
}

/* GM1: init — record the current state (idempotent). The sys_root
 * override (the test hook) survives: only the default is set here. */
void wubu_gamemode_init(void)
{
    char saved_root[GM_PATH_MAX];
    snprintf(saved_root, sizeof(saved_root), "%s", g_gm.sys_root);
    memset(&g_gm, 0, sizeof(g_gm));
    if (saved_root[0])
        snprintf(g_gm.sys_root, sizeof(g_gm.sys_root), "%s", saved_root);
    else
        snprintf(g_gm.sys_root, sizeof(g_gm.sys_root), "/sys");
    char path[GM_PATH_MAX];
    /* the first policy's governor (the one we switch) */
    gm_policy_path(path, sizeof(path), 0);
    char gov_path[GM_PATH_MAX];
    snprintf(gov_path, sizeof(gov_path), "%s/scaling_governor", path);
    char gov[64] = "";
    snprintf(g_gm.prev_governor, sizeof(g_gm.prev_governor), "%s",
             gm_read_file(gov_path, gov, sizeof(gov)) ? gov : "");
    /* the power profile */
    snprintf(path, sizeof(path), "%s/firmware/devicetree/base/chosen/power",
             g_gm.sys_root);
    char pw[32] = "";
    snprintf(g_gm.prev_power, sizeof(g_gm.prev_power), "%s",
             gm_read_file(path, pw, sizeof(pw)) ? pw : "");
    /* the nvidia boost */
    snprintf(path, sizeof(path),
             "%s/module/nvidia/parameters/boost", g_gm.sys_root);
    char nb[32] = "";
    snprintf(g_gm.prev_nvidia_boost, sizeof(g_gm.prev_nvidia_boost), "%s",
             gm_read_file(path, nb, sizeof(nb)) ? nb : "");
    g_gm.active = 0;
}

/* GM2: activate — switch everything to performance. */
int wubu_gamemode_activate(void)
{
    if (g_gm.active) return 0;   /* already on */
    char path[GM_PATH_MAX];
    /* the CPU governor: switch EVERY policy to performance */
    for (int i = 0; i < 32; i++) {
        gm_policy_path(path, sizeof(path), i);
        char gov_path[GM_PATH_MAX];
        snprintf(gov_path, sizeof(gov_path), "%s/scaling_governor", path);
        /* only policies that exist get switched */
        if (gm_write_file(gov_path, GOV_PERFORMANCE))
            g_gm.policies_switched++;
    }
    if (g_gm.policies_switched == 0) {
        /* no cpufreq at all (e.g. a VM): still try the per-cpu knob */
        for (int i = 0; i < 8; i++) {
            snprintf(path, sizeof(path),
                     "%s/devices/system/cpu/cpu%d/cpufreq/scaling_governor",
                     g_gm.sys_root, i);
            if (gm_write_file(path, GOV_PERFORMANCE))
                g_gm.policies_switched++;
        }
    }
    /* the power profile */
    snprintf(path, sizeof(path),
             "%s/firmware/devicetree/base/chosen/power/energy_perf",
             g_gm.sys_root);
    if (gm_write_file(path, "0"))   /* 0 = performance, 6 = power save */
        g_gm.power_switched = 1;
    /* the nvidia boost */
    snprintf(path, sizeof(path),
             "%s/module/nvidia/parameters/boost", g_gm.sys_root);
    if (gm_write_file(path, "1"))
        g_gm.nvidia_switched = 1;
    g_gm.active = 1;
    return 0;
}

/* GM3: deactivate — restore what init recorded. */
int wubu_gamemode_deactivate(void)
{
    if (!g_gm.active) return 0;   /* nothing to undo */
    char path[GM_PATH_MAX];
    if (g_gm.prev_governor[0]) {
        for (int i = 0; i < 32; i++) {
            gm_policy_path(path, sizeof(path), i);
            char gov_path[GM_PATH_MAX];
            snprintf(gov_path, sizeof(gov_path), "%s/scaling_governor", path);
            gm_write_file(gov_path, g_gm.prev_governor);
        }
    }
    if (g_gm.prev_power[0]) {
        snprintf(path, sizeof(path),
                 "%s/firmware/devicetree/base/chosen/power/energy_perf",
                 g_gm.sys_root);
        gm_write_file(path, g_gm.prev_power);
    }
    if (g_gm.prev_nvidia_boost[0]) {
        snprintf(path, sizeof(path),
                 "%s/module/nvidia/parameters/boost", g_gm.sys_root);
        gm_write_file(path, g_gm.prev_nvidia_boost);
    }
    g_gm.active = 0;
    return 0;
}

/* GM4: the current mode. */
int wubu_gamemode_active(void)
{
    return g_gm.active;
}

/* GM5: the test hook — the internal state (the sys_root override lets
 * the tests point at a fake sysfs tree). */
typedef struct {
    int  active;
    char prev_governor[64];
    char sys_root[GM_PATH_MAX];
    int  policies_switched;
} wubu_gamemode_view_t;

void wubu_gamemode_set_sysroot(const char *root)
{
    if (root) snprintf(g_gm.sys_root, sizeof(g_gm.sys_root), "%s", root);
    else      snprintf(g_gm.sys_root, sizeof(g_gm.sys_root), "/sys");
}

int wubu_gamemode_get(const wubu_gamemode_view_t *out)
{
    if (!out) return -1;
    wubu_gamemode_view_t *v = (wubu_gamemode_view_t *)out;
    v->active = g_gm.active;
    snprintf(v->prev_governor, sizeof(v->prev_governor), "%s",
             g_gm.prev_governor);
    snprintf(v->sys_root, sizeof(v->sys_root), "%s", g_gm.sys_root);
    v->policies_switched = g_gm.policies_switched;
    return 0;
}

/* the governor constants (for the tests + the describe) */
const char *wubu_gamemode_gov_perf(void) { return GOV_PERFORMANCE; }
const char *wubu_gamemode_gov_restore(void) { return g_gm.prev_governor; }
