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
#include <ctype.h>
#include <fcntl.h>

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

/* Memory temperature.  DDR5 platforms expose the SPD hub through an hwmon
 * driver named spd5117 (or sometimes "spd"/"dimm"); some vendor EC firmware
 * publishes a plain "dimm" sensor.  Try each; if none exist the answer is -1,
 * and the UI must say so rather than inventing a number. */
/* Temperature from a sysfs attribute file, in Celsius (millidegrees-aware).
 * Used by the explicit override below. */
static int read_temp_sysfs_attr(const char *path)
{
    long raw = 0;
    if (read_int_file(path, &raw) != 0) return -1;
    return to_celsius(raw);
}

/* Any hwmon sensor whose LABEL says it is memory.  This is deliberately a
 * label search rather than a fixed driver list: vendor drivers name these
 * things differently ("Memory", "DIMM", "DRAM", "SEN4"...), and a label is the
 * only place the kernel says what a sensor actually measures. */
static int read_ram_label_scan(void)
{
    DIR *d = opendir("/sys/class/hwmon");
    if (!d) return -1;

    int found = -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "hwmon", 5) != 0) continue;

        char base[512];
        snprintf(base, sizeof(base), "/sys/class/hwmon/%s", e->d_name);

        /* Read every *_label and look for a memory word next to a temp input. */
        char lpath[520];
        for (int s = 1; s <= 8 && found < 0; s++) {
            snprintf(lpath, sizeof(lpath), "%s/temp%d_label", base, s);
            FILE *lf = fopen(lpath, "r");
            if (!lf) continue;                  /* labels are optional; stop at gap */
            char label[128] = {0};
            int ok = fgets(label, sizeof(label), lf) != NULL;
            fclose(lf);
            if (!ok) break;
            label[strcspn(label, "\n")] = '\0';

            /* Case-insensitive match on the words that mean memory. */
            char lower[sizeof(label)];
            for (size_t i = 0; i < sizeof(lower) - 1 && label[i]; i++)
                lower[i] = (char)tolower((unsigned char)label[i]);
            lower[sizeof(lower) - 1] = '\0';
            if (!strstr(lower, "mem") && !strstr(lower, "dimm") &&
                !strstr(lower, "dram")) continue;

            char tpath[520];
            snprintf(tpath, sizeof(tpath), "%s/temp%d_input", base, s);
            int t = read_temp_sysfs_attr(tpath);
            if (t >= 0) found = t;
        }
    }
    closedir(d);
    return found;
}


/* ---- Embedded Controller (EC) fallback ---------------------------------
 *
 * Windows-side tools such as HWiNFO commonly report memory temperature from
 * the embedded controller, which many Linux kernels never surface through
 * hwmon or the ACPI thermal zones.  This machine is exactly that case: seven
 * hwmon drivers and seven ACPI zones, none of them memory.
 *
 * The EC is reachable two ways, tried in order:
 *
 *   1. /sys/kernel/debug/ec/ec0/io -- the kernel's ec_sys interface.  Needs
 *      the module loaded ("sudo modprobe ec_sys") but then needs no privilege.
 *   2. raw I/O ports 0x62/0x66 -- what the bare-metal kernel does, and what
 *      still works under a privileged hosted process.
 *
 * THE OFFSET IS CONFIGURED, NEVER GUESSED.  WUBU_EC_TEMP_OFFSET names the EC
 * register holding memory temperature.  Without it this returns -1: a random
 * register would produce a confident, plausible, completely meaningless number
 * -- the same failure mode that made the old GPU reading report the ambient
 * zone.  To find the offset: load ec_sys, note HWiNFO's memory temperature, and
 * compare it against the EC dump (ecrw); the matching byte is the offset. */

static int ec_byte(long offset)
{
    /* Path 1: ec_sys debugfs, a plain file. */
    FILE *f = fopen("/sys/kernel/debug/ec/ec0/io", "rb");
    if (f) {
        if (fseek(f, offset, SEEK_SET) == 0) {
            int c = fgetc(f);
            if (c != EOF) { fclose(f); return c; }
        }
        fclose(f);
    }

    /* Path 2: the classic two-port EC protocol.  Write the register address to
     * the command port, then read the data port.  Any refusal (no /dev/port,
     * not permitted) yields -1 -- failing closed is the whole point. */
    int fp = open("/dev/port", O_RDWR);
    if (fp < 0) return -1;

    int ok = -1;
    unsigned char cmd[1], val[1];
    cmd[0] = (unsigned char)offset;
    if (lseek(fp, 0x66, SEEK_SET) == 0x66 && write(fp, cmd, 1) == 1) {
        if (lseek(fp, 0x62, SEEK_SET) == 0x62 && read(fp, val, 1) == 1)
            ok = val[0];
    }
    close(fp);
    return ok;
}

static int read_ec_temp(void)
{
    const char *env = getenv("WUBU_EC_TEMP_OFFSET");
    if (!env || !*env) return -1;          /* no configured offset: say nothing */

    long off = strtol(env, NULL, 0);       /* decimal or 0x-prefixed */
    if (off < 0 || off > 255) return -1;

    int raw = ec_byte(off);
    if (raw < 0) return -1;

    /* EC temperature registers are almost always whole degrees Celsius, unlike
     * the millidegree sysfs convention.  Treat anything above 200 as
     * millidegrees anyway so a driver that does use that unit is still right. */
    if (raw > 200) return (int)((raw + 500) / 1000);
    return raw;
}

static int read_ram_temp(void)
{
    /* 1. Named drivers that publish memory temperature directly. */
    static const char *const dimm_drivers[] = { "spd5117", "spd", "dimm" };
    for (size_t i = 0; i < sizeof(dimm_drivers) / sizeof(dimm_drivers[0]); i++) {
        int t = read_hwmon(dimm_drivers[i], 0, NULL);
        if (t >= 0) return t;
    }

    /* 2. Any sensor that LABELS itself as memory (covers vendor naming). */
    int labelled = read_ram_label_scan();
    if (labelled >= 0) return labelled;

    /* 3. Embedded Controller, at an explicitly configured register offset.
     *    This is where Windows tools get their memory temperature. */
    int ec = read_ec_temp();
    if (ec >= 0) return ec;

    /* 4. Explicit operator override.  Some machines expose RAM temperature
     *    only through an OEM interface (an EC behind WMI, say) that Linux does
     *    not surface.  Point WUBU_RAM_TEMP_SYSFS at ANY sysfs temperature file
     *    and it is used verbatim -- millidegrees or whole degrees, both handled.
     *    This is how such a machine gets an honest reading instead of a lie. */
    const char *override = getenv("WUBU_RAM_TEMP_SYSFS");
    if (override && *override) {
        int t = read_temp_sysfs_attr(override);
        if (t >= 0) return t;
    }

    /* 5. Nothing exposed.  Say so (-1); never invent 0. */
    return -1;
}

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

static int  g_cpu = -1, g_gpu = -1, g_nvme = -1, g_ram = -1;
static int  g_have_cpu = 0, g_have_gpu = 0;
static long g_stamp = 0;

int wubu_thermal_have_cpu(void) { return g_have_cpu; }
int wubu_thermal_have_gpu(void) { return g_have_gpu; }
int wubu_thermal_nvme(void)     { return g_nvme; }
int wubu_thermal_ram(void)      { return g_ram; }

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
    g_ram = read_ram_temp();
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
