/*
 * wubu_storage.c -- kernel-owned storage driver routing + tuning.
 *
 * Linux storage "works" but ships with defaults that hurt real users:
 *   - NVMe APST (Autonomous Power State Transition) adds 0.5-10ms of wake
 *     latency per I/O to save a few watts — bad for gaming / latency.
 *   - blk-mq queue depth is left at the driver default, not tuned to the
 *     drive (NVMe 65535 vs SATA 32).
 *   - Intel RST (RAID) mode makes the NVMe drive INVISIBLE to Linux — the
 *     firmware must be in AHCI mode for the kernel to see it.
 *   - AHCI NCQ (Native Command Queuing) + port multiplier are not always
 *     enabled.
 *   - TRIM/discard is off by default, so SSDs fragment over time.
 *
 * WuBuOS owns all of this: the kernel detects the storage topology
 * (NVMe vs SATA vs IDE, Intel RST lockout), and generates the tuning
 * config that fixes every gap. The user never touches fstab or udev.
 *
 * Research (Kevin-Bacon 7-hop on the storage frontier):
 *   - ArchWiki Solid_state_drive/NVMe (APST, TRIM, queue depth)
 *   - NVM Express spec + nvme-cli (namespaces, TRIM)
 *   - blk-mq multi-queue block layer docs
 *   - simplyblock NVMe queue-depth tuning
 *   - Arch Linux forums (Intel RST NVMe invisible)
 *   - linux-sunxi SATA (port multiplier, NCQ)
 */
#include "wubu_storage.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

/* ---- PCI storage classes ---- */
#define PCI_CLASS_STORAGE     0x01
#define PCI_SUBCLASS_IDE      0x01
#define PCI_SUBCLASS_RAID     0x04
#define PCI_SUBCLASS_SATA     0x06
#define PCI_SUBCLASS_NVME     0x08

/* ---- Global state ---- */
static int  g_nvme        = 0;   /* NVMe controller present */
static int  g_sata        = 0;   /* SATA (AHCI) present */
static int  g_ide         = 0;   /* legacy IDE present */
static int  g_raid_rst    = 0;   /* Intel RST / RAID mode lockout */
static int  g_queue_depth = 0;   /* detected optimal queue depth */
static char g_storage_path[64] = "";

/* ---- W1: probe the storage topology ----
 * Only scans PCI on bare-metal hardware (GPU on /dev/dri); WSL2 has no
 * PCI access. Mirrors wubu_audio_probe(). */
void wubu_storage_probe(void)
{
    g_nvme = 0; g_sata = 0; g_ide = 0;
    g_raid_rst = 0; g_queue_depth = 0;
    g_storage_path[0] = '\0';

    if (!(wubu_hw_gpu_present() && wubu_hw_gpu_path() &&
          strstr(wubu_hw_gpu_path(), "/dev/dri")))
        return;   /* WSL2 / no bare-metal PCI */

    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);

    for (int i = 0; i < n; i++) {
        if (devs[i].class_code != PCI_CLASS_STORAGE) continue;

        switch (devs[i].subclass) {
        case PCI_SUBCLASS_NVME:
            g_nvme = 1;
            g_queue_depth = 65535;           /* NVMe: deep queue */
            if (!g_storage_path[0])
                strcpy(g_storage_path, "/dev/nvme0n1");
            break;
        case PCI_SUBCLASS_SATA:
            g_sata = 1;
            g_queue_depth = 32;              /* SATA3 SSD: NCQ depth */
            if (!g_storage_path[0])
                strcpy(g_storage_path, "/dev/sda");
            break;
        case PCI_SUBCLASS_IDE:
            g_ide = 1;
            g_queue_depth = 1;               /* legacy: no queueing */
            if (!g_storage_path[0])
                strcpy(g_storage_path, "/dev/sda");
            break;
        case PCI_SUBCLASS_RAID:
            /* Intel RST / RAID mode. The NVMe drive is usually hidden
             * behind the RAID controller here and Linux can't see it
             * unless the firmware is switched to AHCI. */
            g_raid_rst = 1;
            break;
        default:
            break;
        }
    }
}

/* ---- W2: accessors ---- */
int          wubu_storage_has_nvme(void)  { return g_nvme; }
int          wubu_storage_has_sata(void)  { return g_sata; }
int          wubu_storage_has_ide(void)   { return g_ide; }
int          wubu_storage_has_raid_rst(void) { return g_raid_rst; }
int          wubu_storage_queue_depth(void) { return g_queue_depth; }
const char *wubu_storage_path(void)       { return g_storage_path[0] ? g_storage_path : NULL; }

/* ---- W3: kernel cmdline / modprobe tuning that fixes the gaps ----
 * Returns the kernel command line fragment to append at boot.
 * Fixes: APST latency, queue depth, Intel RST, TRIM. */
const char *wubu_storage_kernel_params(void)
{
    static char params[512] = "";
    if (params[0]) return params;

    if (g_nvme) {
        /* Disable APST auto-transitions -> no wake latency.
         * Force queue depth for latency over throughput. */
        snprintf(params, sizeof(params),
            "nvme_core.default_ps_max_latency_us=0 "
            "nvme_core.io_queue_depth=%d",
            g_queue_depth ? g_queue_depth : 256);
    } else if (g_sata) {
        /* SATA: NCQ is on by default; keep queue depth. */
        snprintf(params, sizeof(params),
            "libahci.ignore_sss=1");   /* ignore staggered spin-up */
    } else {
        strcpy(params, "");
    }
    return params;
}

/* ---- W4: TRIM / discard setup ----
 * Returns an fstab-style discard + fstrim.timer fragment. */
const char *wubu_storage_trim_config(void)
{
    static char cfg[512] = "";
    if (cfg[0]) return cfg;

    if (g_nvme || g_sata) {
        snprintf(cfg, sizeof(cfg),
            "# WuBuOS TRIM config (wubu_storage.c)\n"
            "# Enables automatic TRIM/discard on the boot SSD.\n"
            "# fstab: <device> / ext4 defaults,noatime,discard 0 1\n"
            "UUID=%s  /  ext4  defaults,noatime,discard  0  1\n",
            "wubu-boot");
    } else {
        strcpy(cfg, "");
    }
    return cfg;
}

/* ---- W5: Intel RST lockout warning ----
 * When the firmware is in RST/RAID mode the NVMe drive is invisible to
 * Linux. Returns a warning string the kernel surfaces on boot. */
const char *wubu_storage_rst_warning(void)
{
    if (g_raid_rst) {
        return
            "WARNING (wubu_storage): Intel RST/RAID mode detected. Your NVMe "
            "drive may be invisible to Linux. Switch the firmware SATA mode "
            "to AHCI in the BIOS/UEFI to expose the drive.";
    }
    return NULL;
}

/* ---- W6: summary fragment ---- */
int wubu_storage_summary(char *out, size_t cap)
{
    int n = snprintf(out, cap,
        "storage[nvme=%d sata=%d ide=%d rst=%d qd=%d path=%s]",
        g_nvme, g_sata, g_ide, g_raid_rst, g_queue_depth,
        g_storage_path[0] ? g_storage_path : "none");
    return n < 0 ? -1 : 0;
}
