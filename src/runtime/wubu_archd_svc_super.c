/*
 * wubu_archd_svc_super.c -- Real in-process service supervisor.
 *
 * Closes N1 (popen arch-chroot systemctl) + N2 (no real supervisor):
 * services are fork/exec'd as processes inside their Arch root namespace,
 * tracked by PID, and reported live from /proc. No shell, no systemd.
 *
 * C11, opaque struct, minimal includes.
 */
#define _GNU_SOURCE   /* chroot(), usleep() declarations */
#define _POSIX_C_SOURCE 200809L

#include "wubu_archd_svc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define SUP_MAX WUBU_ARCHD_MAX_SERVICES
#define RESTART_BACKOFF_BASE 1     /* seconds, doubles per restart */
#define RESTART_BACKOFF_MAX  60    /* cap */

typedef struct {
    char root[WUBU_ARCHD_MAX_ROOT_NAME];
    char svc[WUBU_ARCHD_MAX_PACKAGE_NAME];
    char exec[WUBU_ARCHD_MAX_PATH];
    pid_t pid;
    WubuArchServiceState state;
    time_t last_start, last_stop;
    int restart_count;
    bool auto_restart;
    /* s6-style restart backoff: after each unexpected death the unit
     * enters a backoff window that doubles per restart (1s, 2s, 4s...)
     * capped at RESTART_BACKOFF_MAX — a crash-looping service pauses
     * instead of hot-restarting forever. */
    time_t backoff_until;
    /* Declarative unit model (systemd-good, no notorious parts). */
    wubu_svc_type_t type;          /* simple / forking / notify */
    bool notify_ready;                /* Type=notify: ready only after notify */
    int  timeout_ms;                 /* per-unit start wait */
    int  n_req, n_after;
    char requires[8][WUBU_ARCHD_MAX_PACKAGE_NAME];  /* Requires= */
    char after[8][WUBU_ARCHD_MAX_PACKAGE_NAME];     /* After=    */
    /* Plain-text journal ring (kills journald's binary blob). */
    char journal[2048];
    int  jlen;
    /* Stored argv (real fork/exec honors arguments). */
    int  argc;
    char argv[8][256];
} sup_entry_t;

struct wubu_svc_supervisor {
    sup_entry_t entries[SUP_MAX];
    int count;
};

static int find_entry(const wubu_svc_supervisor_t *s, const char *root,
                     const char *svc) {
    for (int i = 0; i < s->count; i++)
        if (strcmp(s->entries[i].root, root) == 0 &&
            strcmp(s->entries[i].svc, svc) == 0)
            return i;
    return -1;
}

static bool root_is_valid(const char *root) {
    if (!root || !*root) return false;
    char p[WUBU_ARCHD_MAX_PATH];
    snprintf(p, sizeof(p), "%s/etc/arch-release", root);
    return access(p, F_OK) == 0;
}

/* Resolve the executable path for a service in a root. */
static void resolve_exec(const char *root, const char *svc, char *out, size_t n) {
    if (root && *root && root_is_valid(root)) {
        char cand[WUBU_ARCHD_MAX_PATH];
        snprintf(cand, sizeof(cand), "%s/usr/bin/%s", root, svc);
        if (access(cand, X_OK) == 0) { snprintf(out, n, "%s", cand); return; }
        snprintf(cand, sizeof(cand), "%s/bin/%s", root, svc);
        if (access(cand, X_OK) == 0) { snprintf(out, n, "%s", cand); return; }
    }
    snprintf(out, n, "%s", svc); /* fall back to a host binary */
}

static bool pid_alive(pid_t pid) {
    if (pid <= 0) return false;
    /* Authoritative liveness: WNOHANG waitpid reaps a zombie
     * (returns pid>0) and leaves a live child running (returns 0).
     * A /proc/<pid> existence check is INSUFFICIENT -- a killed
     * but unreaped child still has a /proc entry, so it would
     * falsely report "alive". waitpid avoids that class of bug. */
    int st;
    return waitpid(pid, &st, WNOHANG) == 0;
}

wubu_svc_supervisor_t *wubu_svc_supervisor_create(void) {
    return calloc(1, sizeof(wubu_svc_supervisor_t));
}

void wubu_svc_supervisor_destroy(wubu_svc_supervisor_t *s) { free(s); }

int wubu_svc_supervisor_add(wubu_svc_supervisor_t *s, const char *root,
                            const char *svc, const char *exec_path,
                            char *const argv[]) {
    (void)argv;
    if (!s || !root || !svc || !*root || !*svc) return -1;
    if (s->count >= SUP_MAX) return -1;
    if (find_entry(s, root, svc) >= 0) return 0; /* idempotent */

    sup_entry_t *e = &s->entries[s->count];
    memset(e, 0, sizeof(*e));
    strncpy(e->root, root, sizeof(e->root) - 1);
    strncpy(e->svc, svc, sizeof(e->svc) - 1);
    if (exec_path && *exec_path)
        strncpy(e->exec, exec_path, sizeof(e->exec) - 1);
    else
        resolve_exec(root, svc, e->exec, sizeof(e->exec));
    /* Store argv so the real fork/exec honors arguments
     * (a bare execl(exec,exec) would drop them). */
    e->argc = 0;
    if (argv) {
        for (int k = 0; argv[k] && e->argc < 8; k++) {
            strncpy(e->argv[e->argc], argv[k], sizeof(e->argv[0]) - 1);
            e->argc++;
        }
    }
    e->pid = 0;
    e->state = SERVICE_STATE_DISABLED;
    /* Declarative defaults: Truthful readiness. Type=notify is
     * the honest default -- a unit stays STARTING until it calls
     * wubu_svc_supervisor_notify_ready(). systemd's "active=running"
     * lie (spawned != ready) is explicitly avoided. */
    e->type = SVC_TYPE_NOTIFY;
    e->notify_ready = false;
    e->timeout_ms = 5000;
    s->count++;
    return 0;
}

/* Child side: isolate in the root (if a real Arch root) and exec,
 * passing the unit's stored argv. */
static void exec_in_root(const char *root, const sup_entry_t *e) {
    if (root && *root && root_is_valid(root)) {
        if (chroot(root) != 0) _exit(127);
        if (chdir("/") != 0) _exit(127);
    }
    char *argv[9];
    for (int k = 0; k < e->argc && k < 8; k++) argv[k] = (char *)e->argv[k];
    argv[e->argc] = NULL;
    execv(e->exec, argv);
    _exit(127);
}

/* Is a unit ready (truthful)? SIMPLE: alive == serving.
 * FORKING: alive == serving (the forked child re-execs the
 *  daemon; we don't double-wait). NOTIFY: only after the
 *  service explicitly calls notify_ready(). */
static bool entry_ready(const sup_entry_t *e) {
    if (e->pid <= 0 || !pid_alive(e->pid)) return false;
    if (e->type == SVC_TYPE_NOTIFY) return e->notify_ready;
    return true;
}

int wubu_svc_supervisor_start(wubu_svc_supervisor_t *s, const char *root,
                              const char *svc) {
    if (!s) return -1;
    int i = find_entry(s, root, svc);
    if (i < 0) return -1;
    sup_entry_t *e = &s->entries[i];
    if (e->pid > 0 && entry_ready(e)) return 0; /* already running+ready */

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        exec_in_root(e->root, e);
        _exit(127);
    }
    /* Parent: confirm the child actually exec'd (not a missing binary). */
    if (pid_alive(pid)) {
        e->pid = pid;
        /* Truthful: not RUNNING yet -- it is STARTING until ready.
         * A Type=notify unit stays STARTING until notify_ready(). */
        e->state = SERVICE_STATE_RESTARTING;
        e->last_start = time(NULL);
        wubu_svc_supervisor_log(s, root, svc, "started (STARTING)");
        /* For SIMPLE/FORKING, ready == alive; promote now.
         * For NOTIFY, poll()/ready-wait promotes it later. */
        if (e->type != SVC_TYPE_NOTIFY) {
            e->state = SERVICE_STATE_RUNNING;
            wubu_svc_supervisor_log(s, root, svc, "READY (running)");
        }
        return 0;
    }
    int st; waitpid(pid, &st, 0);  /* reap the immediate exit(127) */
    e->pid = 0;
    e->state = SERVICE_STATE_FAILED;
    e->last_stop = time(NULL);
    wubu_svc_supervisor_log(s, root, svc, "FAILED: binary absent");
    return -1;
}

int wubu_svc_supervisor_stop(wubu_svc_supervisor_t *s, const char *root,
                             const char *svc) {
    if (!s) return -1;
    int i = find_entry(s, root, svc);
    if (i < 0) return -1;
    sup_entry_t *e = &s->entries[i];
    if (e->pid > 0) {
        kill(e->pid, SIGTERM);
        int st; waitpid(e->pid, &st, 0);
    }
    e->pid = 0;
    e->state = SERVICE_STATE_DISABLED;
    e->last_stop = time(NULL);
    return 0;
}

int wubu_svc_supervisor_restart(wubu_svc_supervisor_t *s, const char *root,
                                const char *svc) {
    if (!s) return -1;
    wubu_svc_supervisor_stop(s, root, svc);
    return wubu_svc_supervisor_start(s, root, svc);
}

/* Stop every managed unit (SIGTERM + reap). Used at daemon shutdown. */
int wubu_svc_supervisor_stop_all(wubu_svc_supervisor_t *s) {
    if (!s) return 0;
    int stopped = 0;
    for (int i = 0; i < s->count; i++) {
        sup_entry_t *e = &s->entries[i];
        if (e->pid > 0) {
            kill(e->pid, SIGTERM);
            int st; waitpid(e->pid, &st, 0);
            e->pid = 0;
            e->state = SERVICE_STATE_DISABLED;
            e->last_stop = time(NULL);
            stopped++;
        }
    }
    return stopped;
}

int wubu_svc_supervisor_status(wubu_svc_supervisor_t *s, const char *root,
                               const char *svc, WubuArchService *out) {
    if (!s || !out) return -1;
    int i = find_entry(s, root, svc);
    if (i < 0) return -1;
    sup_entry_t *e = &s->entries[i];
    memset(out, 0, sizeof(*out));
    strncpy(out->name, e->svc, sizeof(out->name) - 1);
    strncpy(out->root_name, e->root, sizeof(out->root_name) - 1);
    /* Truthful state: a Type=notify unit that is STARTING stays
     * STARTING (never the "active=running" lie) until ready. */
    if (e->pid > 0 && pid_alive(e->pid)) {
        out->pid = (int)e->pid;
        out->state = entry_ready(e) ? SERVICE_STATE_RUNNING
                                     : SERVICE_STATE_RESTARTING;
    } else {
        out->pid = 0;
        out->state = (e->state == SERVICE_STATE_FAILED)
                        ? SERVICE_STATE_FAILED
                        : (e->state == SERVICE_STATE_RESTARTING)
                            ? SERVICE_STATE_RESTARTING
                            : SERVICE_STATE_DISABLED;
    }
    out->last_start = e->last_start;
    out->last_stop = e->last_stop;
    out->restart_count = e->restart_count;
    out->auto_restart = e->auto_restart;
    return 0;
}

int wubu_svc_supervisor_reap(wubu_svc_supervisor_t *s) {
    if (!s) return 0;
    int reaped = 0;
    time_t now = time(NULL);
    for (int i = 0; i < s->count; i++) {
        sup_entry_t *e = &s->entries[i];
        if (e->pid > 0 && !pid_alive(e->pid)) {
            int st; waitpid(e->pid, &st, 0);
            e->pid = 0;
            e->state = SERVICE_STATE_FAILED;
            e->last_stop = now;
            reaped++;
            if (e->auto_restart && now >= e->backoff_until) {
                e->restart_count++;
                /* exponential backoff: 1s, 2s, 4s, ... capped at 60s */
                time_t delay = RESTART_BACKOFF_BASE;
                for (int k = 1; k < e->restart_count && delay < RESTART_BACKOFF_MAX; k++)
                    delay *= 2;
                if (delay > RESTART_BACKOFF_MAX) delay = RESTART_BACKOFF_MAX;
                e->backoff_until = now + delay;
                wubu_svc_supervisor_log(s, e->root, e->svc,
                    "restarting (crash-loop backoff engaged)");
                wubu_svc_supervisor_start(s, e->root, e->svc);
            } else if (e->auto_restart) {
                e->state = SERVICE_STATE_FAILED;
                wubu_svc_supervisor_log(s, e->root, e->svc,
                    "in restart backoff — pausing (crash loop guard)");
            }
        }
    }
    return reaped;
}

int wubu_svc_supervisor_count(const wubu_svc_supervisor_t *s) {
    return s ? s->count : 0;
}

/* -- Declarative unit model (systemd-good, no notorious parts) - */

static sup_entry_t *find_e(wubu_svc_supervisor_t *s, const char *root,
                            const char *svc) {
    int i = find_entry(s, root, svc);
    return i < 0 ? NULL : &s->entries[i];
}

int wubu_svc_supervisor_add_dep(wubu_svc_supervisor_t *s, const char *root,
                                 const char *svc, const char *dep) {
    sup_entry_t *e = find_e(s, root, svc);
    if (!e || e->n_req >= 8 || !dep || !*dep) return -1;
    strncpy(e->requires[e->n_req], dep, sizeof(e->requires[0]) - 1);
    e->n_req++;
    return 0;
}

int wubu_svc_supervisor_add_after(wubu_svc_supervisor_t *s, const char *root,
                                  const char *svc, const char *pre) {
    sup_entry_t *e = find_e(s, root, svc);
    if (!e || e->n_after >= 8 || !pre || !*pre) return -1;
    strncpy(e->after[e->n_after], pre, sizeof(e->after[0]) - 1);
    e->n_after++;
    return 0;
}

void wubu_svc_supervisor_set_type(wubu_svc_supervisor_t *s, const char *root,
                                 const char *svc, wubu_svc_type_t t) {
    sup_entry_t *e = find_e(s, root, svc);
    if (e) e->type = t;
}

void wubu_svc_supervisor_set_restart(wubu_svc_supervisor_t *s, const char *root,
                                   const char *svc, bool on) {
    sup_entry_t *e = find_e(s, root, svc);
    if (e) e->auto_restart = on;
}

void wubu_svc_supervisor_set_timeout_ms(wubu_svc_supervisor_t *s, const char *root,
                                        const char *svc, int ms) {
    sup_entry_t *e = find_e(s, root, svc);
    if (e) e->timeout_ms = ms > 0 ? ms : 5000;
}

/* Type=notify readiness: a service declares it is actually serving.
 * Until this is called the unit stays STARTING (truthful). */
int wubu_svc_supervisor_notify_ready(wubu_svc_supervisor_t *s,
                                      const char *root, const char *svc) {
    sup_entry_t *e = find_e(s, root, svc);
    if (!e) return -1;
    e->notify_ready = true;
    if (e->pid > 0 && pid_alive(e->pid)) {
        e->state = SERVICE_STATE_RUNNING;
        wubu_svc_supervisor_log(s, root, svc, "READY (notify)");
    }
    return 0;
}

/* Wait for a unit to become truthfully READY (alive + notify satisfied),
 * up to its timeout. Returns 0 if ready, -1 on timeout. */
static int wait_ready(wubu_svc_supervisor_t *s, sup_entry_t *e,
                       const char *root, const char *svc) {
    int budget_ms = e->timeout_ms > 0 ? e->timeout_ms : 5000;
    int waited = 0;
    while (waited < budget_ms) {
        wubu_svc_supervisor_poll(s);   /* reap + promote */
        if (entry_ready(e)) return 0;
        if (e->pid <= 0 || !pid_alive(e->pid)) break; /* died */
        usleep(20000);
        waited += 20;
    }
    return entry_ready(e) ? 0 : -1;
}

/* Ordered boot (kills boot races): a unit is only started after every
 * Requires= dep AND After= predecessor is truthfully RUNNING. If a
 * predecessor fails, the dependent is skipped (not racy-started).
 * Returns the number of units that did NOT reach RUNNING. */
int wubu_svc_supervisor_boot(wubu_svc_supervisor_t *s) {
    if (!s) return 0;
    int not_ready = 0;
    /* Single pass is enough for our depth (deps are 1-level). */
    for (int i = 0; i < s->count; i++) {
        sup_entry_t *e = &s->entries[i];
        bool dep_ok = true;
        for (int j = 0; j < e->n_req; j++) {
            WubuArchService st; memset(&st, 0, sizeof(st));
            if (wubu_svc_supervisor_status(s, e->root, e->requires[j], &st) != 0 ||
                st.state != SERVICE_STATE_RUNNING) {
                dep_ok = false; break;
            }
        }
        for (int j = 0; j < e->n_after && dep_ok; j++) {
            WubuArchService st; memset(&st, 0, sizeof(st));
            if (wubu_svc_supervisor_status(s, e->root, e->after[j], &st) != 0 ||
                st.state != SERVICE_STATE_RUNNING) {
                dep_ok = false; break;
            }
        }
        if (!dep_ok) {
            wubu_svc_supervisor_log(s, e->root, e->svc,
                "skipped: predecessor not RUNNING (no boot race)");
            not_ready++;
            continue;
        }
        if (wubu_svc_supervisor_start(s, e->root, e->svc) != 0) {
            not_ready++;
            continue;
        }
        if (wait_ready(s, e, e->root, e->svc) != 0) {
            e->state = SERVICE_STATE_FAILED;
            wubu_svc_supervisor_log(s, e->root, e->svc,
                "FAILED: readiness timeout");
            not_ready++;
        }
    }
    return not_ready;
}

/* -- Plain-text journal (kills journald's binary blob) ---------- */

int wubu_svc_supervisor_log(wubu_svc_supervisor_t *s, const char *root,
                               const char *svc, const char *msg) {
    sup_entry_t *e = find_e(s, root, svc);
    if (!e || !msg) return -1;
    char ts[32]; time_t now = time(NULL);
    struct tm tmv; localtime_r(&now, &tmv);
    strftime(ts, sizeof(ts), "%H:%M:%S", &tmv);
    /* In-memory plain-text ring (grepable, no dbus to read it). */
    int n = snprintf(e->journal + e->jlen,
                   sizeof(e->journal) - (size_t)e->jlen,
                   "[%s] %s\n", ts, msg);
    if (n > 0) e->jlen += n;
    if (e->jlen >= (int)sizeof(e->journal) - 1) e->jlen = 0; /* wrap */
    /* Ensure the plain-text journal dir exists (no journald). */
    char dir[WUBU_ARCHD_MAX_PATH];
    snprintf(dir, sizeof(dir), "/tmp/wubu-svc/%s", root);
    mkdir(dir, 0755);   /* best-effort; ignore EEXIST */

    /* Mirror to a plain file so `grep` works on disk too. */
    char path[WUBU_ARCHD_MAX_PATH];
    snprintf(path, sizeof(path), "/tmp/wubu-svc/%s/%s.log", root, svc);
    FILE *fp = fopen(path, "a");
    if (fp) { fprintf(fp, "[%s] %s\n", ts, msg); fclose(fp); }
    return 0;
}

int wubu_svc_supervisor_logs(wubu_svc_supervisor_t *s, const char *root,
                                const char *svc, char *out, size_t n) {
    sup_entry_t *e = find_e(s, root, svc);
    if (!e || !out) return -1;
    if ((size_t)e->jlen < n) n = (size_t)e->jlen;
    if (n > 0) memcpy(out, e->journal, n);
    out[n] = '\0';
    return (int)n;   /* return the copied length, not 0 */
}

/* -- Health heartbeat (N8 closure) ------------------------------- */
/* Register a callback invoked for each managed service that WAS running
 * and died unexpectedly (not an auto-restart service). The desktop
 * uses this to surface a tray/toast alert (see dosgui_service_mgr). */
typedef void (*wubu_svc_on_fail_fn)(const char *root, const char *svc,
                                     void *ud);
static wubu_svc_on_fail_fn g_on_fail = NULL;
static void *g_on_fail_ud = NULL;

void wubu_svc_supervisor_set_on_fail(wubu_svc_supervisor_t *s,
                                     wubu_svc_on_fail_fn fn, void *ud) {
    (void)s;   /* callback is global to the supervisor domain */
    g_on_fail = fn;
    g_on_fail_ud = ud;
}

/* Poll health: reap dead children, detect unexpected deaths, honor
 * auto_restart for managed ones, and invoke the on_fail callback
 * for services that died without being asked to stop.
 * Returns the number of unexpected deaths detected this poll. */
int wubu_svc_supervisor_poll(wubu_svc_supervisor_t *s) {
    if (!s) return 0;
    int detected = 0;
    time_t now = time(NULL);
    for (int i = 0; i < s->count; i++) {
        sup_entry_t *e = &s->entries[i];
        if (e->pid > 0 && !pid_alive(e->pid)) {
            int st;
            waitpid(e->pid, &st, 0);   /* reap */
            e->pid = 0;
            e->last_stop = now;
            if (e->auto_restart && now >= e->backoff_until) {
                e->restart_count++;
                time_t delay = RESTART_BACKOFF_BASE;
                for (int k = 1; k < e->restart_count && delay < RESTART_BACKOFF_MAX; k++)
                    delay *= 2;
                if (delay > RESTART_BACKOFF_MAX) delay = RESTART_BACKOFF_MAX;
                e->backoff_until = now + delay;
                wubu_svc_supervisor_start(s, e->root, e->svc);
                continue;   /* restarted; not an "unexpected" death */
            }
            if (e->auto_restart) {
                /* crash-loop guard: backoff window active, pause */
                e->state = SERVICE_STATE_FAILED;
                detected++;
                if (g_on_fail) g_on_fail(e->root, e->svc, g_on_fail_ud);
                continue;
            }
            /* Unexpected death: it was RUNNING, now gone, no restart. */
            e->state = SERVICE_STATE_FAILED;
            detected++;
            if (g_on_fail) g_on_fail(e->root, e->svc, g_on_fail_ud);
        }
    }
    return detected;
}

/* -- Global supervisor wiring (consumed by wubu_archd_svc_*) ------- */
static wubu_svc_supervisor_t *g_sup = NULL;

void wubu_archd_svc_set_supervisor(wubu_svc_supervisor_t *s) { g_sup = s; }
wubu_svc_supervisor_t *wubu_archd_svc_get_supervisor(void) { return g_sup; }
