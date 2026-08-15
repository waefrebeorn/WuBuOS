/*
 *"wubu_nfsmount.c -- kernel-owned storage NFS mount routing.
 *
 * NFS mount routes remote filesystems. "Runs on everything"
 * includes correct NFS on every storage system.
 *
 * NFS:
 *   - /proc/mounts: NFS mount entries
 *   - mount options: vers, rsize, wsize, timeo, intr
 *   - /proc/fs/nfsd: NFS server
 *   - /sys/class/net: network interface
 *   - fsid: NFS file handle
 *   - mountd: portmapper, rpc.mountd
 *
 * WuBuOS owns this: detect NFS + mount + options, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the nfsmount frontier):
 *   -NFS common mount options
 */
#include "wubu_nfsmount.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_nfs = 0;         /* NFS present */
static int  g_mount = 0;       /* mount */
static int  g_vers = 0;        /* version */
static int  g_rsize = 0;       /* read size */
static int  g_wsize = 0;       /* write size */
static char g_nfs_drv[24] = "";

void wubu_nfsmount_probe(void)
{
    g_nfs = 0; g_mount = 0; g_vers = 0; g_rsize = 0; g_wsize = 0;
    g_nfs_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/proc/mounts", R_OK) == 0) {
        FILE *f = fopen("/proc/mounts", "r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "nfs") || strstr(line, "nfs4")) {
                    g_nfs = 1; g_mount = 1; g_vers = 1;
                    g_rsize = 1; g_wsize = 1;
                    strcpy(g_nfs_drv, "nfs-mount");
                }
            }
            fclose(f);
        }
    }
    if (access("/proc/fs/nfsd", R_OK) == 0 && !g_nfs_drv[0]) {
        g_nfs = 1; g_mount = 1;
        strcpy(g_nfs_drv, "nfs-server");
    }
#endif
}

int  wubu_nfsmount_present(void){ return g_nfs; }
int  wubu_nfsmount_mount(void)  { return g_mount; }
int  wubu_nfsmount_vers(void)   { return g_vers; }
int  wubu_nfsmount_rsize(void)  { return g_rsize; }
int  wubu_nfsmount_wsize(void)  { return g_wsize; }
const char *wubu_nfsmount_driver(void){ return g_nfs_drv[0] ? g_nfs_drv : NULL; }

const char *wubu_nfsmount_vers_for(const char *v)
{
    if (!v) return NULL;
    if (strstr(v, "4.2")) return "4.2";
    if (strstr(v, "4.1") || strstr(v, "4,1")) return "4.1";
    if (strstr(v, "4.0") || strstr(v, "4,0") || strstr(v, "4")) return "4.0";
    if (strstr(v, "3")) return "3";
    if (strstr(v, "2")) return "2";
    return "4.0";
}

const char *wubu_nfsmount_opt_for(const char *o)
{
    if (!o) return NULL;
    if (strstr(o, "rsize")) return "rsize";
    if (strstr(o, "wsize")) return "wsize";
    if (strstr(o, "timeo")) return "timeo";
    if (strstr(o, "intr"))  return "intr";
    if (strstr(o, "hard"))  return "hard";
    if (strstr(o, "soft"))  return "soft";
    if (strstr(o, "ac"))    return "attr-cache";
    return "rsize";
}

int wubu_nfsmount_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "nfsmount[nfs=%d mount=%d vers=%d rsize=%d wsize=%d drv=%s]",
        g_nfs, g_mount, g_vers, g_rsize, g_wsize,
        wubu_nfsmount_driver() ? wubu_nfsmount_driver() : "none");
}
