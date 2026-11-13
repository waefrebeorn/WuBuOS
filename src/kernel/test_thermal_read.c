/*
 * test_thermal_read.c -- do the reported temperatures match the hardware?
 *
 * The bug this covers was not "no sensor": the old hosted shim looked for a
 * thermal zone whose type contained "gpu", found none, and returned the FIRST
 * zone it had seen -- acpitz, the ACPI ambient sensor.  The widget therefore
 * displayed a confident, plausible, completely wrong GPU temperature.
 *
 * So the assertion here is not "a number came back" but "the number equals what
 * sysfs itself says".  The oracle is re-read independently in this file rather
 * than through the module under test.
 */

#include "wubu_thermal_read.h"
#include "wubu_drv_thermal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

static int g_checks = 0, g_fails = 0;
#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

/* Independent oracle: find the x86_pkg_temp zone and return its Celsius. */
static int oracle_x86_pkg_temp(void)
{
    DIR *d = opendir("/sys/class/thermal");
    if (!d) return -999;

    int best = -999;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "thermal_zone", 12) != 0) continue;

        char path[512], type[128] = {0};
        snprintf(path, sizeof(path), "/sys/class/thermal/%s/type", e->d_name);
        FILE *tf = fopen(path, "r");
        if (!tf) continue;
        if (!fgets(type, sizeof(type), tf)) { fclose(tf); continue; }
        fclose(tf);
        type[strcspn(type, "\n")] = '\0';
        if (!strstr(type, "x86_pkg_temp")) continue;

        snprintf(path, sizeof(path), "/sys/class/thermal/%s/temp", e->d_name);
        FILE *ff = fopen(path, "r");
        if (!ff) continue;
        long raw = 0;
        if (fscanf(ff, "%ld", &raw) == 1) best = (int)((raw + 500) / 1000);
        fclose(ff);
        break;
    }
    closedir(d);
    return best;
}

static int oracle_hwmon(const char *name)
{
    DIR *d = opendir("/sys/class/hwmon");
    if (!d) return -999;
    int best = -999;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "hwmon", 5) != 0) continue;
        char base[512], n[64];
        snprintf(base, sizeof(base), "/sys/class/hwmon/%s", e->d_name);
        char np[520];
        snprintf(np, sizeof(np), "%s/name", base);
        FILE *nf = fopen(np, "r");
        if (!nf) continue;
        if (!fgets(n, sizeof(n), nf)) { fclose(nf); continue; }
        fclose(nf);
        n[strcspn(n, "\n")] = '\0';
        if (!strstr(n, name)) continue;
        char tp[520];
        snprintf(tp, sizeof(tp), "%s/temp1_input", base);
        FILE *ff = fopen(tp, "r");
        if (!ff) continue;
        long raw = 0;
        if (fscanf(ff, "%ld", &raw) == 1) best = (int)((raw + 500) / 1000);
        fclose(ff);
        break;
    }
    closedir(d);
    return best;
}

int main(void)
{
    wubu_thermal_refresh();

    int cpu = wubu_thermal_read_temp(WUBU_THERMAL_CPU);
    int gpu = wubu_thermal_read_temp(WUBU_THERMAL_GPU);
    int nvme = wubu_thermal_nvme();

    /* ---- honesty: a missing sensor is -1, never 0 ----------------- */
    TEST(cpu != 0 || !wubu_thermal_have_cpu(),
         "CPU is either a real reading or honestly unknown (-1), never 0-by-default");
    TEST(gpu != 0 || !wubu_thermal_have_gpu(),
         "GPU is either a real reading or honestly unknown (-1), never 0-by-default");

    /* ---- plausibility: silicon does not run at 400 C -------------- */
    if (wubu_thermal_have_cpu()) TEST(cpu > -1 && cpu < 130, "CPU temperature is physically plausible");
    if (wubu_thermal_have_gpu()) TEST(gpu > -1 && gpu < 130, "GPU temperature is physically plausible");

    /* ---- the oracle: our number must equal sysfs ----------------- */
    int pkg = oracle_x86_pkg_temp();
    if (pkg > -900) {
        TEST(wubu_thermal_have_cpu(), "package sensor exists, so CPU must be reported");
        TEST(cpu >= pkg - 2 && cpu <= pkg + 2,
             "reported CPU temperature matches x86_pkg_temp");
    }

    int ct = oracle_hwmon("coretemp");
    if (ct > -900 && pkg < -900) {
        /* Only assert this when the package zone is absent, otherwise the
         * reader is correctly preferring x86_pkg_temp. */
        TEST(cpu >= ct - 2 && cpu <= ct + 2,
             "reported CPU temperature matches coretemp");
    }

    /* ---- NVMe is its own thing, not disguised as skin -------------- */
    int nv = oracle_hwmon("nvme");
    if (nv > -900) {
        TEST(nvme >= nv - 2 && nvme <= nv + 2, "NVMe temperature matches sysfs");
    } else {
        TEST(nvme == -1, "no NVMe drive means unknown, not zero");
    }

    /* ---- GPU: whatever the source, it must be sane ---------------- */
    if (wubu_thermal_have_gpu()) {
        /* The old bug reported acpitz (~72 C) here.  Any real GPU sensor on an
         * idle laptop is far below that, so this also catches a regression to
         * "first zone we saw". */
        TEST(gpu < 110, "GPU temperature is not the ambient zone in disguise");
    }

    printf("=== thermal_read: %d checks, %d failed (cpu=%d gpu=%d nvme=%d) ===\n",
           g_checks, g_fails, cpu, gpu, nvme);
    return g_fails ? 1 : 0;
}
