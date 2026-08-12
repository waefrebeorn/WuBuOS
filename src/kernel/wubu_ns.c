/*
 * wubu_ns.c -- kernel-owned NVMe namespace/multipath routing.
 *
 * NVMe namespaces are logical volumes within an NVMe controller. Multipath
 * provides failover across multiple controllers/paths. "Runs on
 * everything" includes correct NVMe namespace + multipath handling.
 *
 * NVMe namespace:
 *   - nvme-cli: nvme list-ns, id-ns (nsid)
 *   - /dev/nvme0n1: namespace block devices
 *   - /sys/class/nvme/nvme0/namespaces/nvme0n1: ns state
 *   - namespace formatting (nvme format)
 *   - multipath: nvme-multipath (nvme0n1 -> nvme0c0n1 dm)
 *   - ana (asymmetric namespace access): primary/secondary paths
 *
 * WuBuOS owns this: detect NVMe namespaces + multipath + ANA, route to the
 * right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the NVMe-namespace frontier):
 *   - nvme-cli: list-ns, id-ns (nsid)
 *   - /sys/class/nvme namespaces
 *   - nvme-multipath: dm-multipath failover
 *   - ANA: asymmetric namespace access (primary/secondary)
 */
#include "wubu_ns.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_nvme = 0;        /* NVMe present */
static int  g_ns = 0;          /* namespace */
static int  g_multipath = 0;   /* multipath */
static int  g_ana = 0;         /* ANA */
static int  g_cli = 0;         /* nvme-cli */
static char g_ns_drv[24] = "";

/* ---- W1: probe the namespace topology ---- */
void wubu_ns_probe(void)
{
    g_nvme = 0; g_ns = 0; g_multipath = 0; g_ana = 0; g_cli = 0;
    g_ns_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* NVMe present? */
    if (access("/sys/class/nvme", R_OK) == 0) {
        g_nvme = 1;
        strcpy(g_ns_drv, "nvme");
    }
    /* Namespace present? */
    if (access("/sys/class/nvme", R_OK) == 0) {
        DIR *d = opendir("/sys/class/nvme");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == 'n') {
                    char p[96];
                    snprintf(p, sizeof(p), "/sys/class/nvme/%s/namespaces", e->d_name);
                    if (access(p, R_OK) == 0) { g_ns = 1; break; }
                }
            }
            closedir(d);
        }
    }
    /* multipath (dm)? */
    if (access("/sys/module/nvme_multipath", R_OK) == 0 ||
        access("/sys/module/dm_multipath", R_OK) == 0) {
        g_multipath = 1;
        if (!g_ns_drv[0]) strcpy(g_ns_drv, "nvme-multipath");
    }
    /* ANA? */
    if (access("/sys/module/nvme_multipath", R_OK) == 0) {
        g_ana = 1;
    }
    /* nvme-cli? */
    if (access("/usr/sbin/nvme", R_OK) == 0 ||
        access("/usr/bin/nvme", R_OK) == 0) {
        g_cli = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_ns_nvme(void)      { return g_nvme; }
int  wubu_ns_namespace(void) { return g_ns; }
int  wubu_ns_multipath(void) { return g_multipath; }
int  wubu_ns_ana(void)       { return g_ana; }
int  wubu_ns_cli(void)       { return g_cli; }
const char *wubu_ns_driver(void){ return g_ns_drv[0] ? g_ns_drv : NULL; }

/* ---- W3: namespace routing ---- */
const char *wubu_ns_path_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "primary") || strstr(mode, "optimized")) return "primary";
    if (strstr(mode, "secondary") || strstr(mode, "non-opt")) return "secondary";
    if (strstr(mode, "npath") || strstr(mode, "multipath")) return "multipath";
    return "ns";
}

const char *wubu_ns_state_for(const char *state)
{
    if (!state) return NULL;
    if (strstr(state, "live"))  return "live";
    if (strstr(state, "offline")) return "offline";
    if (strstr(state, "read-only")) return "read-only";
    return "live";
}

/* ---- W4: summary ---- */
int wubu_ns_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "ns[nvme=%d ns=%d mpath=%d ana=%d cli=%d drv=%s]",
        g_nvme, g_ns, g_multipath, g_ana, g_cli,
        wubu_ns_driver() ? wubu_ns_driver() : "none");
}
