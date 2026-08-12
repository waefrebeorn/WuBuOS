/*
 * wubu_nfsclient.c -- kernel-owned storage NFS client routing.
 *
 * NFS client mounts remote filesystems via RPC. "Runs on everything"
 * includes correct NFS on every storage system.
 *
 * NFS client:
 *   - /proc/mounts: NFS mount entries
 *   - rpc.idmapd: NFSv4 identity mapping
 *   - rpc.statd: lock manager
 *   - /proc/self/mountstats: NFS stats
 *   - mount: vers, proto, sec
 *   - /etc/exports: server export config
 *   - /var/lib/nfs: statd state
 *
 * WuBuOS owns this: detect NFS client + rpc + mount, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the nfsclient frontier):
 *   -NFS client vers rpc.idmapd
 */
#include "wubu_nfsclient.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_nfs = 0;         /* NFS client present */
static int  g_idmapd = 0;      /* rpc.idmapd */
static int  g_statd = 0;       /* rpc.statd */
static int  g_mount = 0;       /* mount */
static int  g_sec = 0;         /* security */
static char g_nfs_drv[24] = "";

void wubu_nfsclient_probe(void)
{
    g_nfs = 0; g_idmapd = 0; g_statd = 0; g_mount = 0; g_sec = 0;
    g_nfs_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/proc/mounts", R_OK) == 0) {
        FILE *f = fopen("/proc/mounts", "r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "nfs") || strstr(line, "nfs4")) {
                    g_nfs = 1; g_mount = 1; g_sec = 1;
                    strcpy(g_nfs_drv, "nfs-client");
                }
            }
            fclose(f);
        }
    }
    if (access("/var/lib/nfs", R_OK) == 0) {
        g_nfs = 1; g_idmapd = 1; g_statd = 1;
        if (!g_nfs_drv[0]) strcpy(g_nfs_drv, "rpc-client");
    }
#endif
}

int  wubu_nfsclient_present(void){ return g_nfs; }
int  wubu_nfsclient_idmapd(void){ return g_idmapd; }
int  wubu_nfsclient_statd(void)  { return g_statd; }
int  wubu_nfsclient_mount(void)  { return g_mount; }
int  wubu_nfsclient_sec(void)    { return g_sec; }
const char *wubu_nfsclient_driver(void){ return g_nfs_drv[0] ? g_nfs_drv : NULL; }

const char *wubu_nfsclient_vers_for(const char *v)
{
    if (!v) return NULL;
    if (strstr(v, "4.2")) return "4.2";
    if (strstr(v, "4.1")) return "4.1";
    if (strstr(v, "4.0") || strstr(v, "4")) return "4.0";
    if (strstr(v, "3")) return "3";
    if (strstr(v, "2")) return "2";
    return "4.0";
}

const char *wubu_nfsclient_proto_for(const char *p)
{
    if (!p) return NULL;
    if (strstr(p, "tcp")) return "tcp";
    if (strstr(p, "udp")) return "udp";
    if (strstr(p, "rdma")) return "rdma";
    return "tcp";
}

int wubu_nfsclient_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "nfsclient[nfs=%d idmapd=%d statd=%d mount=%d sec=%d drv=%s]",
        g_nfs, g_idmapd, g_statd, g_mount, g_sec,
        wubu_nfsclient_driver() ? wubu_nfsclient_driver() : "none");
}
