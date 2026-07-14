/*
 * wubu_ns_kernel.c -- WuBuOS Namespace Bridge: kernel + hw control plane
 * (rip off CachyOS kernel-manager / chwd, do it better through /n).
 *
 *   /n/kernel/scheduler  -- read = active CPU policy; write "bore"|"eevdf"|
 *                           "fair"|"rt" selects it. The policy is stored and
 *                           exposed so launchers (bottle run, gamescope) apply
 *                           equivalent userspace tunables (nice / sched class /
 *                           cpu affinity) -- the CachyOS sched-ext vibe without
 *                           a separate kernel-manager daemon.
 *   /n/hw/<gpu>/mode     -- chwd vibe: detect primary GPU, expose/switch its
 *                           passthrough mode. GPU detection is injectable (DI)
 *                           so the control plane is testable without pulling
 *                           the full proton2/host-exec graph into the test.
 *
 * Reuses g_ns_root/ns_mkdir/ns_write from wubu_ns_fs.c. C11, opaque, minimal.
 */

#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    NS_SCHED_BORE  = 0,   /* burst-oriented response enhancer (gamescope/proton) */
    NS_SCHED_EEVDF = 1,   /* default fair, latency-aware */
    NS_SCHED_FAIR  = 2,   /* plain SCHED_OTHER */
    NS_SCHED_RT    = 3,   /* SCHED_FIFO best-effort RT */
    NS_SCHED_COUNT
} NsSchedPolicy;

typedef int (*wubu_ns_gpu_detect_fn)(char *name, int name_len, char *pci, int pci_len);

/* Active scheduler policy; persisted to /n/kernel/scheduler. */
static NsSchedPolicy g_sched = NS_SCHED_EEVDF;

static const char *sched_name(NsSchedPolicy p) {
    switch (p) {
        case NS_SCHED_BORE:  return "bore";
        case NS_SCHED_EEVDF: return "eevdf";
        case NS_SCHED_FAIR:  return "fair";
        case NS_SCHED_RT:    return "rt";
        default:             return "unknown";
    }
}

static NsSchedPolicy sched_from_str(const char *s) {
    if (!s) return NS_SCHED_COUNT;
    if (!strcmp(s, "bore"))   return NS_SCHED_BORE;
    if (!strcmp(s, "eevdf"))  return NS_SCHED_EEVDF;
    if (!strcmp(s, "fair"))   return NS_SCHED_FAIR;
    if (!strcmp(s, "rt"))     return NS_SCHED_RT;
    return NS_SCHED_COUNT;
}

int wubu_ns_sched_set(const char *policy) {
    NsSchedPolicy p = sched_from_str(policy);
    if (p == NS_SCHED_COUNT) return -1;
    g_sched = p;
    if (!g_ns_root) return 0;
    char sub[4096];
    snprintf(sub, sizeof(sub), "kernel/scheduler");
    return ns_write(sub, sched_name(p));
}

const char *wubu_ns_sched_get(void) {
    return sched_name(g_sched);
}

/* -- Hardware (chwd vibe) -------------------------------------- */

/* Detect the primary GPU and publish /n/hw/<gpu>/mode. detect_fn may be NULL
 * (uses no detection; caller supplies a mock in tests). mode_out receives the
 * detected GPU name (caller buffer). Returns 0 on success, -1 on no detect. */
int wubu_ns_hw_detect(wubu_ns_gpu_detect_fn detect_fn, char *mode_out, size_t n) {
    if (!g_ns_root) return -1;
    char name[128], pci[128];
    if (!detect_fn || detect_fn(name, sizeof(name), pci, sizeof(pci)) != 0) {
        if (mode_out) { mode_out[0] = '\0'; }
        return -1;
    }
    if (mode_out) snprintf(mode_out, n, "%s", name);

    char dir[4096];
    snprintf(dir, sizeof(dir), "hw/%s", name);
    if (ns_mkdir(dir) != 0) return -1;

    char sub[4096];
    snprintf(sub, sizeof(sub), "hw/%s/mode", name);
    /* default passthrough mode; chwd-style auto-detect sets "passthrough" */
    if (ns_write(sub, "passthrough\n") != 0) return -1;
    snprintf(sub, sizeof(sub), "hw/%s/pci", name);
    if (ns_write(sub, pci) != 0) return -1;
    return 0;
}

/* Set the passthrough mode for a detected GPU (chwd-style switch). */
int wubu_ns_hw_set_mode(const char *gpu, const char *mode) {
    if (!g_ns_root || !gpu || !mode) return -1;
    char sub[4096];
    snprintf(sub, sizeof(sub), "hw/%s/mode", gpu);
    char buf[256];
    snprintf(buf, sizeof(buf), "%s\n", mode);
    return ns_write(sub, buf);
}

int wubu_ns_publish_kernel(wubu_ns_gpu_detect_fn detect_fn) {
    if (!g_ns_root) return -1;
    if (ns_mkdir("kernel") != 0) return -1;
    if (ns_mkdir("hw") != 0) return -1;

    /* scheduler ctl (write a policy; read = active) */
    char sub[4096];
    snprintf(sub, sizeof(sub), "kernel/scheduler");
    if (ns_write(sub, sched_name(g_sched)) != 0) return -1;

    /* hw: detect + publish primary GPU */
    char gpu[128];
    if (wubu_ns_hw_detect(detect_fn, gpu, sizeof(gpu)) == 0) {
        /* published; nothing else needed here */
    }
    return 0;
}
