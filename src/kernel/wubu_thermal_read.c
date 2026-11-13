/*
 * wubu_thermal_read.c -- see header for the "why".
 *
 * One owner for sensor VALUES.  The kernel driver (wubu_drv_thermal.c) keeps
 * policy and limits; this module owns what the machine's sensors actually say.
 * A 1 Hz cache keeps per-frame calls cheap without freezing a stale number on
 * screen between refreshes.
 */

#include "wubu_thermal_read.h"
#include "wubu_drv_thermal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <time.h>

/* ---- small helpers ------------------------------------------------- */

static int read_int_file(const char *path, long *out)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long v = 0;
    int ok = (fscanf(f, "%ld", &v) == 1);
    fclose(f);
    if (!ok) return -1;
    *out = v;
    return 0;
}

static int read_name(const char *base, char *out, size_t cap)
{
    char path[520];
    snprintf(path, sizeof(path), "%s/name", base);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int ok = fgets(out, (int)cap, f) != NULL;
    fclose(f);
    if (ok) out[strcspn(out, "\n")] = '\0';
    return ok ? 0 : -1;
}

/* Most thermal interfaces report milli-Celsius; a few report whole degrees.
 * Anything above 200 is treated as millidegrees.  Conservative on purpose:
 * dividing a real Celsius value would display 0 C for a hot chip. */
static int to_celsius(long raw)
{
    if (raw > 200) return (int)((raw + 500) / 1000);
    return (int)raw;
}

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

/* ---- hwmon ---------------------------------------------------------- */

/* `match`: NULL means "any", otherwise a substring of the driver name.
 * Exact-match callers (NVMe) use strcmp on the result. */
static int read_hwmon(const char *match, int exact, const char **matched_name)
{
    DIR *d = opendir("/sys/class/hwmon");
    if (!d) return -1;

    int found = -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "hwmon", 5) != 0) continue;

        char base[512], name[64];
        snprintf(base, sizeof(base), "/sys/class/hwmon/%s", e->d_name);
        if (read_name(base, name, sizeof(name)) != 0) continue;

        if (match) {
            int hit = exact ? (strcmp(name, match) == 0)
                            : (strstr(name, match) != NULL);
            if (!hit) continue;
        }

        /* temp1_input is the junction temperature on every driver we care
         * about (coretemp, amdgpu, nouveau, radeon, i915). */
        char tpath[520];
        snprintf(tpath, sizeof(tpath), "%s/temp1_input", base);
        long raw = 0;
        if (read_int_file(tpath, &raw) == 0) {
            found = to_celsius(raw);
            if (matched_name) *matched_name = NULL;   /* caller does not need it */
            break;
        }
    }
    closedir(d);
    return found;
}

static int read_nvme_temp(void) { return read_hwmon("nvme", 1, NULL); }

/* ---- CPU ------------------------------------------------------------ */

static int read_cpu_zone(void)
{
    DIR *d = opendir("/sys/class/thermal");
    if (!d) return -1;

    int best = -1, fallback = -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "thermal_zone", 12) != 0) continue;

        char base[512], type[128] = {0};
        snprintf(base, sizeof(base), "/sys/class/thermal/%s", e->d_name);

        char tpath[520];
        snprintf(tpath, sizeof(tpath), "%s/type", base);
        FILE *tf = fopen(tpath, "r");
        if (!tf) continue;
        if (!fgets(type, sizeof(type), tf)) { fclose(tf); continue; }
        fclose(tf);
        type[strcspn(type, "\n")] = '\0';

        snprintf(tpath, sizeof(tpath), "%s/temp", base);
        long raw = 0;
        if (read_int_file(tpath, &raw) != 0) continue;

        /* x86_pkg_temp is the package sensor -- the number people mean when
         * they ask how hot the processor is.  Take it immediately. */
        if (strstr(type, "x86_pkg_temp")) { best = to_celsius(raw); break; }
        if (fallback < 0) fallback = to_celsius(raw);
        if (best < 0 && (strstr(type, "cpu-thermal") || strstr(type, "cpu_thermal")))
            best = to_celsius(raw);
    }
    closedir(d);
    return best >= 0 ? best : fallback;
}

/* ---- GPU: NVML first, then the open drivers ------------------------- */

typedef int (*nvml_init_fn)(void);
typedef int (*nvml_shutdown_fn)(void);
typedef int (*nvml_count_fn)(unsigned int *);
typedef int (*nvml_handle_fn)(unsigned int, void **);
typedef int (*nvml_temp_fn)(void *, int, unsigned int *);

#define NVML_TEMPERATURE_GPU 0

static void          *g_nvml_lib      = NULL;
static nvml_handle_fn g_nvml_handle   = NULL;
static nvml_temp_fn   g_nvml_temp     = NULL;
static nvml_shutdown_fn g_nvml_shutdown = NULL;
static int            g_nvml_state    = 0;   /* 0 = untried, 1 = ready, -1 = absent */

static int read_gpu_nvml(void)
{
    if (g_nvml_state == -1) return -1;

    if (g_nvml_state == 0) {
        g_nvml_lib = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
        if (!g_nvml_lib) g_nvml_lib = dlopen("libnvidia-ml.so", RTLD_LAZY);
        if (!g_nvml_lib) { g_nvml_state = -1; return -1; }

        nvml_init_fn init = (nvml_init_fn)dlsym(g_nvml_lib, "nvmlInit_v2");
        nvml_count_fn count = (nvml_count_fn)dlsym(g_nvml_lib, "nvmlDeviceGetCount_v2");
        g_nvml_handle = (nvml_handle_fn)dlsym(g_nvml_lib,
                                               "nvmlDeviceGetHandleByIndex_v2");
        g_nvml_temp = (nvml_temp_fn)dlsym(g_nvml_lib, "nvmlDeviceGetTemperature");
        g_nvml_shutdown = (nvml_shutdown_fn)dlsym(g_nvml_lib, "nvmlShutdown");

        unsigned int n = 0;
        if (!init || !count || !g_nvml_handle || !g_nvml_temp ||
            init() != 0 || count(&n) != 0 || n == 0) {
            if (g_nvml_shutdown) g_nvml_shutdown();
            dlclose(g_nvml_lib);
            g_nvml_lib = NULL;
            g_nvml_state = -1;
            return -1;
        }
        g_nvml_state = 1;
    }

    void *dev = NULL;
    if (!g_nvml_handle || g_nvml_handle(0, &dev) != 0 || !dev) return -1;

    unsigned int t = 0;
    if (g_nvml_temp(dev, NVML_TEMPERATURE_GPU, &t) == 0) return (int)t;
    return -1;
}

static int read_gpu(void)
{
    int gpu = read_gpu_nvml();
    if (gpu >= 0) return gpu;

    static const char *const drivers[] = { "amdgpu", "nouveau", "radeon", "i915" };
    for (size_t i = 0; i < sizeof(drivers) / sizeof(drivers[0]); i++) {
        gpu = read_hwmon(drivers[i], 0, NULL);
        if (gpu >= 0) return gpu;
    }
    return -1;
}

/* ---- public state ---------------------------------------------------- */

static int  g_cpu = -1, g_gpu = -1, g_nvme = -1;
static int  g_have_cpu = 0, g_have_gpu = 0;
static long g_stamp = 0;

int wubu_thermal_have_cpu(void) { return g_have_cpu; }
int wubu_thermal_have_gpu(void) { return g_have_gpu; }
int wubu_thermal_nvme(void)     { return g_nvme; }

void wubu_thermal_refresh(void)
{
    g_have_cpu = 0;
    g_have_gpu = 0;

    g_cpu = read_cpu_zone();
    if (g_cpu < 0) g_cpu = read_hwmon("coretemp", 0, NULL);
    if (g_cpu >= 0) g_have_cpu = 1;

    g_gpu = read_gpu();
    if (g_gpu >= 0) g_have_gpu = 1;

    g_nvme = read_nvme_temp();
    g_stamp = now_ms();
}

int wubu_thermal_read_temp(int zone)
{
    /* Rate-limit: these are tiny files, but there is no reason to stat them
     * sixty times a frame. */
    long now = now_ms();
    if (g_stamp == 0 || now - g_stamp > 1000) wubu_thermal_refresh();

    if (zone == WUBU_THERMAL_GPU) return g_gpu;
    return g_cpu;
}
