/*
 * wubu_buddy emotion core -- read machine state, expose a continuous affect.
 *
 * Sensor policy (all sysfs, no external commands in the hot path):
 *   CPU package : hwmon entry whose name is coretemp        -> temp1_input
 *   SSD        : hwmon entry named nvme                      -> temp1_input
 *   load       : /proc/loadavg first field, normalised by _SC_NPROCESSORS_ONLN
 *   GPU        : WUBU_BUDDY_GPU_TEMP env when set, else nvidia-smi every 4 s
 * Missing hardware is not an error: channels hold at a neutral value and the
 * mapping still yields a well-defined affect.
 */
#include "wubu_buddy_emotion.h"

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct wubu_buddy_emotion {
    int initialized;
    float cpu, gpu, ssd;      /* degrees C; -1 means unknown */
    float energy, stress, warmth, heat, load;
    long ncpu;
    double last_gpu_query;    /* seconds since process start */
    float  sensor_accum;      /* seconds since the last sensor scan */
};

/* --------------------------------------------------------------- scanning */

static int read_int_file(const char *path, long *out)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    char buf[128];
    if (!fgets(buf, sizeof buf, fp)) { fclose(fp); return 0; }
    fclose(fp);
    char *end = NULL;
    errno = 0;
    long v = strtol(buf, &end, 10);
    if (errno || end == buf) return 0;
    *out = v;
    return 1;
}

/* hwmon reports millidegrees; some platforms report whole degrees. */
static float read_hwmon_c(const char *dir)
{
    long raw = 0;
    if (!read_int_file(dir, &raw)) return -1.0f;
    if (raw > 10000) return raw / 1000.0f;   /* millidegree form */
    return (float)raw;
}

static void scan_hwmon(struct wubu_buddy_emotion *e)
{
    /* Test/CI overrides: fixed sensor inputs make the affect mapping
     * deterministic. Absent env vars use the real hwmon tree below. */
    bool fixed_cpu = false, fixed_ssd = false;
    const char *cpu_env = getenv("WUBU_BUDDY_CPU_TEMP");
    if (cpu_env) {
        char *end = NULL;
        float v = strtof(cpu_env, &end);
        if (end != cpu_env && v >= 0.0f && v < 200.0f) {
            e->cpu = v; fixed_cpu = true;
        }
    }
    const char *ssd_env = getenv("WUBU_BUDDY_SSD_TEMP");
    if (ssd_env) {
        char *end = NULL;
        float v = strtof(ssd_env, &end);
        if (end != ssd_env && v >= 0.0f && v < 200.0f) {
            e->ssd = v; fixed_ssd = true;
        }
    }
    static const char *root = "/sys/class/hwmon";
    DIR *d = opendir(root);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char nbuf[512], path[640], npath[640];
        snprintf(npath, sizeof npath, "%s/%s/name", root, de->d_name);
        FILE *nf = fopen(npath, "r");
        if (!nf) continue;
        size_t n = fread(nbuf, 1, sizeof nbuf - 1, nf);
        fclose(nf);
        while (n && (nbuf[n-1] == '\n' || isspace((unsigned char)nbuf[n-1]))) n--;
        nbuf[n] = '\0';

        snprintf(path, sizeof path, "%s/%s/temp1_input", root, de->d_name);
        float c = read_hwmon_c(path);
        if (c < 0.0f) continue;

        if (!fixed_cpu && strcmp(nbuf, "coretemp") == 0) {
            e->cpu = c;
            /* per-core sensors can be hotter than the package label */
            for (int i = 2; i <= 8; ++i) {
                snprintf(path, sizeof path, "%s/%s/temp%d_input",
                         root, de->d_name, i);
                float p = read_hwmon_c(path);
                if (p > e->cpu) e->cpu = p;
            }
        } else if (!fixed_ssd && strcmp(nbuf, "nvme") == 0) {
            if (c > e->ssd) e->ssd = c;   /* multiple drives: take the warmest */
        }
    }
    closedir(d);
}

static void read_load(struct wubu_buddy_emotion *e)
{
    const char *env = getenv("WUBU_BUDDY_LOAD");
    if (env) {
        char *end = NULL;
        float v = strtof(env, &end);
        if (end != env && v >= 0.0f) {
            e->load = v > 1.0f ? 1.0f : v;
            return;
        }
    }
    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp) { e->load = 0.0f; return; }
    float l1 = 0.0f;
    if (fscanf(fp, "%f %*f %*f", &l1) != 1) l1 = 0.0f;
    fclose(fp);

    long n = e->ncpu > 0 ? e->ncpu : 1;
    float norm = l1 / (float)n;
    if (norm > 1.0f) norm = 1.0f;
    if (norm < 0.0f) norm = 0.0f;
    e->load = norm;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Optional GPU: environment override first (used by tests and WSL), then a
 * cached nvidia-smi query so we do not spawn a process every frame. */
static void read_gpu(struct wubu_buddy_emotion *e)
{
    const char *env = getenv("WUBU_BUDDY_GPU_TEMP");
    if (env) {
        char *end = NULL;
        float v = strtof(env, &end);
        if (end != env && v >= 0.0f && v < 200.0f) e->gpu = v;
        return;
    }

    double t = now_s();
    if (e->initialized && (t - e->last_gpu_query) < 4.0) return;
    e->last_gpu_query = t;

    FILE *pp = popen("nvidia-smi --query-gpu=temperature.gpu "
                     "--format=csv,noheader,nounits 2>/dev/null", "r");
    if (!pp) return;
    char buf[64];
    if (fgets(buf, sizeof buf, pp)) {
        char *end = NULL;
        float v = strtof(buf, &end);
        if (end != buf && v >= 0.0f && v < 200.0f) e->gpu = v;
    }
    pclose(pp);
}

/* ------------------------------------------------------------ affect map */

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

static float ema(float prev, float target, float dt, float tau)
{
    if (dt <= 0.0f) dt = 0.001f;
    float a = dt / (tau + dt);
    if (a > 1.0f) a = 1.0f;
    return prev + (target - prev) * a;
}

static void map_affect(struct wubu_buddy_emotion *e, float dt)
{
    float cpu_excess = (e->cpu < 0.0f) ? 0.30f : (e->cpu - 45.0f) / 45.0f;
    float gpu_excess = (e->gpu < 0.0f) ? 0.15f : (e->gpu - 50.0f) / 30.0f;
    float ssd_excess = (e->ssd < 0.0f) ? 0.10f : (e->ssd - 50.0f) / 25.0f;
    e->heat = clamp01(cpu_excess > gpu_excess ? cpu_excess : gpu_excess);
    float worst = e->heat;
    if (ssd_excess > worst) worst = ssd_excess;
    e->heat = clamp01(worst);

    e->energy = ema(e->energy, clamp01(0.70f * e->load + 0.55f * e->heat),
                    dt, 0.8f);
    e->stress = ema(e->stress,
                    e->heat > 0.55f ? clamp01((e->heat - 0.45f) * 1.8f)
                                    : 0.0f,
                    dt, 1.6f);
    e->warmth = ema(e->warmth,
                    e->cpu > 0.0f ? clamp01((e->cpu - 25.0f) / 75.0f) : 0.30f,
                    dt, 4.0f);
}

/* ------------------------------------------------------------------ API */

int wubu_buddy_emotion_create(wubu_buddy_emotion **out)
{
    if (!out) return 0;
    struct wubu_buddy_emotion *e = calloc(1, sizeof *e);
    if (!e) return 0;
    e->cpu = e->gpu = e->ssd = -1.0f;
    e->energy = 0.20f;
    e->stress = 0.0f;
    e->warmth = 0.20f;
    e->heat = 0.0f;
    e->load = 0.0f;
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    e->ncpu = n > 0 ? n : 1;
    e->last_gpu_query = 0.0;
    *out = e;
    return 1;
}

void wubu_buddy_emotion_destroy(wubu_buddy_emotion *e)
{
    free(e);
}

void wubu_buddy_emotion_poll(wubu_buddy_emotion *e, float dt)
{
    if (!e) return;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 10.0f) dt = 10.0f;

    /* Sensors are sysfs walks (a directory scan plus a file open per zone). The
     * machine state does not change faster than a few Hz, so refresh at most
     * twice a second while still integrating the affect model with the REAL dt
     * on every call -- the orb called this once per blob per frame with a
     * hardcoded 0.5f, i.e. 9 scans a frame at a fake 15x timestep. */
    e->sensor_accum += dt;
    if (!e->initialized || e->sensor_accum >= 0.5f) {
        scan_hwmon(e);
        read_load(e);
        read_gpu(e);
        e->sensor_accum = 0.0f;
    }
    map_affect(e, dt);
    e->initialized = 1;
}

float wubu_buddy_emotion_energy(const wubu_buddy_emotion *e) { return e->energy; }
float wubu_buddy_emotion_stress(const wubu_buddy_emotion *e) { return e->stress; }
float wubu_buddy_emotion_warmth(const wubu_buddy_emotion *e) { return e->warmth; }
float wubu_buddy_emotion_heat(const wubu_buddy_emotion *e)   { return e->heat; }
float wubu_buddy_emotion_load(const wubu_buddy_emotion *e)   { return e->load; }

float wubu_buddy_emotion_mood(const wubu_buddy_emotion *e)
{
    if (e->heat > 0.70f) return WUBU_BUDDY_MOOD_HOT;
    if (e->load > 0.72f && e->heat < 0.50f) return WUBU_BUDDY_MOOD_ALERT;
    if (e->energy > 0.62f) return WUBU_BUDDY_MOOD_EXCITED;
    if (e->energy > 0.38f) return WUBU_BUDDY_MOOD_JOY;
    if (e->load < 0.10f && e->heat < 0.25f) return WUBU_BUDDY_MOOD_SLEEPY;
    return WUBU_BUDDY_MOOD_CALM;
}

float wubu_buddy_emotion_cpu_c(const wubu_buddy_emotion *e) { return e->cpu; }
float wubu_buddy_emotion_gpu_c(const wubu_buddy_emotion *e) { return e->gpu; }
float wubu_buddy_emotion_ssd_c(const wubu_buddy_emotion *e) { return e->ssd; }
