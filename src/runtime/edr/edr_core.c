/* edr_core.c  --  WuBuOS EDR Engine Core
 *
 * Lifecycle, module system, alert buffer, FNV-1a hashing,
 * lock-free event queue, worker thread, and public API.
 * Telemetry sources are in separate per-pin modules.
 */

#include "edr_internal.h"
#include <sched.h>
#include <stdatomic.h>

/* ================================================================
 * Process Model — global state
 * ================================================================ */

struct EdrProcessInfo g_processes[EDR_MAX_PROCESSES];
int g_process_count = 0;
uint64_t g_next_uid = 1;
pthread_rwlock_t g_proc_lock = PTHREAD_RWLOCK_INITIALIZER;

struct EdrProcessInfo *edr_proc_find(uint32_t pid) {
    for (int i = 0; i < g_process_count; i++)
        if (g_processes[i].pid == pid && g_processes[i].pid > 0)
            return &g_processes[i];
    return NULL;
}

struct EdrProcessInfo *edr_proc_find_or_add(uint32_t pid, uint32_t ppid) {
    struct EdrProcessInfo *p = edr_proc_find(pid);
    if (p) return p;
    if (g_process_count >= EDR_MAX_PROCESSES) return NULL;
    p = &g_processes[g_process_count++];
    memset(p, 0, sizeof(*p));
    p->uid = g_next_uid++;
    p->pid = pid;
    p->ppid = ppid;
    p->create_time = (uint64_t)time(NULL) * 1000000000ULL;
    /* read /proc/pid/ for enrichment */
    char path[64];
    snprintf(path, sizeof(path), "/proc/%u/comm", pid);
    FILE *f = fopen(path, "r");
    if (f) { fgets(p->name, sizeof(p->name), f); fclose(f);
             size_t n = strlen(p->name); if (n > 0 && p->name[n-1]=='\n') p->name[n-1]=0; }
    snprintf(path, sizeof(path), "/proc/%u/cmdline", pid);
    f = fopen(path, "r");
    if (f) { size_t n = fread(p->cmdline, 1, sizeof(p->cmdline)-1, f);
             p->cmdline[n] = 0; fclose(f); }
    snprintf(path, sizeof(path), "/proc/%u/exe", pid);
    ssize_t r = readlink(path, p->exe_path, sizeof(p->exe_path)-1);
    if (r > 0) p->exe_path[r] = 0;
    /* link parent */
    p->model_parent = edr_proc_find(ppid);
    return p;
}

/* ================================================================
 * Lock-free Event Queue (MPSC)
 * ================================================================ */

EdrEventQueue g_queue = { .head = 0, .tail = 0, .dropped = 0 };

bool edr_queue_push(EdrEvent *ev) {
    uint64_t t = __sync_fetch_and_add(&g_queue.tail, 1);
    if (t - g_queue.head >= EDR_MAX_EVENTS) {
        __sync_fetch_and_sub(&g_queue.tail, 1);
        __sync_fetch_and_add(&g_queue.dropped, 1);
        return false;
    }
    g_queue.buffer[t & (EDR_MAX_EVENTS - 1)] = ev;
    __sync_synchronize();
    return true;
}

EdrEvent *edr_queue_pop(void) {
    if (g_queue.head >= g_queue.tail) return NULL;
    uint64_t h = g_queue.head;
    EdrEvent *ev = g_queue.buffer[h & (EDR_MAX_EVENTS - 1)];
    if (!ev) return NULL;
    __sync_synchronize();
    g_queue.head = h + 1;
    return ev;
}

/* ================================================================
 * Modifier Engine — sliding window of last N files per process
 * ================================================================ */

EdrFileWindow g_file_windows[EDR_MAX_PROCESSES];
int g_file_window_count = 0;

/* ================================================================
 * Alert Buffer
 * ================================================================ */

EdrAlert g_alerts[EDR_ALERT_CAPACITY];
int g_alert_count = 0;

uint64_t fnv1a(const char *key) {
    uint64_t h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char*)key; *p; p++)
        { h ^= *p; h *= 1099511628211ULL; }
    return h;
}

void edr_alert_push(const char *rule, const char *sev,
                     uint32_t pid, const char *module,
                     const char *desc) {
    if (g_alert_count >= EDR_ALERT_CAPACITY) return;
    EdrAlert *a = &g_alerts[g_alert_count++];
    char key[256]; snprintf(key, sizeof(key), "%s-%u-%lu", rule, pid, (unsigned long)time(NULL));
    snprintf(a->id, sizeof(a->id), "%016lx", fnv1a(key));
    snprintf(a->rule_name, sizeof(a->rule_name), "%s", rule);
    snprintf(a->severity, sizeof(a->severity), "%s", sev);
    a->pid = pid; a->timestamp = (uint64_t)time(NULL) * 1000000000ULL;
    snprintf(a->module, sizeof(a->module), "%s", module);
    snprintf(a->description, sizeof(a->description), "%s", desc);
    /* build process chain */
    pthread_rwlock_rdlock(&g_proc_lock);
    struct EdrProcessInfo *p = edr_proc_find(pid);
    char chain[2048] = {0};
    int depth = 0;
    while (p && depth < 16) {
        char seg[256];
        snprintf(seg, sizeof(seg), "[%u] %s", p->pid, p->name);
        if (chain[0]) { char tmp[2048]; snprintf(tmp, sizeof(tmp), "%s <- %s", seg, chain);
                       memcpy(chain, tmp, sizeof(chain)); }
        else snprintf(chain, sizeof(chain), "%s", seg);
        p = p->model_parent; depth++;
    }
    snprintf(a->process_chain, sizeof(a->process_chain), "%s", chain);
    pthread_rwlock_unlock(&g_proc_lock);
}

/* ================================================================
 * Module Globals
 * ================================================================ */

EdrModule *g_modules[EDR_MAX_MODULES];
int g_module_count = 0;
EdrModule *g_active_module = NULL;

void edr_register_module(EdrModule *mod) {
    if (g_module_count < EDR_MAX_MODULES)
        g_modules[g_module_count++] = mod;
}

int edr_switch_module(const char *name) {
    for (int i = 0; i < g_module_count; i++) {
        if (strcmp(g_modules[i]->name, name) == 0) {
            if (g_active_module) g_active_module->shutdown(g_active_module);
            g_active_module = g_modules[i];
            char cfg[EDR_MAX_PATH];
            snprintf(cfg, sizeof(cfg), "%s/%s", EDR_CONFIG_PATH, name);
            return g_active_module->init(g_active_module, cfg);
        }
    }
    return -1;
}

/* ================================================================
 * Built-in Detection Modules
 * ================================================================ */

/* --- YARA Module (static + behavioral) --- */
static int yara_module_init(EdrModule *self, const char *cfg) {
    (void)self; (void)cfg; return 0;
}
static void yara_module_shutdown(EdrModule *self) { (void)self; }
static int yara_module_scan(EdrModule *self, const uint8_t *data,
                             size_t len, const char *path,
                             char *verdict, size_t vlen) {
    (void)self; (void)data; (void)len; (void)path;
    snprintf(verdict, vlen, "clean"); return 0;
}
static void yara_module_on_event(EdrModule *self, const void *hdr,
                                  const void *data, const struct EdrProcessInfo *proc) {
    (void)self; (void)hdr; (void)data; (void)proc;
}
static int yara_module_drain(EdrModule *self, EdrAlert *out, int max) {
    (void)self; (void)out; (void)max; return 0;
}
static EdrModule g_yara_module = {
    "yara", "0.1.0", 7,
    yara_module_init, yara_module_shutdown,
    yara_module_scan, yara_module_on_event, yara_module_drain
};

/* --- Behavioral Rules Module --- */
static int behavioral_module_init(EdrModule *self, const char *cfg) {
    (void)self; (void)cfg; return 0;
}
static void behavioral_module_shutdown(EdrModule *self) { (void)self; }
static int behavioral_module_scan(EdrModule *self, const uint8_t *data,
                                   size_t len, const char *path,
                                   char *verdict, size_t vlen) {
    (void)self; (void)data; (void)len; (void)path;
    snprintf(verdict, vlen, "clean"); return 0;
}

/* Built-in rules */
static void rule_ppid_spoof(EdrModule *self, const EdrEventHeader *hdr,
                             const struct EdrProcessInfo *proc) {
    (void)self;
    if (!proc || hdr->type != EDR_EV_PROCESS_CREATE) return;
    if (hdr->extra_pid && hdr->extra_pid != proc->ppid) {
        char desc[512];
        snprintf(desc, sizeof(desc),
                 "Parent PID spoofing detected: pid=%u reports PPID=%u "
                 "but was actually created by %u",
                 proc->pid, proc->ppid, hdr->extra_pid);
        edr_alert_push("ppid_spoof", "malicious", proc->pid,
                       "behavioral", desc);
    }
}

static void rule_lsass_handle_read(EdrModule *self, const EdrEventHeader *hdr,
                                    const struct EdrProcessInfo *proc) {
    (void)self;
    if (hdr->type != EDR_EV_PROCESS_HANDLE_ACCESS) return;
    if (!proc) return;
    if (strstr(proc->name, "lsass") && hdr->u32) {
        char desc[512];
        snprintf(desc, sizeof(desc),
                 "Cross-process handle to lsass with VM_READ: "
                 "pid=%u name=%s", proc->pid, proc->name);
        edr_alert_push("lsass_handle_read", "malicious",
                       proc->pid, "behavioral", desc);
    }
}

static void rule_file_burst(EdrModule *self, const EdrEventHeader *hdr,
                             const struct EdrProcessInfo *proc) {
    (void)self;
    if (hdr->type != EDR_EV_FILE_CREATE) return;
    if (!proc) return;
    struct EdrProcessInfo *p = (struct EdrProcessInfo*)proc;
    int *count = NULL;
    for (int i = 0; i < p->ext_count; i++)
        if (p->ext[i].key == 0x46494C45) { count = (int*)&p->ext[i].val; break; }
    if (!count && p->ext_count < 16) {
        p->ext[p->ext_count].key = 0x46494C45;
        count = (int*)&p->ext[p->ext_count].val;
        *count = 0;
        p->ext_count++;
    }
    if (count) {
        (*count)++;
        if (*count > 50) {
            char desc[512];
            snprintf(desc, sizeof(desc),
                     "Rapid file creation burst (%d files) by pid=%u "
                     "name=%s — possible ransomware",
                     *count, proc->pid, proc->name);
            edr_alert_push("file_burst", "suspicious",
                           proc->pid, "behavioral", desc);
            *count = 0;
        }
    }
}

static void behavioral_module_on_event(EdrModule *self,
                                        const void *hdr,
                                        const void *data,
                                        const struct EdrProcessInfo *proc) {
    (void)data;
    const EdrEventHeader *ev_hdr = (const EdrEventHeader *)hdr;
    rule_ppid_spoof(self, ev_hdr, proc);
    rule_lsass_handle_read(self, ev_hdr, proc);
    rule_file_burst(self, ev_hdr, proc);
}

static int behavioral_module_drain(EdrModule *self, EdrAlert *out, int max) {
    (void)self; (void)out; (void)max; return 0;
}
static EdrModule g_behavioral_module = {
    "behavioral", "0.1.0", 2,
    behavioral_module_init, behavioral_module_shutdown,
    behavioral_module_scan, behavioral_module_on_event,
    behavioral_module_drain
};

/* ================================================================
 * Replay System
 * ================================================================ */

int edr_replay(const char *json_path) {
    FILE *f = fopen(json_path, "r");
    if (!f) return -1;
    char line[65536];
    while (fgets(line, sizeof(line), f)) {
        /* JSONL parser stub — full implementation uses wubu_oci JSON helpers */
        (void)line;
    }
    fclose(f);
    return 0;
}

/* ================================================================
 * Telemetry Source File Descriptors
 * ================================================================ */

int g_fanotify_fd = -1;
int g_inotify_fd = -1;
int g_proc_connector_fd = -1;
int g_netfilter_fd = -1;
int g_udev_fd = -1;

/* ================================================================
 * Worker Thread
 * ================================================================ */

pthread_t g_worker;
volatile bool g_running = false;

static void *edr_worker_loop(void *arg) {
    (void)arg;
    while (g_running) {
        EdrEvent *ev;
        int drained = 0;
        /* Drain event batch */
        while ((ev = edr_queue_pop()) != NULL && drained < EDR_BATCH_SIZE) {
            /* Stage 1: Update ProcessModel */
            if (ev->header.type == EDR_EV_PROCESS_CREATE) {
                pthread_rwlock_wrlock(&g_proc_lock);
                edr_proc_find_or_add(ev->header.pid, ev->header.extra_pid);
                pthread_rwlock_unlock(&g_proc_lock);
            }

            /* Stage 2: Get process context */
            pthread_rwlock_rdlock(&g_proc_lock);
            struct EdrProcessInfo *proc = edr_proc_find(ev->header.pid);

            /* Stage 3: Run active module */
            if (g_active_module && g_active_module->on_event)
                g_active_module->on_event(g_active_module, &ev->header,
                                           EDR_EVENT_DATA(ev), proc);
            pthread_rwlock_unlock(&g_proc_lock);

            free(ev);
            drained++;
        }

        /* Poll telemetry sources */
        edr_fanotify_poll();
        edr_proc_pin_poll();
        edr_poller_scan();

        if (!drained) usleep(10000); /* 10ms idle */
    }
    return NULL;
}

/* ================================================================
 * Public Lifecycle API
 * ================================================================ */

/* Forward decl so edr_start() can call the analytics loader defined below. */
static void edr_analytics_load(void);

int edr_start(void) {
    g_running = true;
    edr_analytics_load();   /* honor persisted master toggle */
    edr_register_module(&g_yara_module);
    edr_register_module(&g_behavioral_module);
    g_active_module = &g_behavioral_module;
    char cfg[EDR_MAX_PATH];
    snprintf(cfg, sizeof(cfg), "%s/behavioral", EDR_CONFIG_PATH);
    g_active_module->init(g_active_module, cfg);

    /* Start all telemetry pins */
    edr_fanotify_start();
    edr_proc_pin_start();
    edr_poller_start();

    pthread_create(&g_worker, NULL, edr_worker_loop, NULL);
    return 0;
}

void edr_stop(void) {
    g_running = false;
    pthread_join(g_worker, NULL);
    if (g_active_module) g_active_module->shutdown(g_active_module);

    edr_fanotify_stop();
    edr_proc_pin_stop();
    edr_poller_stop();
}

/* ================================================================
 * Agent (AGI) transparency -- master analytics toggle + event logging
 * ================================================================ */

/* Master analytics switch (the "giant toggle"). Default ON. When OFF, no
 * agent/UI-automation events are recorded; by policy the user then forfeits
 * debug-report / bug-fix eligibility (they are no longer in the corpus). */
static volatile bool g_analytics_enabled = true;
static uint64_t      g_agent_events_logged = 0;

bool edr_analytics_enabled(void) { return g_analytics_enabled; }

void edr_analytics_set_enabled(bool on) {
    g_analytics_enabled = on;
    /* Persist the choice so it survives reboot (best-effort). */
    char path[EDR_MAX_PATH];
    snprintf(path, sizeof(path), "%s/analytics", EDR_CONFIG_PATH);
    FILE *f = fopen(path, "w");
    if (f) { fputc(on ? '1' : '0', f); fclose(f); }
}

/* Load the persisted toggle (called from edr_start). */
static void edr_analytics_load(void) {
    char path[EDR_MAX_PATH];
    snprintf(path, sizeof(path), "%s/analytics", EDR_CONFIG_PATH);
    FILE *f = fopen(path, "r");
    if (f) {
        int c = fgetc(f); fclose(f);
        g_analytics_enabled = (c != '0');
    }
}

int edr_log_event(uint16_t type, uint32_t pid, uint32_t extra_pid,
                  uint64_t u64a, uint64_t u64b, uint32_t u32,
                  const char *detail) {
    if (!g_analytics_enabled) return -1;   /* master toggle gates everything */


    size_t dlen = detail ? strlen(detail) + 1 : 1;
    EdrEvent *ev = (EdrEvent *)malloc(sizeof(EdrEventHeader) + dlen);
    if (!ev) return -1;
    memset(ev, 0, sizeof(EdrEventHeader));
    ev->header.version   = 1;
    ev->header.type      = type;
    ev->header.size      = (uint32_t)(sizeof(EdrEventHeader) + dlen);
    ev->header.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
    ev->header.pid       = pid;
    ev->header.tid       = 0;
    ev->header.extra_pid = extra_pid;
    ev->header.u64a      = u64a;
    ev->header.u64b      = u64b;
    ev->header.u32       = u32;
    ev->header.var_count = detail ? 1 : 0;
    if (detail) memcpy(EDR_EVENT_DATA(ev), detail, dlen);

    g_agent_events_logged++;
    if (!edr_queue_push(ev)) { free(ev); return -1; }
    return 0;
}

int edr_log_agent_action(uint16_t action, int x, int y, int btn,
                          uint32_t key, const char *detail) {
    char buf[256];
    if (!detail) {
        snprintf(buf, sizeof(buf), "agent:%u x=%d y=%d btn=%d key=%u",
                 action, x, y, btn, key);
        detail = buf;
    }
    /* u64a = packed cursor (x<<32 | y); u32 = action sub-type. */
    uint64_t cur = ((uint64_t)(uint32_t)x << 32) | (uint32_t)y;
    return edr_log_event(EDR_EV_AGENT_ACTION, (uint32_t)getpid(), 0,
                         cur, (uint64_t)key, action, detail);
}

/* Re-export for tests that want the count without draining the queue. */
uint64_t edr_agent_events_logged(void) { return g_agent_events_logged; }

/* ================================================================
 * Public Query API
 * ================================================================ */

int edr_get_alerts(EdrAlert *buf, int max) {
    int n = g_alert_count < max ? g_alert_count : max;
    memcpy(buf, g_alerts, n * sizeof(EdrAlert));
    return n;
}

int edr_get_process_count(void) {
    pthread_rwlock_rdlock(&g_proc_lock);
    int n = g_process_count;
    pthread_rwlock_unlock(&g_proc_lock);
    return n;
}

const struct EdrProcessInfo *edr_get_process(uint32_t pid) {
    pthread_rwlock_rdlock(&g_proc_lock);
    struct EdrProcessInfo *p = edr_proc_find(pid);
    pthread_rwlock_unlock(&g_proc_lock);
    return p;
}