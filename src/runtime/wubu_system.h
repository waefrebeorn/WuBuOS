/*
 * wubu_system.h -- WuBuOS System Root (immutable / atomic) layer
 *
 * SteamOS lesson: the OS ships with a read-only root filesystem and applies
 * updates atomically (download new image to inactive slot, reboot into it).
 * WuBuOS already has a real snapshot/overlayfs manager (wubu_snapshot.c); this
 * module wraps it so the *whole system root* is a snapshot-backed, rollback-able
 * unit with a read-only base -- exactly the SteamOS A/B + immutable-root model.
 *
 * Design (no stubs, every fn does real work):
 *   - One WubuSnapshotManager owns the system root store (~/.wubu/system).
 *   - The active system state is a named branch ("main"); each commit is a
 *     snapshot. The current read-only base is the snapshot marked read_only.
 *   - wubu_system_commit() stamps a new read-only base from the live upper layer
 *     (like RAUC writing an inactive slot), then flips the active pointer.
 *   - wubu_system_rollback() restores the previous base via wubu_snapshot_rollback.
 *   - DEVELOPER mode (WUBU_SYSTEM_DEV=1) disables read-only enforcement so the
 *     tree is mutable for development (documented escape hatch).
 */

#ifndef WUBU_SYSTEM_H
#define WUBU_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

/* Default store location for the system-root snapshot manager. */
#define WUBU_SYSTEM_STORE      "~/.wubu/system"
#define WUBU_SYSTEM_BRANCH     "main"

/* -- Lifecycle --------------------------------------------------- */

/* Initialize the system-root manager. Allocates and owns an internal
 * WubuSnapshotManager. Returns 0 on success. */
int  wubu_system_init(const char *store_path);

/* Shut down and release the system-root manager. */
void wubu_system_shutdown(void);

/* True once wubu_system_init() has succeeded. */
bool wubu_system_ready(void);

/* -- Immutable / atomic operations ------------------------------ */

/* Commit the live system state as a new read-only base snapshot labeled
 * `label` (e.g. "2026-07-07-stable"). The previous base remains available for
 * rollback. Returns 0 on success and writes the new snapshot id into out_id
 * (may be NULL). */
int  wubu_system_commit(const char *label, char *out_id, int id_len);

/* Roll the active system back to the snapshot identified by snapshot_id.
 * Returns 0 on success. Restores the previous read-only base via the snapshot
 * manager's rollback (real overlayfs tree copy). */
int  wubu_system_rollback(const char *snapshot_id);

/* -- State queries ---------------------------------------------- */

/* Is the system root currently enforcing a read-only base? (false in DEVELOPER
 * mode). This is the SteamOS "read-only rootfs" guarantee. */
bool wubu_system_is_readonly(void);

/* Return the active system baseline label (last successful commit), or ""
 * if none. Caller passes a buffer of at least WUBU_SYS_LABEL_MAX bytes. */
int  wubu_system_active_label(char *out_label, int label_len);

/* Number of committed system baselines (snapshots on the system branch). */
int  wubu_system_baseline_count(void);

/* DEVELOPER mode flag (read from WUBU_SYSTEM_DEV env at init). */
bool wubu_system_developer_mode(void);

#define WUBU_SYS_LABEL_MAX 128

#endif /* WUBU_SYSTEM_H */
