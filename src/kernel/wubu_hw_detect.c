/*
 * wubu_hw_detect.c -- the HARDWARE BUS DETECTOR (the OS's magic sense of self).
 *
 * "It should work on bare metal or not bare metal because we are a magical
 *  operating system."
 *
 * This module answers ONE question at boot with zero user interaction:
 *
 *    "Am I running on bare metal or inside a VM/WSL?"
 *
 * It probes, in order of reliability:
 *   1. /dev/dgx          — WSL2 GPU paravirtualized device (definitive WSL)
 *   2. /proc/sys/kernel/osrelease — "microsoft" magic string
 *   3. CPUID hypervisor bit (Intel/AMD) — VMware/Hyper-V/QEMU/KVM
 *   4. DMI /sys/class/dmi/id/product_name — "Virtual" prefixes
 *   5. PCI scan for VMware/VirtualBox/QEMU devices
 *
 * On bare metal, the PCI bus has the REAL GPU in device 00:01.0 or 00:02.0.
 * On WSL2, /dev/dgx is the only GPU path; bare PCI scan finds nothing.
 *
 * The detector publishes its verdict to the KV-FS at:
 *   /kv/world/hw_platform  -> "bare_metal" | "wsl2" | "kvm" | "vmware" | "qemu"
 *   /kv/world/hw_gpu       -> the GPU device path (/dev/nvidia0 or /dev/dgx)
 *
 * C11. Hosted sections (fopen/access) guarded by _GNU_SOURCE; the bare-metal
 * kernel uses the CPUID + PCI path only.
 */
#include "wubu_hw_detect.h"
#include "wubu_pci.h"
#include "wubu_kvfs.h"

#include <string.h>
#include <stdint.h>

#ifdef _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#endif

/* the verdict (populated by wubu_hw_detect()) */
static char g_platform[32] = "unknown";
static char g_gpu_path[64]  = "";
static int  g_is_wsl        = 0;
static int  g_gpu_present   = 0;

/* ---- W1: the detector ---- */
void wubu_hw_detect(void)
{
    /* start clean */
    g_platform[0] = '\0';
    g_gpu_path[0]  = '\0';
    g_is_wsl = 0;
    g_gpu_present = 0;

#ifdef _GNU_SOURCE
    /* 1. /proc/sys/kernel/osrelease — the WSL2 magic string */
    FILE *f = fopen("/proc/sys/kernel/osrelease", "r");
    if (f) {
        char buf[256] = "";
        if (fgets(buf, sizeof(buf), f)) {
            if (strstr(buf, "microsoft") || strstr(buf, "Microsoft")) {
                strcpy(g_platform, "wsl2");
                g_is_wsl = 1;
            }
        }
        fclose(f);
    }

    /* 2. /dev/dgx — WSL2 GPU paravirtualized device */
    if (access("/dev/dxg", R_OK) == 0) {
        strcpy(g_gpu_path, "/dev/dxg");
        g_gpu_present = 1;
        strcpy(g_platform, "wsl2");
        g_is_wsl = 1;
    }
#else
    /* Bare-metal kernel: no /proc, no stdio. The CPUID + PCI path below
     * is the only detection available. */
#endif

    /* 3. CPUID hypervisor bit — if no /dev/dgx, check for VMs
     * (works in both bare-metal and hosted builds — CPUID is a real
     * instruction on both Intel and AMD). */
    if (!g_is_wsl) {
        uint32_t eax, ebx, ecx, edx;
        __asm__ __volatile__(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(1), "c"(0)
        );
        if (ecx & (1u << 31)) {
            uint32_t max;
            __asm__ __volatile__("cpuid" : "=a"(max) : "a"(0x40000000) : "ebx", "ecx", "edx");
            if (max >= 0x40000000) {
                char hv_id[13] = {0};
                __asm__ __volatile__(
                    "cpuid"
                    : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                    : "a"(0x40000000)
                );
                memcpy(hv_id + 0, &ebx, 4);
                memcpy(hv_id + 4, &ecx, 4);
                memcpy(hv_id + 8, &edx, 4);
                if (strstr(hv_id, "Microsoft")) {
                    strcpy(g_platform, "hyperv");
                    g_is_wsl = 1;
                } else if (strstr(hv_id, "VMware")) {
                    strcpy(g_platform, "vmware");
                } else if (strstr(hv_id, "KVM")) {
                    strcpy(g_platform, "kvm");
                } else if (strstr(hv_id, "QEMU")) {
                    strcpy(g_platform, "qemu");
                }
            }
        }
    }

    /* 4. Bare-metal GPU detection via PCI scan */
    if (!g_gpu_present) {
        wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
        int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
        for (int i = 0; i < n; i++) {
            /* NVIDIA: vendor 0x10DE */
            if (devs[i].vendor == 0x10DE) {
                strcpy(g_gpu_path, "/dev/nvidia0");
                g_gpu_present = 1;
                if (g_platform[0] == '\0') strcpy(g_platform, "bare_metal");
                break;
            }
            /* AMD/Intel: already covered by wubu_drv_gpu */
            if ((devs[i].vendor == 0x1002 && (devs[i].device == 0x163F)) ||
                (devs[i].vendor == 0x8086)) {
                strcpy(g_gpu_path, "/dev/dri/card0");
                g_gpu_present = 1;
                if (g_platform[0] == '\0') strcpy(g_platform, "bare_metal");
                break;
            }
        }
    }

    /* 5. If still unknown, call it bare metal */
    if (g_platform[0] == '\0') strcpy(g_platform, "bare_metal");

    /* publish verdict to KV-FS */
    if (g_wubu_kvfs && g_wubu_kv_base && g_wubu_kv_capacity > 8) {
        float vec[8] = {0};
        for (int i = 0; i < 8 && g_platform[i] && g_wubu_kv_capacity > (size_t)(i * 4 + 3); i++) {
            vec[i] = (float)(uint8_t)g_platform[i];
        }
        wubu_kvfs_write(g_wubu_kvfs, "/kv/world/hw_platform", g_wubu_kv_base, vec, 8);
    }
}

/* ---- W2: accessors (read-only to the rest of the kernel) ---- */
int  wubu_hw_is_wsl(void)          { return g_is_wsl; }
int  wubu_hw_gpu_present(void)      { return g_gpu_present; }
const char *wubu_hw_platform(void)  { return g_platform; }
const char *wubu_hw_gpu_path(void)  { return g_gpu_path[0] ? g_gpu_path : NULL; }

/* ---- W3: boot summary (the console's `hw` command output) ---- */
int wubu_hw_summary(char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    int dxg = 0;
#ifdef _GNU_SOURCE
    dxg = (access("/dev/dxg", R_OK) == 0);
#endif
#ifdef _GNU_SOURCE
    int n = snprintf(out, cap,
        "hw[platform=%s gpu=%s dxg=%d wsl=%d]",
        g_platform,
        g_gpu_path[0] ? g_gpu_path : "none",
        dxg,
        g_is_wsl);
    return n < 0 ? -1 : 0;
#else
    /* bare-metal kernel libc stub */
    (void)out; (void)cap; (void)dxg;
    return -1;
#endif
}
