/*
 * wubu_ns_bridge.h -- WuBuOS Namespace Bridge: expose archd services
 * and bottles as a uniform 9P/Styx control plane.
 *
 * THE PLAY: every other OS controls services/apps through a DIFFERENT
 * mechanism -- systemd (sockets/cgroups), launchd (XPC), Flatpak (D-Bus
 * portals), Bottles (JSON over Python). WuBuOS does ALL of it through ONE
 * uniform filesystem namespace under /n. That is strictly better:
 *
 *   systemctl start  gamescope   ->   echo start  > /n/svc/root/gamescope/ctl
 *   systemctl status gamescope   ->   cat             /n/svc/root/gamescope/status
 *   bottles-cli run <name>       ->   echo run    > /n/bottles/<name>/ctl
 *   bottles-cli verify <name>    ->   cat             /n/bottles/<name>/verify
 *   snapper rollback <id>        ->   echo <id>   > /n/snap/<c>/rollback
 *   snapper list                 ->   cat             /n/snap/<c>/list
 *
 * The bridge synthesizes a real /n tree on disk (so it can be served by the
 * existing Styx/9P host server) and maps file reads/writes to the archd
 * service API and the bottle API.
 *
 * Dependency injection: production wires wubu_ns_svc_ops_real (the real
 * wubu_archd_svc_* functions). Tests inject mock ops so the dispatch
 * routing is verified without shelling out to arch-chroot/systemctl.
 *
 * C11, opaque structs, minimal includes -- no god headers.
 */

#ifndef WUBU_NS_BRIDGE_H
#define WUBU_NS_BRIDGE_H

#include "wubu_archd.h"   /* WubuArchd, WubuArchService, WubuArchServiceState */
#include "wubu_bottles.h" /* WubuBottle */
#include "wubu_snapshot.h" /* WubuSnapshotManager, WubuSnapshot */
#include "wubu_pkg.h"     /* PkgManager, PkgEntry */

#ifdef __cplusplus
extern "C" {
#endif

/* Injectable service control operations. Production points these at the
 * real wubu_archd_svc_* functions; tests inject mocks. */
typedef struct wubu_ns_svc_ops {
    int (*svc_start)(WubuArchd *d, const char *root, const char *svc);
    int (*svc_stop)(WubuArchd *d, const char *root, const char *svc);
    int (*svc_restart)(WubuArchd *d, const char *root, const char *svc);
    int (*svc_enable)(WubuArchd *d, const char *root, const char *svc);
    int (*svc_disable)(WubuArchd *d, const char *root, const char *svc);
    int (*svc_status)(WubuArchd *d, const char *root, const char *svc,
                      WubuArchService *out);
} wubu_ns_svc_ops_t;

/* The real (production) ops table -- wires wubu_archd_svc_*. */
extern const wubu_ns_svc_ops_t wubu_ns_svc_ops_real;

/* Service-state enum -> short string (active/inactive/...). */
const char *wubu_ns_state_str(WubuArchServiceState state);

/* -- Namespace lifecycle ------------------------------------------ */

/* Create the /n tree under ns_root (must be an existing, writable dir).
 * Populates /n/svc and /n/bottles. Returns 0 on success, -1 on error. */
int wubu_ns_bridge_create(const char *ns_root);

/* -- Service publishing ------------------------------------------- */

/* Publish a service as /n/svc/<root>/<svc>/{status,ctl}.
 * status is written from ops->svc_status; ctl is an action file whose
 * writes are dispatched by wubu_ns_svc_ctl(). Returns 0/-1. */
int wubu_ns_publish_service(WubuArchd *d, const char *root, const char *svc,
                            const wubu_ns_svc_ops_t *ops);

/* Pure dispatcher: route a ctl command to the right service op.
 * cmd in {"start","stop","restart","enable","disable"}. Returns the op
 * result, or -1 if cmd is unknown. This is what a 9P write to ctl calls. */
int wubu_ns_svc_ctl(WubuArchd *d, const char *root, const char *svc,
                    const char *cmd, const wubu_ns_svc_ops_t *ops);

/* -- Bottle publishing -------------------------------------------- */

/* Publish a bottle as /n/bottles/<name>/{info,verify,ctl}.
 * info = name/type/runner/installed/verified; verify written from
 * wubu_bottle_verify(); ctl dispatches run/verify actions. Returns 0/-1. */
int wubu_ns_publish_bottle(const WubuBottle *b, const char *name);

/* Pure dispatcher for bottle actions. action in {"run","verify"}.
 * Returns the bottle API result. This is what a 9P write to ctl calls. */
int wubu_ns_bottle_action(WubuBottle *b, const char *action);

/* -- Snapshot publishing (rip off snapper/btrfs rollback) --------- */

/* Publish a container's snapshots as /n/snap/<container>/{list,create,
 * rollback,delete}. list = formatted wubu_snapshot_list snapshot; the ctl
 * files map writes to wubu_ns_snap_create/rollback/delete. Returns 0/-1. */
int wubu_ns_publish_snapshots(WubuSnapshotManager *mgr, const char *container_id);

/* Render the snapshot list for a container into buf (one line per snap:
 * "<id>\t<label>\t<status>\t<size_bytes>"). Returns bytes written or -1. */
int wubu_ns_snap_list_str(WubuSnapshotManager *mgr, const char *container_id,
                          char *buf, size_t buf_size);

/* Pure dispatchers (what a 9P write to the ctl files calls). */
int wubu_ns_snap_create(WubuSnapshotManager *mgr, const char *container_id,
                        const char *label);
int wubu_ns_snap_rollback(WubuSnapshotManager *mgr, const char *snapshot_id);
int wubu_ns_snap_delete(WubuSnapshotManager *mgr, const char *snapshot_id);

/* -- Packages (rip off pacman/Chaotic-AUR) -------------------- */
int wubu_ns_publish_pkg(PkgManager *mgr);
int wubu_ns_pkg_install(PkgManager *mgr, const char *name);
int wubu_ns_pkg_remove(PkgManager *mgr, const char *name);
int wubu_ns_pkg_add_repo(PkgManager *mgr, const char *name, const char *url);

/* -- Kernel + HW (rip off kernel-manager / chwd) -------------- */
typedef int (*wubu_ns_gpu_detect_fn)(char *name, int name_len, char *pci, int pci_len);
int wubu_ns_publish_kernel(wubu_ns_gpu_detect_fn detect_fn);
int wubu_ns_sched_set(const char *policy);
const char *wubu_ns_sched_get(void);
int wubu_ns_hw_detect(wubu_ns_gpu_detect_fn detect_fn, char *mode_out, size_t n);
int wubu_ns_hw_set_mode(const char *gpu, const char *mode);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_NS_BRIDGE_H */
