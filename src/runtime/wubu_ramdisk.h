/*
 * wubu_ramdisk.h — WuBuOS Root Mount for Arch Container Roots
 *
 * Cell 392: Two-mode Arch root — RAM for containers, SSD for bare metal.
 *
 * "Por qué no los dos" — again.
 *
 * CONTAINER MODE (hosted):
 *   - Default: tmpfs mount, Arch root lives in RAM
 *   - Zero disk writes during runtime
 *   - Instant teardown on shutdown (unmount = gone)
 *   - Can snapshot → SSD for persistence opt-in
 *
 * BARE METAL MODE:
 *   - SSD install, Arch root on disk
 *   - Persistent across reboots
 *   - Normal filesystem, no tmpfs tricks
 *   - This is the "real OS on real hardware" path
 *
 * Both paths use the same container runtime (wubu_host_exec).
 * The only difference is where the chroot root lives:
 *   - /run/wubu/ramdisk  (tmpfs, container mode)
 *   - /var/wubu/roots/arch-base  (SSD, bare metal)
 *
 * Flow - Container Mode:
 *   1. wubu_rd_create(RD_RAM)  → configure tmpfs
 *   2. wubu_rd_boot()          → mount tmpfs + unpack rootfs image
 *   3. Containers chroot into /run/wubu/ramdisk
 *   4. wubu_rd_snapshot()      → optional: save to SSD
 *   5. wubu_rd_destroy()       → unmount tmpfs (RAM contents vanish)
 *
 * Flow - Bare Metal Mode:
 *   1. wubu_rd_create(RD_DISK) → configure SSD path
 *   2. wubu_rd_boot()          → bootstrap Arch on SSD (if not exists)
 *   3. Containers chroot into /var/wubu/roots/arch-base
 *   4. wubu_rd_destroy()       → no-op (SSD stays)
 *
 * Flow - Install to SSD:
 *   1. Running in container mode (RAM)
 *   2. wubu_rd_install_to_disk() → copy RAM contents → SSD
 *   3. Now bare metal boots from that SSD root
 */
#ifndef WUBU_RAMDISK_H
#define WUBU_RAMDISK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Root Mount Mode ────────────────────────────────────────────── */

typedef enum {
    WUBU_RD_RAM  = 0,   /* tmpfs — container/hosted mode */
    WUBU_RD_DISK = 1,   /* SSD/disk — bare metal mode */
} WubuRdMode;

typedef enum {
    WUBU_RD_NONE      = 0,   /* Not mounted/created */
    WUBU_RD_MOUNTED   = 1,   /* tmpfs mounted or dir verified */
    WUBU_RD_LOADED    = 2,   /* Rootfs unpacked / Arch bootstrapped */
    WUBU_RD_FAILED    = 3,   /* Mount/unpack failed */
} WubuRdState;

/* ── Configuration ──────────────────────────────────────────────── */

#define WUBU_RD_RAM_PATH       "/run/wubu/ramdisk"
#define WUBU_RD_DISK_PATH      "/var/wubu/roots/arch-base"
#define WUBU_RD_RAM_SIZE       "2048m"          /* 2GB default tmpfs */
#define WUBU_RD_IMAGE_DEFAULT  "dist/rootfs.cgz"

typedef struct {
    char     path[512];        /* Mount point (RAM or disk) */
    char     ram_size[32];     /* tmpfs size limit (RAM mode only) */
    char     image_path[512];  /* Compressed rootfs image */
    char     disk_path[512];   /* SSD target for install_to_disk */
    WubuRdMode mode;           /* RAM or DISK */
    WubuRdState state;         /* Current state */
    uint64_t usage_mb;         /* Current usage in MB */
    uint64_t limit_mb;         /* Size limit in MB (RAM mode) */
} WubuRamdisk;

/* ── Lifecycle ──────────────────────────────────────────────────── */

/*
 * Create root mount configuration.
 *
 * mode:       RD_RAM (container) or RD_DISK (bare metal)
 * image_path: rootfs image to load (NULL = default)
 *
 * RAM mode:  path = /run/wubu/ramdisk, tmpfs mounted
 * DISK mode: path = /var/wubu/roots/arch-base, directory created
 */
WubuRamdisk *wubu_rd_create(WubuRdMode mode, const char *image_path);

/*
 * Destroy root mount handle.
 * RAM mode: unmounts tmpfs (data vanishes from RAM).
 * DISK mode: no-op (SSD data persists).
 */
void wubu_rd_destroy(WubuRamdisk *rd);

/*
 * Boot the root mount.
 *
 * RAM mode:  mount tmpfs + unpack rootfs image into it.
 * DISK mode: mkdir -p + bootstrap Arch if not exists.
 *
 * Returns: 0 on success, -1 on failure.
 */
int wubu_rd_boot(WubuRamdisk *rd);

/*
 * Get root path — the chroot target for containers.
 * RAM:  /run/wubu/ramdisk
 * DISK: /var/wubu/roots/arch-base
 */
const char *wubu_rd_root_path(WubuRamdisk *rd);

/*
 * Get current state.
 */
WubuRdState wubu_rd_state(WubuRamdisk *rd);

/*
 * Get current usage in MB.
 */
uint64_t wubu_rd_usage_mb(WubuRamdisk *rd);

/* ── RAM Mode Operations ────────────────────────────────────────── */

/*
 * Mount tmpfs (RAM mode only).
 * Creates empty RAM-backed filesystem at path.
 */
int wubu_rd_mount(WubuRamdisk *rd);

/*
 * Load rootfs image into mount point.
 * Auto-detects format: .cpio.gz, .tar.gz, .tar.zst, directory.
 */
int wubu_rd_load(WubuRamdisk *rd);

/*
 * Unmount tmpfs (RAM mode only).
 * ALL DATA IN RAM IS DESTROYED.
 */
int wubu_rd_unmount(WubuRamdisk *rd);

/* ── DISK Mode Operations ───────────────────────────────────────── */

/*
 * Bootstrap Arch directly onto SSD (DISK mode).
 * Runs pacstrap into the disk path.
 * Requires: root, arch-install-scripts, network.
 */
int wubu_rd_bootstrap_disk(WubuRamdisk *rd, const char *mirror);

/* ── Cross-Mode: Install RAM → SSD ─────────────────────────────── */

/*
 * Install ramdisk contents to SSD.
 * Copies entire RAM rootfs to disk_path.
 * After this, bare metal boots from that SSD root.
 *
 * This is the "save my container state to disk" operation.
 * Use it when you want persistence from container mode.
 *
 * disk_path: SSD target (NULL = /var/wubu/roots/arch-base)
 */
int wubu_rd_install_to_disk(WubuRamdisk *rd, const char *disk_path);

/*
 * Snapshot rootfs to compressed image.
 * RAM mode: snapshots RAM contents.
 * DISK mode: snapshots SSD contents.
 *
 * output_path: where to write (NULL = dist/rootfs.cgz)
 * Creates: tar.zst (zstd-compressed tar)
 */
int wubu_rd_snapshot(WubuRamdisk *rd, const char *output_path);

/* ── Package Management ─────────────────────────────────────────── */

/*
 * Install packages into root.
 * RAM mode: pacstrap into tmpfs.
 * DISK mode: pacstrap into SSD.
 */
int wubu_rd_install(WubuRamdisk *rd, const char *packages);

/*
 * Set tmpfs size limit (RAM mode only).
 */
void wubu_rd_set_ram_size(WubuRamdisk *rd, const char *size_str);

#endif /* WUBU_RAMDISK_H */
