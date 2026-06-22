/*
 * wubu_archd.h  --  WuBuOS Arch Linux Daemon
 *
 * The Arch daemon manages Arch Linux container roots for WuBuOS.
 * It is the "systemd" of the Arch NT layer — managing roots,
 * packages, services, and container lifecycle.
 *
 * Architecture (learned from SteamOS + Ubuntu):
 *   - Like SteamOS pressure-vessel: manages container roots with
 *     namespace isolation, bind mounts, GPU passthrough
 *   - Like Ubuntu systemd: service management, socket activation,
 *     health monitoring, auto-restart
 *   - Like Ubuntu NetworkManager: network configuration per root
 *   - Like Ubuntu udisks2: storage management for rootfs images
 *
 * Daemon responsibilities:
 *   1. Root lifecycle: create, destroy, clone, snapshot, rollback
 *   2. Package management: install, remove, update, search, query
 *   3. Service management: enable, disable, start, stop, restart
 *   4. Health monitoring: ping, resource usage, auto-heal
 *   5. Network config: DHCP, DNS, firewall per root
 *   6. Storage: rootfs images, mount management, disk quotas
 *   7. GPU passthrough: detect, configure, validate
 *   8. Event bus: publish state changes to WuBuOS desktop
 *
 * Communication: Unix domain socket + JSON protocol
 *   Client: wubu_archctl (CLI tool)
 *   Server: wubu_archd (this daemon)
 */

#ifndef WUBU_ARCHD_H
#define WUBU_ARCHD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define WUBU_ARCHD_SOCKET_PATH  "/run/wubu/archd.sock"
#define WUBU_ARCHD_PID_PATH     "/run/wubu/archd.pid"
#define WUBU_ARCHD_LOG_PATH     "/var/log/wubu/archd.log"
#define WUBU_ARCHD_ROOTS_PATH   "/var/wubu/roots"
#define WUBU_ARCHD_IMAGES_PATH  "/var/wubu/images"
#define WUBU_ARCHD_MAX_ROOTS    64
#define WUBU_ARCHD_MAX_SERVICES 256
#define WUBU_ARCHD_MAX_PACKAGE_NAME 128
#define WUBU_ARCHD_MAX_ROOT_NAME    64
#define WUBU_ARCHD_MAX_PATH         512
#define WUBU_ARCHD_MAX_CMD          1024
#define WUBU_ARCHD_MAX_RESPONSE     8192
#define WUBU_ARCHD_VERSION          "0.1.0"

/* -- Root States (like systemd unit states) ------------------------ */

typedef enum {
    ROOT_STATE_INACTIVE = 0,    /* Root exists but not mounted */
    ROOT_STATE_ACTIVATING,      /* Being created/mounted */
    ROOT_STATE_ACTIVE,          /* Mounted and ready */
    ROOT_STATE_DEACTIVATING,    /* Being unmounted/destroyed */
    ROOT_STATE_FAILED,          /* Error state, needs intervention */
    ROOT_STATE_MAINTENANCE,     /* Update/upgrade in progress */
    ROOT_STATE_SNAPSHOT,        /* Snapshot in progress */
} WubuArchRootState;

/* -- Root Types (like Steam Runtime versions) ---------------------- */

typedef enum {
    ROOT_TYPE_BASE = 0,         /* Minimal Arch base */
    ROOT_TYPE_GUI,              /* Base + X11/Wayland + Mesa */
    ROOT_TYPE_STEAM,            /* Base + Steam Runtime */
    ROOT_TYPE_GAMING,           /* Base + Proton + DXVK + GPU */
    ROOT_TYPE_PROTON,           /* Base + Wine + DXVK + VKD3D */
    ROOT_TYPE_DEVELOP,          /* Base + dev tools + compilers */
    ROOT_TYPE_CUSTOM,           /* User-defined */
} WubuArchRootType;

/* -- Service States (like systemd) -------------------------------- */

typedef enum {
    SERVICE_STATE_DISABLED = 0,
    SERVICE_STATE_ENABLED,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_FAILED,
    SERVICE_STATE_RESTARTING,
} WubuArchServiceState;

/* -- Arch Daemon Root Record -------------------------------------- */

typedef struct {
    char name[WUBU_ARCHD_MAX_ROOT_NAME];
    char path[WUBU_ARCHD_MAX_PATH];
    WubuArchRootType type;
    WubuArchRootState state;
    time_t created;
    time_t last_used;
    time_t last_updated;
    uint64_t disk_usage_bytes;
    uint64_t disk_limit_bytes;
    int container_count;        /* Number of active containers */
    bool auto_update;
    bool gpu_passthrough;
    char gpu_pci[64];           /* PCI address of passed-through GPU */
} WubuArchdRoot;

/* -- Service Record ----------------------------------------------- */

typedef struct {
    char name[WUBU_ARCHD_MAX_PACKAGE_NAME];
    char root_name[WUBU_ARCHD_MAX_ROOT_NAME];
    WubuArchServiceState state;
    int pid;                    /* Running PID, 0 if not running */
    time_t last_start;
    time_t last_stop;
    int restart_count;
    bool auto_restart;
} WubuArchService;

/* -- Daemon Configuration ----------------------------------------- */

typedef struct {
    char roots_path[WUBU_ARCHD_MAX_PATH];
    char images_path[WUBU_ARCHD_MAX_PATH];
    char socket_path[WUBU_ARCHD_MAX_PATH];
    char log_path[WUBU_ARCHD_MAX_PATH];
    char mirror[256];
    bool auto_update;
    int  health_check_interval_sec;
    int  update_check_interval_sec;
    int  max_roots;
    int  log_level;             /* 0=error, 1=warn, 2=info, 3=debug */
    bool daemonize;
    bool gpu_detect;
} WubuArchdConfig;

/* -- Daemon State ------------------------------------------------- */

typedef struct {
    WubuArchdConfig config;
    WubuArchdRoot roots[WUBU_ARCHD_MAX_ROOTS];
    int root_count;
    WubuArchService services[WUBU_ARCHD_MAX_SERVICES];
    int service_count;
    bool running;
    int server_fd;              /* Unix socket */
    int epoll_fd;               /* Event loop */
    time_t start_time;
    uint64_t requests_handled;
    uint64_t errors;
} WubuArchd;

/* -- Request/Response Protocol ------------------------------------ */

typedef enum {
    /* Root lifecycle */
    ARCHD_CMD_ROOT_CREATE = 1,
    ARCHD_CMD_ROOT_DESTROY,
    ARCHD_CMD_ROOT_LIST,
    ARCHD_CMD_ROOT_INFO,
    ARCHD_CMD_ROOT_CLONE,
    ARCHD_CMD_ROOT_SNAPSHOT,
    ARCHD_CMD_ROOT_ROLLBACK,
    ARCHD_CMD_ROOT_MOUNT,
    ARCHD_CMD_ROOT_UNMOUNT,

    /* Package management */
    ARCHD_CMD_PKG_INSTALL,
    ARCHD_CMD_PKG_REMOVE,
    ARCHD_CMD_PKG_UPDATE,
    ARCHD_CMD_PKG_SEARCH,
    ARCHD_CMD_PKG_INFO,
    ARCHD_CMD_PKG_LIST,

    /* Service management */
    ARCHD_CMD_SVC_ENABLE,
    ARCHD_CMD_SVC_DISABLE,
    ARCHD_CMD_SVC_START,
    ARCHD_CMD_SVC_STOP,
    ARCHD_CMD_SVC_RESTART,
    ARCHD_CMD_SVC_STATUS,
    ARCHD_CMD_SVC_LIST,

    /* Health & monitoring */
    ARCHD_CMD_PING,
    ARCHD_CMD_STATS,
    ARCHD_CMD_HEALTH,
    ARCHD_CMD_LOG,

    /* GPU */
    ARCHD_CMD_GPU_DETECT,
    ARCHD_CMD_GPU_LIST,
    ARCHD_CMD_GPU_ASSIGN,

    /* Daemon control */
    ARCHD_CMD_SHUTDOWN,
    ARCHD_CMD_RELOAD,
    ARCHD_CMD_VERSION,
} WubuArchdCmd;

typedef struct {
    WubuArchdCmd cmd;
    char root_name[WUBU_ARCHD_MAX_ROOT_NAME];
    char data[2048];            /* JSON payload */
} WubuArchdRequest;

typedef struct {
    int status;                 /* 0=success, -1=error */
    char message[512];
    char data[WUBU_ARCHD_MAX_RESPONSE];
} WubuArchdResponse;

/* -- Daemon Lifecycle --------------------------------------------- */

int  wubu_archd_init(WubuArchd *d, const WubuArchdConfig *config);
int  wubu_archd_start(WubuArchd *d);
void wubu_archd_event_loop(WubuArchd *d);    /* Main event loop */
void wubu_archd_stop(WubuArchd *d);
void wubu_archd_shutdown(WubuArchd *d);

/* -- Root Operations ---------------------------------------------- */

int  wubu_archd_root_create(WubuArchd *d, const char *name,
                             WubuArchRootType type, const char *mirror);
int  wubu_archd_root_destroy(WubuArchd *d, const char *name);
int  wubu_archd_root_clone(WubuArchd *d, const char *src, const char *dst);
int  wubu_archd_root_snapshot(WubuArchd *d, const char *name, const char *snap_name);
int  wubu_archd_root_rollback(WubuArchd *d, const char *name, const char *snap_name);
int  wubu_archd_root_list(WubuArchd *d, WubuArchdRoot *out, int max);
int  wubu_archd_root_info(WubuArchd *d, const char *name, WubuArchdRoot *out);

/* -- Package Operations (delegates to pacman in root) ------------- */

int  wubu_archd_pkg_install(WubuArchd *d, const char *root,
                             const char *packages);
int  wubu_archd_pkg_remove(WubuArchd *d, const char *root,
                            const char *packages);
int  wubu_archd_pkg_update(WubuArchd *d, const char *root);
int  wubu_archd_pkg_list(WubuArchd *d, const char *root,
                          char *out, size_t out_size);

/* -- Service Operations ------------------------------------------- */

int  wubu_archd_svc_enable(WubuArchd *d, const char *root, const char *svc);
int  wubu_archd_svc_disable(WubuArchd *d, const char *root, const char *svc);
int  wubu_archd_svc_start(WubuArchd *d, const char *root, const char *svc);
int  wubu_archd_svc_stop(WubuArchd *d, const char *root, const char *svc);
int  wubu_archd_svc_restart(WubuArchd *d, const char *root, const char *svc);
int  wubu_archd_svc_status(WubuArchd *d, const char *root, const char *svc,
                            WubuArchService *out);

/* -- Health & Monitoring ------------------------------------------ */

int  wubu_archd_health_check(WubuArchd *d, const char *root);
int  wubu_archd_stats(WubuArchd *d, char *out, size_t out_size);
int  wubu_archd_gpu_detect(WubuArchd *d, char *out, size_t out_size);

/* -- Event Bus (publish to WuBuOS desktop) ------------------------ */

int  wubu_archd_publish_event(WubuArchd *d, const char *event_type,
                               const char *root_name, const char *data);

/* -- Utility ------------------------------------------------------ */

const char *wubu_archd_root_state_str(WubuArchRootState state);
const char *wubu_archd_root_type_str(WubuArchRootType type);
const char *wubu_archd_svc_state_str(WubuArchServiceState state);
const char *wubu_archd_cmd_str(WubuArchdCmd cmd);
const char *wubu_archd_version(void);

#endif /* WUBU_ARCHD_H */
