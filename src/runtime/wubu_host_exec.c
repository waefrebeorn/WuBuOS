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
#define _GNU_SOURCE
#include "wubu_host_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include "styxfs.h"
#include "wubu_ct_isolate.h"

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
        default:         return "unknown";
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
    ct->n_binds = 0;
    
    return ct;
}

void wubu_ct_destroy(WubuCt *ct) {
    if (!ct) return;
    if (ct->state == CT_RUNNING) {
        wubu_ct_kill(ct, SIGKILL);
        wubu_ct_wait(ct);
    }
    /* Close Styx socket */
    if (ct->styx_fd >= 0) {
        close(ct->styx_fd);
        unlink(ct->styx_path);
    }
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
        ct->argv[i] = strdup(argv[i]);
    }
    return 0;
}

int wubu_ct_add_env(WubuCt *ct, const char *env) {
    if (!ct || !env) return -1;
    /* Find empty slot */
    for (int i = 0; i < WUBU_CT_MAX_ENV; i++) {
        if (!ct->envp[i]) {
            ct->envp[i] = strdup(env);
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
    return 0;
}

/* -- Per-Container Styx 9P Server (Cell 414) ----------------------- */

static void run_container_styx_server(WubuCt *ct) {
    if (!ct || ct->styx_fd < 0) return;
    
    /* Initialize StyxFS server */
    styxfs_server_t srv;
    styxfs_init(&srv);
    
    /* Mount the container's root filesystem at / */
    styxfs_mount(&srv, "/", ct->root, 1);
    
    /* Set up signal handling for clean shutdown */
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    
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
        uint8_t inbuf[8192], outbuf[8192];
        uint32_t inlen, outlen;
        
        while (1) {
            /* Read message length (4 bytes) */
            ssize_t n = read(client_fd, &inlen, 4);
            if (n <= 0) break;
            
            /* Read message body */
            n = read(client_fd, inbuf, inlen);
            if (n <= 0) break;
            
            /* Process via StyxFS */
            outlen = sizeof(outbuf);
            if (styxfs_serve(&srv, inbuf, inlen, outbuf, &outlen) == 0) {
                write(client_fd, &outlen, 4);
                write(client_fd, outbuf, outlen);
            } else {
                break;
            }
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

/* -- Container Start (fork + chroot + exec) ----------------------- */

int wubu_ct_start(WubuCt *ct) {
    if (!ct || ct->state == CT_RUNNING) return -1;
    if (!ct->argv[0]) return -1;
    
    ct->state = CT_STARTING;
    
    /* Setup per-container Styx namespace socket */
    if (setup_styx_socket(ct) != 0) {
        /* Non-fatal  --  container can run without 9P namespace */
    }
    
    /* Add GPU bind mounts if requested */
    if (ct->gpu_passthrough) {
        add_gpu_binds(ct);
    }
    
    /* Add X11 socket for GUI apps */
    wubu_ct_add_bind(ct, "/tmp/.X11-unix", "/tmp/.X11-unix", false);
    
    /* Cell 420: Setup isolation (cgroups v2 + seccomp) before fork */
    wubu_ct_setup_isolation(ct);

    /* Fork */
    pid_t pid = fork();
    if (pid < 0) {
        ct->state = CT_FAILED;
        return -1;
    }

    if (pid == 0) {
        /* -- CHILD: container process -------------------------- */

        /* Cell 420: Apply child isolation (cgroup attach + seccomp filter) */
        wubu_ct_child_isolation();
        
        /* chroot into container root (Arch base) */
        if (ct->root[0] && chroot(ct->root) != 0) {
            /* If chroot fails (no root fs), fall through  -- 
             * container runs in host namespace as fallback */
            fprintf(stderr, "wubu_ct: chroot(%s) failed: %s\n",
                    ct->root, strerror(errno));
        } else if (ct->root[0]) {
            chdir("/");
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
        
        /* Cell 414: Start per-container Styx 9P server
         *
         * NOTE: For production, the Styx server should be managed by the
         * parent (container manager), not forked from the child. The child
         * is about to execv() and can't manage grandchild processes.
         * For now we skip server fork — it will be re-integrated when
         * the container manager is refactored as a proper supervisor.
         */
        (void)run_container_styx_server;

        /* Execute */
        execv(ct->argv[0], ct->argv);
        
        /* If execv fails */
        _exit(127);
    }
    
    /* -- PARENT ------------------------------------------------ */
    ct->pid = pid;
    ct->state = CT_RUNNING;
    
    return 0;
}

/* -- Container Wait ----------------------------------------------- */

int wubu_ct_wait(WubuCt *ct) {
    if (!ct || ct->pid <= 0) return -1;
    
    int status = 0;
    pid_t ret = waitpid(ct->pid, &status, 0);
    
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
    }
    
    return ct->exit_code;
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
        }
    }
    
    return ct->state;
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
