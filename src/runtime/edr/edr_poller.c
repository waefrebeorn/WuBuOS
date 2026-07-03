/* edr_poller.c  --  WuBuOS EDR Periodic Poller
 *
 * Periodic /proc scans for telemetry that doesn't have a kernel
 * notification mechanism.
 *
 * Polls every tick of the EDR worker (10ms) but does heavy work
 * at a throttled rate (every 100 ticks = every 1 second):
 *
 * 1. Unix domain socket scan — /proc/net/unix diff detection for
 *    named pipe / unix socket creation (heavener's named pipe pin)
 * 2. Driver / kernel module load scan — /proc/modules diff detection
 *    (heavener's DriverLoad event)
 * 3. Scheduled task scan — /etc/cron* and /etc/systemd/system diff
 *    (heavener's ScheduledTaskCreated event)
 * 4. Proc tree scan — detect running processes not yet in model
 *    (recovery for missed PROC_EVENT_FORK messages)
 */

#include "edr_internal.h"

/* ================================================================
 * State for diff-based detection (first-run baseline)
 * ================================================================ */

static int g_tick_count = 0;
static const int SCAN_INTERVAL = 100;  /* every ~1 second at 10ms ticks */

/* Previous snapshots for diff */
static char g_prev_unix_sockets[65536];
static bool g_prev_unix_init = false;

static char g_prev_modules[65536];
static bool g_prev_modules_init = false;

static char g_prev_cron[65536];
static bool g_prev_cron_init = false;

/* ================================================================
 * /proc/net/unix scanner — detect new unix domain sockets
 * ================================================================ */

static bool unix_scan_diff(const char *current) {
    if (!g_prev_unix_init) {
        strncpy(g_prev_unix_sockets, current, sizeof(g_prev_unix_sockets) - 1);
        g_prev_unix_init = true;
        return false;
    }

    /* Simple line-by-line diff */
    char *prev_copy = strdup(g_prev_unix_sockets);
    if (!prev_copy) return false;

    bool found_new = false;
    char *line, *save;
    char *cur_copy = strdup(current);
    if (!cur_copy) { free(prev_copy); return false; }

    /* For each line in current, check if it exists in prev */
    line = strtok_r(cur_copy, "\n", &save);
    while (line) {
        /* Skip header line */
        if (strncmp(line, "Num", 3) == 0) { line = strtok_r(NULL, "\n", &save); continue; }
        /* Skip abstract sockets */
        if (strstr(line, "@") || strstr(line, "0000")) { line = strtok_r(NULL, "\n", &save); continue; }

        /* Check if this line exists in prev */
        if (strstr(g_prev_unix_sockets, line) == NULL) {
            found_new = true;
            /* Extract path (last field) */
            const char *path = strrchr(line, ' ');
            if (path) {
                while (*path == ' ') path++;
                EdrEvent *ev = calloc(1, sizeof(EdrEvent) + EDR_MAX_PATH);
                if (ev) {
                    ev->header.version = 1;
                    ev->header.type = EDR_EV_NAMED_PIPE_CREATE;
                    ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
                    ev->header.pid = 0;
                    ev->header.size = sizeof(EdrEvent) + EDR_MAX_PATH;
                    ev->header.var_count = 1;
                    char *dst = EDR_EVENT_DATA(ev);
                    snprintf(dst, EDR_MAX_PATH, "unix:%.*s", (int)(sizeof(line) - (size_t)(path - line)), path);
                    edr_queue_push(ev);
                }
            }
        }
        line = strtok_r(NULL, "\n", &save);
    }

    free(cur_copy);
    /* Update prev snapshot */
    strncpy(g_prev_unix_sockets, current, sizeof(g_prev_unix_sockets) - 1);
    free(prev_copy);
    return found_new;
}

static void scan_unix_sockets(void) {
    FILE *f = fopen("/proc/net/unix", "r");
    if (!f) return;

    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return;
    buf[n] = '\0';

    unix_scan_diff(buf);
}

/* ================================================================
 * /proc/modules scanner — detect new kernel modules
 * ================================================================ */

static bool modules_scan_diff(const char *current) {
    if (!g_prev_modules_init) {
        strncpy(g_prev_modules, current, sizeof(g_prev_modules) - 1);
        g_prev_modules_init = true;
        return false;
    }

    bool found_new = false;
    char *cur_copy = strdup(current);
    if (!cur_copy) return false;

    char *line, *save;
    line = strtok_r(cur_copy, "\n", &save);
    while (line) {
        /* Extract module name (first field before space) */
        char mod_name[256];
        const char *sp = strchr(line, ' ');
        if (!sp) { line = strtok_r(NULL, "\n", &save); continue; }
        size_t nlen = (size_t)(sp - line);
        if (nlen > 255) nlen = 255;
        memcpy(mod_name, line, nlen);
        mod_name[nlen] = '\0';

        /* Check if module name exists in prev */
        if (strstr(g_prev_modules, mod_name) == NULL) {
            found_new = true;
            EdrEvent *ev = calloc(1, sizeof(EdrEvent) + EDR_MAX_PATH);
            if (ev) {
                ev->header.version = 1;
                ev->header.type = EDR_EV_DRIVER_LOAD;
                ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
                ev->header.size = sizeof(EdrEvent) + EDR_MAX_PATH;
                ev->header.var_count = 1;
                char *dst = EDR_EVENT_DATA(ev);
                snprintf(dst, EDR_MAX_PATH, "module:%s", mod_name);
                edr_queue_push(ev);
            }
        }
        line = strtok_r(NULL, "\n", &save);
    }

    free(cur_copy);
    strncpy(g_prev_modules, current, sizeof(g_prev_modules) - 1);
    return found_new;
}

static void scan_modules(void) {
    FILE *f = fopen("/proc/modules", "r");
    if (!f) return;

    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return;
    buf[n] = '\0';

    modules_scan_diff(buf);
}

/* ================================================================
 * Cron / systemd timer scanner — detect new scheduled tasks
 * ================================================================ */

static char *read_cron_dir(const char *dir) {
    char *buf = malloc(65536);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t offset = 0;

    DIR *d = opendir(dir);
    if (!d) { free(buf); return NULL; }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        int n = snprintf(buf + offset, 65536 - offset, "%s/%s\n", dir, de->d_name);
        if (n > 0) offset += (size_t)n;
        if (offset >= 65500) break;
    }
    closedir(d);
    return buf;
}

static void scan_scheduled_tasks(void) {
    char *cron_d = read_cron_dir("/etc/cron.d");
    if (!cron_d) return;

    if (!g_prev_cron_init) {
        strncpy(g_prev_cron, cron_d, sizeof(g_prev_cron) - 1);
        g_prev_cron_init = true;
        free(cron_d);
        return;
    }

    /* Diff */
    char *cur_copy = strdup(cron_d);
    free(cron_d);
    if (!cur_copy) return;

    char *line, *save;
    line = strtok_r(cur_copy, "\n", &save);
    while (line) {
        if (strstr(g_prev_cron, line) == NULL) {
            EdrEvent *ev = calloc(1, sizeof(EdrEvent) + EDR_MAX_PATH);
            if (ev) {
                ev->header.version = 1;
                ev->header.type = EDR_EV_SCHEDULED_TASK_CREATED;
                ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
                ev->header.size = sizeof(EdrEvent) + EDR_MAX_PATH;
                ev->header.var_count = 1;
                char *dst = EDR_EVENT_DATA(ev);
                snprintf(dst, EDR_MAX_PATH, "cron:%s", line);
                edr_queue_push(ev);
            }
        }
        line = strtok_r(NULL, "\n", &save);
    }

    free(cur_copy);
    strncpy(g_prev_cron, cur_copy ? cur_copy : "", sizeof(g_prev_cron) - 1);
}

/* ================================================================
 * /proc tree scan — recover missed processes
 * ================================================================ */

static void scan_proc_tree(void) {
    DIR *d = opendir("/proc");
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        pid_t pid = (pid_t)atoi(de->d_name);
        if (pid <= 0) continue;

        /* Check if already in process model */
        pthread_rwlock_rdlock(&g_proc_lock);
        struct EdrProcessInfo *existing = edr_proc_find((uint32_t)pid);
        pthread_rwlock_unlock(&g_proc_lock);

        if (existing) continue;  /* Already tracked */

        /* Read PPID from /proc/pid/status */
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        uint32_t ppid = 0;
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PPid:", 5) == 0) {
                ppid = (uint32_t)atoi(line + 5);
                break;
            }
        }
        fclose(f);

        /* Push recovery PROCESS_CREATE event */
        EdrEvent *ev = calloc(1, sizeof(EdrEvent) + sizeof(uint64_t)*2);
        if (ev) {
            ev->header.version = 1;
            ev->header.type = EDR_EV_PROCESS_CREATE;
            ev->header.pid = (uint32_t)pid;
            ev->header.extra_pid = ppid;
            ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
            ev->header.size = sizeof(EdrEvent) + sizeof(uint64_t)*2;
            ev->header.u64a = (uint64_t)pid;
            edr_queue_push(ev);
        }
    }
    closedir(d);
}

/* ================================================================
 * Public API
 * ================================================================ */

int edr_poller_start(void) {
    g_tick_count = 0;
    g_prev_unix_init = false;
    g_prev_modules_init = false;
    g_prev_cron_init = false;

    /* Do an initial full scan to populate baselines */
    scan_proc_tree();
    scan_unix_sockets();
    scan_modules();

    printf("[edr_poller] periodic poller active (interval=%d ticks)\n", SCAN_INTERVAL);
    return 0;
}

void edr_poller_scan(void) {
    g_tick_count++;
    if (g_tick_count < SCAN_INTERVAL) return;
    g_tick_count = 0;

    /* Throttled scans (every ~1s) */
    scan_unix_sockets();
    scan_modules();
    scan_scheduled_tasks();
    scan_proc_tree();
}

void edr_poller_stop(void) {
    /* Nothing to clean up except reset state */
    g_prev_unix_init = false;
    g_prev_modules_init = false;
    g_prev_cron_init = false;
}