/*
 * wubu_host_exec.h  --  WuBuOS Host Container Execution (Linux)
 *
 * Cell 203: Fork+exec for .wubu containers via host delegation.
 *
 * Architecture:
 *   - Containers are HOST PROCESSES (fork + chroot + exec)
 *   - NOT syscall emulation  --  we delegate to the host Linux kernel
 *   - Arch Linux base for SteamOS compat (DRM/KMS/NVIDIA/AMD drivers)
 *   - Per-container 9P namespace via Styx socket mount
 *   - GPU passthrough: /dev/dri, /dev/nvidia flow into container
 *   - SteamOS container: Arch root + Steam Runtime + Proton + games
 *
 * This replaces the old VSL "Linux compat layer" model.
 * VSL was 46 function pointers that couldn't fork/exec.
 * This is real process creation.
 */
#ifndef WUBU_HOST_EXEC_H
#define WUBU_HOST_EXEC_H

#include <stdint.h>
#include <stdbool.h>

/* -- Container Configuration -------------------------------------- */

#define WUBU_CT_MAX_ARGS    64
#define WUBU_CT_MAX_ENV     64
#define WUBU_CT_MAX_BINDS   32
#define WUBU_CT_MAX_GUEST   256
#define WUBU_CT_NAME_MAX    64
#define WUBU_CT_ROOT_MAX    512

typedef enum {
    CT_STOPPED   = 0,
    CT_STARTING  = 1,
    CT_RUNNING   = 2,
    CT_EXITED    = 3,
    CT_FAILED    = 4,
} CtState;

typedef enum {
    CT_NONE      = 0,     /* No special runtime */
    CT_STEAMOS   = 1,     /* Steam Runtime + Proton */
    CT_NATIVE    = 2,     /* Direct host exec */
    CT_PROTON    = 3,     /* Wine/Proton for Windows apps */
    CT_HOLYC     = 4,     /* HolyC JIT session */
} CtRuntime;

/* 9P/Styx bind mount: host path → guest path */
typedef struct {
    char host[256];
    char guest[256];
    bool readonly;
} CtBind;

typedef struct {
    char     name[WUBU_CT_NAME_MAX];
    char     root[WUBU_CT_ROOT_MAX];     /* chroot root (Arch rootfs) */
    CtRuntime runtime;
    CtState  state;
    
    /* Process */
    int      pid;           /* Host PID (0 if not running) */
    int      exit_code;     /* Exit code if CT_EXITED */
    
    /* Identity */
    int      uid, gid;      /* Container user/group */
    
    /* Resources */
    uint64_t mem_limit_mb;  /* Memory limit in MB (0 = unlimited) */
    int      cpu_limit;     /* CPU count limit (0 = unlimited) */
    uint64_t io_read_bps;   /* I/O read bandwidth limit (bytes/sec, 0 = unlimited) */
    uint64_t io_write_bps;  /* I/O write bandwidth limit (bytes/sec, 0 = unlimited) */
    uint32_t io_weight;     /* I/O weight (1-10000, 0 = default 100) */
    bool     gpu_passthrough;   /* /dev/dri + /dev/nvidia */
    bool     net_enabled;       /* Network access */
    
    /* 9P namespace binds */
    CtBind   binds[WUBU_CT_MAX_BINDS];
    int      n_binds;
    
    /* Styx socket */
    int      styx_fd;       /* Per-container Styx namespace fd */
    char     styx_path[256];/* Unix socket path */
    
    /* Command */
    char    *argv[WUBU_CT_MAX_ARGS];
    char    *envp[WUBU_CT_MAX_ENV];
} WubuCt;

/* -- Container Lifecycle ------------------------------------------ */

/*
 * Create a container configuration.
 * name: human-readable container name
 * root: chroot root path (e.g., "/var/wubu/roots/arch-base")
 * runtime: CT_NATIVE, CT_STEAMOS, CT_PROTON, CT_HOLYC
 */
WubuCt *wubu_ct_create(const char *name, const char *root, CtRuntime runtime);

/*
 * Destroy a container config (must be stopped).
 */
void wubu_ct_destroy(WubuCt *ct);

/*
 * Start a container: fork + chroot + exec.
 * The child process runs the container's argv[0] inside the chroot.
 * Returns 0 on success, -1 on failure.
 */
int wubu_ct_start(WubuCt *ct);

/*
 * Wait for container to exit. Blocks until CT_EXITED.
 * Returns exit code.
 */
int wubu_ct_wait(WubuCt *ct);

/*
 * Send signal to container process.
 */
int wubu_ct_kill(WubuCt *ct, int sig);

/*
 * Get container state.
 */
CtState wubu_ct_state(WubuCt *ct);

/* -- Container Configuration -------------------------------------- */

/* Set command to execute inside container */
int wubu_ct_set_cmd(WubuCt *ct, int argc, char **argv);

/* Add environment variable (format: "KEY=VALUE") */
int wubu_ct_add_env(WubuCt *ct, const char *env);

/* Add a 9P bind mount: host_path → guest_path inside container */
int wubu_ct_add_bind(WubuCt *ct, const char *host, const char *guest, bool ro);

/* Enable GPU passthrough (/dev/dri, /dev/nvidia into container) */
void wubu_ct_set_gpu(WubuCt *ct, bool enable);

/* Set resource limits */
void wubu_ct_set_limits(WubuCt *ct, uint64_t mem_mb, int cpu_count);

/* -- Preset Containers -------------------------------------------- */

/*
 * Create a SteamOS container preset.
 * Base: Arch rootfs + Steam Runtime + Proton
 * GPU: enabled (DRM/KMS passthrough)
 * Binds: /dev/dri, /dev/nvidia, /dev/shm, /tmp/.X11-unix
 */
WubuCt *wubu_ct_steamos(const char *name, const char *root);

/*
 * Create a native Linux container preset.
 * Base: Arch rootfs
 * GPU: optional
 */
WubuCt *wubu_ct_native(const char *name, const char *root);

/* -- Bubblewrap Presets (Hosted Demo) ------------------------------- */

/*
 * Create a native Linux container using bubblewrap (no chroot needed).
 * Uses host filesystem with bind mounts. Shares Wayland socket for GUI.
 */
WubuCt *wubu_ct_bwrap_native(const char *name);

/* Bubblewrap start function (replaces wubu_ct_start for hosted mode) */
int wubu_ct_start_bwrap(WubuCt *ct);

/* -- Diagnostics -------------------------------------------------- */

/* Get human-readable state name */
const char *wubu_ct_state_name(CtState state);

/* Get human-readable runtime name */
const char *wubu_ct_runtime_name(CtRuntime runtime);

#endif /* WUBU_HOST_EXEC_H */
