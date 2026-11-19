/*
 * wubu_host_exec.c  --  WuBuOS Host Container Execution (Linux)
 *
 * Cell 203: Fork+exec for .wubu containers via host delegation.
 *
 * Containers are HOST PROCESSES. No syscall emulation.
 * Arch base → SteamOS compat → rip through Linux drivers.
 * GPU passthrough: /dev/dri, /dev/nvidia → container.
 * 9P namespace: per-container Styx socket mount.
 */
#define WUBU_HOSTED  /* chroot() is XSI/POSIX, hidden under -std=c11 without this */
#include "../egojar.h"
#include "../egojar_net.h"
#include <sys/socket.h>
#include "wubu_host_exec.h"
#include "wubu_secmon.h"
#include "wubu_std.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "styxfs.h"
#include "wubu_ct_isolate.h"

/* L9 forward declarations (definitions live below the Styx server). */
static void ct_detach(WubuCt *ct);
__attribute__((weak)) int wubu_ns_guests_sync(void);
__attribute__((weak)) void wubu_ns_guests_forget(WubuCt *ct);

/* -- State/Runtime Names ------------------------------------------- */

const char *wubu_ct_state_name(CtState state) {
    switch (state) {
        case CT_STOPPED:  return "stopped";
        case CT_STARTING: return "starting";
        case CT_RUNNING:  return "running";
        case CT_EXITED:   return "exited";
        case CT_FAILED:   return "failed";
        default:          return "unknown";
    }
}

const char *wubu_ct_runtime_name(CtRuntime runtime) {
    switch (runtime) {
        case CT_NONE:    return "none";
        case CT_STEAMOS: return "SteamOS";
        case CT_NATIVE:  return "native";
        case CT_PROTON:  return "Proton";
        case CT_HOLYC:   return "HolyC";
        case CT_MACHO:   return "Mach-O";
        default:          return "unknown";
    }
}

/* -- Container Create/Destroy -------------------------------------- */

WubuCt *wubu_ct_create(const char *name, const char *root, CtRuntime runtime) {
    if (!name || !root) return NULL;
    
    WubuCt *ct = (WubuCt*)calloc(1, sizeof(WubuCt));
    if (!ct) return NULL;
    
    strncpy(ct->name, name, WUBU_CT_NAME_MAX - 1);
    strncpy(ct->root, root, WUBU_CT_ROOT_MAX - 1);
    ct->runtime = runtime;
    ct->state = CT_STOPPED;
    ct->pid = 0;
    ct->exit_code = -1;
    ct->uid = 0;
    ct->gid = 0;
    ct->mem_limit_mb = 0;
    ct->cpu_limit = 0;
    ct->gpu_passthrough = false;
    ct->net_enabled = true;
    ct->styx_fd = -1;
    ct->styx_pid = 0;
    ct->n_binds = 0;
    
    return ct;
}

void wubu_ct_destroy(WubuCt *ct) {
    if (!ct) return;
    /* Drop any /n/guests ownership pointer FIRST, so a sync triggered by the
     * kill+wait below can never reap (free) the ct we are destroying. */
    wubu_ns_guests_forget(ct);
    if (ct->state == CT_RUNNING) {
        wubu_ct_kill(ct, SIGKILL);
        wubu_ct_wait(ct);
    }
    /* Idempotent detach: stops/reaps the 9P server if still up, unregisters.
     * No sync here -- ct is freed below and must not be touched afterwards. */
    ct_detach(ct);
    /* Free argv/envp strings (if dynamically allocated) */
    for (int i = 0; i < WUBU_CT_MAX_ARGS; i++) {
        if (ct->argv[i]) { free(ct->argv[i]); ct->argv[i] = NULL; }
    }
    for (int i = 0; i < WUBU_CT_MAX_ENV; i++) {
        if (ct->envp[i]) { free(ct->envp[i]); ct->envp[i] = NULL; }
    }
    free(ct);
}

/* -- Configuration ------------------------------------------------ */

int wubu_ct_set_cmd(WubuCt *ct, int argc, char **argv) {
    if (!ct || !argv) return -1;
    int n = argc < WUBU_CT_MAX_ARGS ? argc : WUBU_CT_MAX_ARGS;
    for (int i = 0; i < n; i++) {
        if (ct->argv[i]) free(ct->argv[i]);
        ct->argv[i] = wubu_strdup(argv[i]);
    }
    return 0;
}

int wubu_ct_add_env(WubuCt *ct, const char *env) {
    if (!ct || !env) return -1;
    /* Find empty slot */
    for (int i = 0; i < WUBU_CT_MAX_ENV; i++) {
        if (!ct->envp[i]) {
            ct->envp[i] = wubu_strdup(env);
            return 0;
        }
    }
    return -1;  /* No room */
}

int wubu_ct_add_bind(WubuCt *ct, const char *host, const char *guest, bool ro) {
    if (!ct || !host || !guest) return -1;
    if (ct->n_binds >= WUBU_CT_MAX_BINDS) return -1;
    CtBind *b = &ct->binds[ct->n_binds++];
    strncpy(b->host, host, sizeof(b->host) - 1);
    strncpy(b->guest, guest, sizeof(b->guest) - 1);
    b->readonly = ro;
    return 0;
}

void wubu_ct_set_gpu(WubuCt *ct, bool enable) {
    if (ct) ct->gpu_passthrough = enable;
}

void wubu_ct_set_limits(WubuCt *ct, uint64_t mem_mb, int cpu_count) {
    if (!ct) return;
    ct->mem_limit_mb = mem_mb;
    ct->cpu_limit = cpu_count;
}

/* -- GPU Passthrough Binds ---------------------------------------- */

static void add_gpu_binds(WubuCt *ct) {
    /* DRM/KMS  --  works for AMD and Intel */
    wubu_ct_add_bind(ct, "/dev/dri", "/dev/dri", false);
    /* NVIDIA */
    wubu_ct_add_bind(ct, "/dev/nvidia0", "/dev/nvidia0", false);
    wubu_ct_add_bind(ct, "/dev/nvidiactl", "/dev/nvidiactl", false);
    wubu_ct_add_bind(ct, "/dev/nvidia-uvm", "/dev/nvidia-uvm", false);
    wubu_ct_add_bind(ct, "/dev/nvidia-modeset", "/dev/nvidia-modeset", false);
    /* Shared memory for GPU */
    wubu_ct_add_bind(ct, "/dev/shm", "/dev/shm", false);
}

/* -- Styx Namespace Setup ----------------------------------------- */

static int setup_styx_socket(WubuCt *ct) {
    snprintf(ct->styx_path, sizeof(ct->styx_path),
             "/tmp/wubu-ct-%s-styx.sock", ct->name);
    
    ct->styx_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ct->styx_fd < 0) return -1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ct->styx_path, sizeof(addr.sun_path) - 1);
    
    unlink(ct->styx_path);
    if (bind(ct->styx_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(ct->styx_fd);
        ct->styx_fd = -1;
        return -1;
    }
    listen(ct->styx_fd, 5);
    /* CLOEXEC: the container child's execv() must NOT inherit the listening
     * socket. The 9P server child (forked below, never execs) keeps its copy. */
    fcntl(ct->styx_fd, F_SETFD, FD_CLOEXEC);
    return 0;
}

/* -- Per-Container Styx 9P Server (Cell 414, L9: switched ON) --------
 *
 * Runs PARENT-SIDE in its own forked process (the concurrency model of the
 * hosted runtime is processes, not threads -- see hosted.c's buddy spawn).
 * wubu_ct_start() forks it right after the listening socket exists; the
 * container child never sees it (styx_fd is CLOEXEC, so execv drops it).
 * wubu_ct_wait()/wubu_ct_state()/wubu_ct_destroy() stop and REAP it when the
 * container exits -- no zombies, no orphaned servers.
 *
 * Wire framing (9P2000): every message -- T and R -- begins with a 4-byte
 * LITTLE-endian size that COUNTS ITSELF. styx_serve() expects that prefix
 * INSIDE inbuf, and its responses already carry it (styx_write_header).
 * The original loop stripped the prefix on the way in and prepended a second
 * one on the way out, so no real client could ever talk to it. Fixed here:
 * read the whole frame into inbuf, hand it to styxfs_serve verbatim, write
 * the response verbatim. */

/* Read exactly n bytes; 0 on success, -1 on EOF/error/EINTR-exhausted. */
static int read_exact(int fd, uint8_t *buf, uint32_t n) {
    uint32_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, buf + got, n - got);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) return -1;
        got += (uint32_t)r;
    }
    return 0;
}

/* Write exactly n bytes; 0 on success, -1 on error. */
static int write_exact(int fd, const uint8_t *buf, uint32_t n) {
    uint32_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, buf + done, n - done);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        done += (uint32_t)w;
    }
    return 0;
}

static void run_container_styx_server(WubuCt *ct) {
    if (!ct || ct->styx_fd < 0) return;

    /* Initialize StyxFS server */
    styxfs_server_t srv;
    styxfs_init(&srv);

    /* Mount the container's root filesystem at / */
    styxfs_mount(&srv, "/", ct->root, 1);

    /* Set up signal handling for clean shutdown: the parent stops us with
     * SIGTERM (default action terminates even while blocked in accept). */
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGPIPE, SIG_IGN);

    /* One frame at a time; buffers sized for the protocol maximum so a
     * hostile size field can never overflow them (validated below). */
    static uint8_t inbuf[STYX_MAX_MSG], outbuf[STYX_MAX_MSG];

    /* Accept and serve connections */
    while (1) {
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(ct->styx_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* Serve this connection */
        while (1) {
            /* Whole frame: 4-byte size prefix + body, prefix included. */
            if (read_exact(client_fd, inbuf, 4) != 0) break;
            uint32_t size = styx_get32(inbuf);
            if (size < 7 || size > STYX_MAX_MSG) break;
            if (read_exact(client_fd, inbuf + 4, size - 4) != 0) break;

            uint32_t outlen = sizeof(outbuf);
            if (styxfs_serve(&srv, inbuf, size, outbuf, &outlen) != 0) break;
            /* outlen already includes the size prefix -- write verbatim. */
            if (write_exact(client_fd, outbuf, outlen) != 0) break;
        }

        close(client_fd);
    }

    /* Cleanup */
    styxfs_mount_t *m = srv.mounts;
    while (m) {
        styxfs_mount_t *next = m->next;
        free(m);
        m = next;
    }
}

/* -- Running-Container Registry (single owner of live ct state) ----- */

#define WUBU_CT_REGISTRY_MAX 64
static WubuCt *g_ct_registry[WUBU_CT_REGISTRY_MAX];
static int g_ct_registry_n = 0;

int wubu_ct_registry_count(void) { return g_ct_registry_n; }

WubuCt *wubu_ct_registry_get(int idx) {
    if (idx < 0 || idx >= g_ct_registry_n) return NULL;
    return g_ct_registry[idx];
}

WubuCt *wubu_ct_registry_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_ct_registry_n; i++)
        if (strcmp(g_ct_registry[i]->name, name) == 0) return g_ct_registry[i];
    return NULL;
}

static void ct_register(WubuCt *ct) {
    if (!ct || wubu_ct_registry_find(ct->name)) return;
    if (g_ct_registry_n >= WUBU_CT_REGISTRY_MAX) return;
    g_ct_registry[g_ct_registry_n++] = ct;
}

static void ct_unregister(WubuCt *ct) {
    for (int i = 0; i < g_ct_registry_n; i++) {
        if (g_ct_registry[i] == ct) {
            g_ct_registry[i] = g_ct_registry[--g_ct_registry_n];
            g_ct_registry[g_ct_registry_n] = NULL;
            return;
        }
    }
}

/* /n/guests publisher + ownership hook. Weak no-ops here so every link line
 * that carries wubu_host_exec.o works without the namespace module; the real
 * wubu_ns_guests.o overrides them at link time (product + guests gates). */
__attribute__((weak)) int wubu_ns_guests_sync(void) { return -1; }
__attribute__((weak)) void wubu_ns_guests_forget(WubuCt *ct) { (void)ct; }

/* Stop and REAP the parent-side 9P server, close the listening socket.
 * Idempotent: safe to call from wait, state-poll and destroy. */
static void ct_stop_styx(WubuCt *ct) {
    if (ct->styx_pid > 0) {
        pid_t spid = (pid_t)ct->styx_pid;
        kill(spid, SIGTERM);
        /* Bounded reap: SIGTERM's default action kills even a server blocked
         * in accept(); escalate to SIGKILL only if it somehow lingers. */
        int tries = 0;
        pid_t r;
        while ((r = waitpid(spid, NULL, WNOHANG)) == 0 && tries++ < 200)
            usleep(10000);                       /* up to ~2 s */
        if (r == 0) {
            kill(spid, SIGKILL);
            waitpid(spid, NULL, 0);
        }
        ct->styx_pid = 0;                        /* reaped -- no zombie */
    }
    if (ct->styx_fd >= 0) {
        close(ct->styx_fd);
        ct->styx_fd = -1;
        unlink(ct->styx_path);
    }
}

/* Detach a container from the live set: stop its 9P server, unregister.
 * No namespace sync here -- callers that may free ct afterwards must not
 * trigger a sync that reaps OTHER containers re-entrantly. */
static void ct_detach(WubuCt *ct) {
    ct_stop_styx(ct);
    ct_unregister(ct);
}

/* Full retirement on container exit: detach + refresh /n/guests. The sync
 * may destroy ns-launched containers (the guests module owns those), so
 * callers MUST capture return values into locals BEFORE calling this. */
static void ct_retire(WubuCt *ct) {
    ct_detach(ct);
    wubu_ns_guests_sync();
}

/* -- Container Start (fork + chroot + exec) ----------------------- */

int wubu_ct_start(WubuCt *ct) {
    if (!ct || ct->state == CT_RUNNING) return -1;
    if (!ct->argv[0]) return -1;
    /* One running guest per name: the Styx socket path and /n/guests entry
     * are both keyed by name, so a duplicate would clobber the live one. */
    if (wubu_ct_registry_find(ct->name)) return -1;

    ct->state = CT_STARTING;

    /* Setup per-container Styx namespace socket */
    if (setup_styx_socket(ct) != 0) {
        /* Non-fatal  --  container can run without 9P namespace */
    }

    /* L9: run the per-container 9P server PARENT-SIDE, in its own process.
     * Forked here (before the container fork) so it inherits the listening
     * socket but none of the child's isolation/chroot/exec. Its lifecycle is
     * tied to the container: ct_retire() stops and reaps it on exit. */
    if (ct->styx_fd >= 0) {
        pid_t spid = fork();
        if (spid == 0) {
            /* Server child: die if the parent dies first (no orphans), then
             * serve until SIGTERM from ct_stop_styx(). Never execs. */
            prctl(PR_SET_PDEATHSIG, SIGTERM);
            if (getppid() == 1) _exit(0);   /* parent already gone */
            run_container_styx_server(ct);
            _exit(0);
        }
        if (spid > 0) {
            ct->styx_pid = (int)spid;
        } else {
            /* Server fork failed -- non-fatal, run without 9P namespace. */
            close(ct->styx_fd);
            ct->styx_fd = -1;
            unlink(ct->styx_path);
        }
    }
    
    /* Add GPU bind mounts if requested */
    if (ct->gpu_passthrough) {
        add_gpu_binds(ct);
    }
    
    /* Add X11 socket for GUI apps */
    wubu_ct_add_bind(ct, "/tmp/.X11-unix", "/tmp/.X11-unix", false);
    
    /* Cell 420: Setup isolation (cgroups v2 + seccomp) before fork */
    wubu_ct_setup_isolation(ct);

    /* Capture the parent's current working directory BEFORE forking so the
     * child can re-enter it. The era-apps launcher (and any caller that
     * passes a relative output path to its payload) expects the container's
     * CWD to match the launching process's CWD. When chroot to a real root
     * fs succeeds the child stays in that root; otherwise it runs in the
     * host namespace and this chdir lands it in the real host CWD so
     * CWD-relative file writes are visible to the parent. */
    char parent_cwd[1024];
    if (!getcwd(parent_cwd, sizeof(parent_cwd))) parent_cwd[0] = '\0';

    /* Fork */
    pid_t pid = fork();
    if (pid < 0) {
        ct->state = CT_FAILED;
        return -1;
    }

    if (pid == 0) {
        /* -- CHILD: container process -------------------------- */

        /* Apply container env overrides (wubu_ct_add_env) into the child's
         * environment BEFORE anything that reads them. The previous code used
         * execv(), which ignored ct->envp entirely -- so isolation profile /
         * namespace / repo-root settings never reached the payload and the
         * default full-isolation seccomp killed things like Wine (SIGSYS).
         * This MUST run before wubu_ct_child_isolation(), which reads
         * WUBU_SECCOMP_PROFILE / WUBU_NS_FLAGS from the environment. */
        for (int i = 0; i < WUBU_CT_MAX_ENV && ct->envp[i]; i++) {
            char *eq = strchr(ct->envp[i], '=');
            if (eq) {
                *eq = '\0';
                setenv(ct->envp[i], eq + 1, 1);
                *eq = '=';
            }
        }

        /* Cell 420: Apply child isolation (cgroup attach + seccomp filter) */
        wubu_ct_child_isolation();
        
        /* chroot into container root (Arch base). A root of "/" or "" means
         * "run in the host namespace" -- chroot("/") is a no-op that would
         * still force CWD to "/", so we skip it and instead re-enter the
         * launching process's CWD so CWD-relative payload output is visible
         * to the parent. */
        bool did_chroot = false;
        if (ct->root[0] && strcmp(ct->root, "/") != 0) {
            if (chroot(ct->root) == 0) {
                did_chroot = true;
                chdir("/");
            } else {
                fprintf(stderr, "wubu_ct: chroot(%s) failed: %s\n",
                        ct->root, strerror(errno));
            }
        }
        if (!did_chroot && parent_cwd[0]) {
            chdir(parent_cwd);
        }
        
        /* Set uid/gid */
        if (ct->gid > 0) setgid((gid_t)ct->gid);
        if (ct->uid > 0) setuid((uid_t)ct->uid);

        /* Cell 415: Apply resource limits via setrlimit before exec */
        if (ct->cpu_limit > 0) {
            struct rlimit cpu_rl = { .rlim_cur = ct->cpu_limit, .rlim_max = ct->cpu_limit };
            setrlimit(RLIMIT_CPU, &cpu_rl);
        }
        if (ct->mem_limit_mb > 0) {
            struct rlimit as_rl = { .rlim_cur = ct->mem_limit_mb * 1024 * 1024, .rlim_max = ct->mem_limit_mb * 1024 * 1024 };
            setrlimit(RLIMIT_AS, &as_rl);
        }

        /* Mount bind mounts (GPU passthrough, X11 socket, etc.) */
        for (int i = 0; i < ct->n_binds; i++) {
            /* Create mount point inside chroot */
            struct stat st;
            if (stat(ct->binds[i].guest, &st) != 0) {
                /* Try mkdir -p for the mount point */
                /* Simple version: just mkdir the last component */
                mkdir(ct->binds[i].guest, 0755);
            }
            unsigned long mflags = MS_BIND | MS_REC;
            if (ct->binds[i].readonly) mflags |= MS_RDONLY;
            if (mount(ct->binds[i].host, ct->binds[i].guest, NULL, mflags, NULL) != 0) {
                /* Non-fatal  --  bind mount may fail if host path doesn't exist */
                /* (e.g., /dev/nvidia0 on a system without NVIDIA) */
            }
        }
        
        /* Cell 414 / L9: the per-container Styx 9P server runs PARENT-SIDE
         * (forked in wubu_ct_start before this child). The child is about to
         * execv() and must not carry a server grandchild -- styx_fd is
         * CLOEXEC so the exec drops the listening socket here. */

        /* Execute */
        execv(ct->argv[0], ct->argv);
        
        /* If execv fails */
        _exit(127);
    }
    
    /* -- PARENT ------------------------------------------------ */
    /* -- PARENT: attach the AGI syscall camera to the game process ----
     * This is the interception layer: every syscall the game makes is
     * captured and streamed to /kv/agent/sys_<pid>/<seq> so the Brain
     * (wubuwizard) can observe the full behavior stream over 9P.
     *
     * The child has been fork()ed but not yet execv'd. We PTRACE_ATTACH
     * here: the child will stop at the next syscall boundary (the exec),
     * giving us a race-free observation point. The camera runs on-demand
     * — callers poll wubu_secmon_poll() or block on wubu_secmon_wait(). */
    if (getenv("WUBU_SECCOMP_PROFILE") &&
        strcmp(getenv("WUBU_SECCOMP_PROFILE"), "none") != 0 &&
        !ct->secmon) {
        ct->secmon = wubu_secmon_create(0);
        if (ct->secmon)
            wubu_secmon_attach((wubu_secmon_t *)ct->secmon, pid);
    }

    ct->pid = pid;
    ct->state = CT_RUNNING;

    /* L9: register in the live-container registry and publish /n/guests. */
    ct_register(ct);
    wubu_ns_guests_sync();

    return 0;
}

/* -- Container Wait ----------------------------------------------- */

int wubu_ct_wait(WubuCt *ct) {
    if (!ct || ct->pid <= 0) return -1;

    int status = 0;
    pid_t ret = waitpid(ct->pid, &status, 0);
    /* Capture into a local BEFORE retiring: ct_retire() -> guests sync may
     * destroy (free) an ns-launched ct, so ct must not be read afterwards. */
    int code = ct->exit_code;

    if (ret == ct->pid) {
        if (WIFEXITED(status)) {
            ct->exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            ct->exit_code = -WTERMSIG(status);
        } else {
            ct->exit_code = -1;
        }
        ct->state = CT_EXITED;
        ct->pid = 0;
        code = ct->exit_code;
        /* L9: container exited -- stop/reap its 9P server, unregister,
         * refresh /n/guests. */
        ct_retire(ct);
    }

    return code;
}

/* -- Container Kill ----------------------------------------------- */

int wubu_ct_kill(WubuCt *ct, int sig) {
    if (!ct || ct->pid <= 0) return -1;
    if (kill(ct->pid, sig) != 0) return -1;
    return 0;
}

/* -- State Query -------------------------------------------------- */

CtState wubu_ct_state(WubuCt *ct) {
    if (!ct) return CT_STOPPED;

    /* Local capture: retirement below may free an ns-launched ct. */
    CtState s = ct->state;

    /* Check if running process has exited */
    if (ct->state == CT_RUNNING && ct->pid > 0) {
        int status = 0;
        pid_t ret = waitpid(ct->pid, &status, WNOHANG);
        if (ret == ct->pid) {
            if (WIFEXITED(status))
                ct->exit_code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                ct->exit_code = -WTERMSIG(status);
            ct->state = CT_EXITED;
            ct->pid = 0;
            s = CT_EXITED;
            /* L9: exited -- stop/reap the 9P server, unregister, sync /n. */
            ct_retire(ct);
        }
    }

    return s;
}

/* -- Preset: SteamOS Container ------------------------------------ */

WubuCt *wubu_ct_steamos(const char *name, const char *root) {
    WubuCt *ct = wubu_ct_create(name, root, CT_STEAMOS);
    if (!ct) return NULL;
    
    /* SteamOS: GPU passthrough always on */
    ct->gpu_passthrough = true;
    ct->net_enabled = true;
    
    /* Steam Runtime environment */
    wubu_ct_add_env(ct, "STEAM_RUNTIME=1");
    wubu_ct_add_env(ct, "PROTON_LOG=1");
    
    return ct;
}

/* -- Preset: Native Linux Container ------------------------------- */

WubuCt *wubu_ct_native(const char *name, const char *root) {
    WubuCt *ct = wubu_ct_create(name, root, CT_NATIVE);
    if (!ct) return NULL;
    
    ct->net_enabled = true;
    
    return ct;
}

/* -- Preset: Darwin / Mach-O Container (agnostic HW operator) ---- */

/*
 * Mach-O apps are Darwin binaries. WuBuOS runs them as a first-class
 * WubuCt process: fork + chroot into an Arch root that has Darling
 * installed, then exec `darling <macho>` as the container's argv[0].
 * VSL serves the GPU capability underneath (Vulkan via lavapipe/D3D12,
 * Metal shaders transposed to SPIR-V by the metal2vulkan shim), so the
 * same container machinery that drives ELF/PE also drives Mach-O -- we are
 * an agnostic operator across all execution container types.
 */
WubuCt *wubu_ct_macho(const char *name, const char *root) {
    WubuCt *ct = wubu_ct_create(name, root, CT_MACHO);
    if (!ct) return NULL;

    /* Darling needs the network + GPU passthrough to serve Vulkan/Metal. */
    ct->net_enabled     = true;
    ct->gpu_passthrough = true;

    /* Point Mach-O at the Metal->Vulkan transposition shim if present.
     * Prefer the real GPU-PV ICD (/dev/dxg -> gfxstream) and always keep
     * llvmpipe as a guaranteed fallback so Vulkan calls are properly
     * processed even when the host GPU is unavailable. VK_ICD_FILENAMES is
     * normally supplied machine-wide by /etc/profile.d/wubu-gpu-shim.sh; we
     * only set it here if it was absent. */
    wubu_ct_add_env(ct, "WUBU_METAL2VULKAN=1");
    if (!getenv("VK_ICD_FILENAMES")) {
        wubu_ct_add_env(ct, "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/gfxstream_vk_icd.json:/usr/share/vulkan/icd.d/lvp_icd.json");
    }

    return ct;
}
