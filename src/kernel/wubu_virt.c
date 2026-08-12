/*
 * wubu_virt.c -- kernel-owned virtualization driver routing.
 *
 * WuBuOS must "run on everything" — including every hypervisor. The PV
 * (paravirtualized) drivers differ per host:
 *   - QEMU/KVM: virtio (virtio_blk, virtio_net, virtio_gpu, virtio_input)
 *   - Microsoft Hyper-V / Azure: hv_vmbus + hv_netvsc + storvsc + hv_utils
 *   - VMware: vmxnet3 (net), vmw_balloon, vmwgfx (GPU), mpt3sas
 *   - Xen: xen-blkfront, xen-netfront, xen-gntdev, xen-privcmd
 *   - Oracle VirtualBox: vboxguest, vboxsf (shared folders)
 *   - Parallels: prl_*
 *
 * Detection: CPUID hypervisor bit (0x40000000 leaf) + vendor string.
 * WuBuOS owns the detection and routes to the matching PV driver set.
 */
#include "wubu_virt.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

/* ---- Hypervisor vendor IDs (CPUID 0x40000000) ---- */
#define HYPERV_KVM      1
#define HYPERV_HYPERV   2   /* Microsoft Hyper-V */
#define HYPERV_VMWARE   3
#define HYPERV_XEN      4
#define HYPERV_VBOX     5
#define HYPERV_PARALLELS 6
#define HYPERV_NONE     0

/* ---- Global state ---- */
static int  g_hyper = HYPERV_NONE;
static int  g_virtio = 0;
static char g_pv_driver[32] = "";
static char g_hv_name[24] = "";

/* ---- W1: probe the hypervisor ---- */
void wubu_virt_probe(void)
{
    g_hyper = HYPERV_NONE;
    g_virtio = 0;
    g_pv_driver[0] = '\0';
    g_hv_name[0] = '\0';

#ifdef _GNU_SOURCE
    /* Hypervisor bit + vendor via CPUID. WuBuOS has a cpuid wrapper, but
     * for the hosted selftest read the hypervisor from /sys/hypervisor or
     * dmesg-visible hints, falling back to CPUID via __builtin_cpu. */
    char vendor[13] = {0};
    unsigned int a = 0, b = 0, c = 0, d = 0;
    __asm__ __volatile__("cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "a"(0x40000000));
    memcpy(vendor + 0, &b, 4);
    memcpy(vendor + 4, &c, 4);
    memcpy(vendor + 8, &d, 4);

    if (strncmp(vendor, "KVMKVMKVM", 9) == 0) {
        g_hyper = HYPERV_KVM; strcpy(g_pv_driver, "virtio"); strcpy(g_hv_name, "KVM");
    } else if (strncmp(vendor, "Microsoft Hv", 12) == 0) {
        g_hyper = HYPERV_HYPERV; strcpy(g_pv_driver, "hv_vmbus"); strcpy(g_hv_name, "Hyper-V");
    } else if (strncmp(vendor, "VMwareVMware", 12) == 0) {
        g_hyper = HYPERV_VMWARE; strcpy(g_pv_driver, "vmxnet3"); strcpy(g_hv_name, "VMware");
    } else if (strncmp(vendor, "XenVMMXenVMM", 12) == 0) {
        g_hyper = HYPERV_XEN; strcpy(g_pv_driver, "xen-blkfront"); strcpy(g_hv_name, "Xen");
    } else if (strncmp(vendor, "VBoxVBoxVBox", 12) == 0) {
        g_hyper = HYPERV_VBOX; strcpy(g_pv_driver, "vboxguest"); strcpy(g_hv_name, "VirtualBox");
    } else if (strncmp(vendor, "prl hyperv", 11) == 0) {
        g_hyper = HYPERV_PARALLELS; strcpy(g_pv_driver, "prl_fs"); strcpy(g_hv_name, "Parallels");
    } else {
        g_hyper = HYPERV_NONE;
    }

    /* virtio devices present (QEMU/KVM). */
    g_virtio = (access("/sys/bus/virtio", R_OK) == 0);
#endif
}

/* ---- W2: accessors ---- */
int  wubu_virt_hypervisor(void)     { return g_hyper; }
int  wubu_virt_has_virtio(void)     { return g_virtio; }
const char *wubu_virt_pv_driver(void){ return g_pv_driver[0] ? g_pv_driver : NULL; }
const char *wubu_virt_hypervisor_name(void) { return g_hv_name[0] ? g_hv_name : NULL; }

/* ---- W3: PV driver set per hypervisor ---- */
const char *wubu_virt_driver_set(int hyper)
{
    switch (hyper) {
    case HYPERV_KVM:      return "virtio_blk,virtio_net,virtio_gpu,virtio_input";
    case HYPERV_HYPERV:   return "hv_vmbus,hv_netvsc,storvsc,hv_utils,hv_balloon";
    case HYPERV_VMWARE:   return "vmxnet3,vmw_balloon,vmwgfx,mpt3sas";
    case HYPERV_XEN:      return "xen-blkfront,xen-netfront,xen-gntdev,xen-privcmd";
    case HYPERV_VBOX:     return "vboxguest,vboxsf,vboxvideo";
    case HYPERV_PARALLELS:return "prl_fs,prl_net,prl_balloon";
    default:              return NULL;
    }
}

/* ---- W4: summary ---- */
int wubu_virt_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "virt[hyper=%s pv=%s virtio=%d]",
        wubu_virt_hypervisor_name() ? wubu_virt_hypervisor_name() : "bare-metal",
        wubu_virt_pv_driver() ? wubu_virt_pv_driver() : "-",
        g_virtio);
}
