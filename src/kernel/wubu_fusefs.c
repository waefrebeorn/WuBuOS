/*
 * wubu_fusefs.c -- kernel-owned storage FUSE filesystem routing.
 *
 * FUSE (Filesystem in Userspace) runs filesystem code in userspace.
 * "Runs on everything" includes correct FUSE on every storage system.
 *
 * FUSE:
 *   - /dev/fuse: FUSE character device
 *   - /sys/class/fuse: FUSE control
 *   - mount: fuse, fuse3, fusectl
 *   - /proc/mounts: FUSE mount entries
 *   - ops: getattr, readdir, open, read, write
 *   - /sys/fs/fuse/connections: FUSE connections
 *
 * WuBuOS owns this: detect FUSE + mount + ops, route to the
 *  right driver, expose the topology.
 *
 * Research (7-hop on the fusefs frontier):
 *   -FUSE filesystem userspace block implementation
 */
#include "wubu_fusefs.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_fuse = 0;       /* FUSE present */
static int  g_dev = 0;        /* /dev/fuse */
static int  g_mount = 0;      /* mount */
static int  g_ctl = 0;        /* fusectl */
static int  g_conn = 0;       /* connections */
static char g_fuse_drv[24] = "";

void wubu_fusefs_probe(void)
{
    g_fuse = 0; g_dev = 0; g_mount = 0; g_ctl = 0; g_conn = 0;
    g_fuse_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/dev/fuse", R_OK) == 0 ||
        access("/proc/mounts", R_OK) == 0) {
        FILE *f = fopen("/proc/mounts", "r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "fuse")) {
                    g_fuse = 1; g_mount = 1; g_ctl = 1; g_conn = 1;
                    strcpy(g_fuse_drv, "fuse-mount");
                }
            }
            fclose(f);
        }
    }
    if (access("/dev/fuse", R_OK) == 0) {
        g_fuse = 1; g_dev = 1;
        if (!g_fuse_drv[0]) strcpy(g_fuse_drv, "fuse-dev");
    }
    if (access("/sys/fs/fuse/connections", R_OK) == 0) {
        g_fuse = 1; g_conn = 1;
        if (!g_fuse_drv[0]) strcpy(g_fuse_drv, "fuse-conn");
    }
#endif
}

int  wubu_fusefs_present(void){ return g_fuse; }
int  wubu_fusefs_dev(void)    { return g_dev; }
int  wubu_fusefs_mount(void)  { return g_mount; }
int  wubu_fusefs_ctl(void)    { return g_ctl; }
int  wubu_fusefs_conn(void)   { return g_conn; }
const char *wubu_fusefs_driver(void){ return g_fuse_drv[0] ? g_fuse_drv : NULL; }

const char *wubu_fusefs_impl_for(const char *i)
{
    if (!i) return NULL;
    if (strstr(i, "ssh")) return "sshfs";
    if (strstr(i, "ntfs")) return "ntfs-3g";
    if (strstr(i, "mp3")) return "mp3fs";
    if (strstr(i, "iso")) return "fuseiso";
    if (strstr(i, "enc")) return "encfs";
    if (strstr(i, "bind")) return "bindfs";
    return "sshfs";
}

const char *wubu_fusefs_op_for(const char *o)
{
    if (!o) return NULL;
    if (strstr(o, "getattr")) return "getattr";
    if (strstr(o, "readdir")) return "readdir";
    if (strstr(o, "open")) return "open";
    if (strstr(o, "read")) return "read";
    if (strstr(o, "write")) return "write";
    if (strstr(o, "unlink")) return "unlink";
    if (strstr(o, "release")) return "release";
    return "getattr";
}

int wubu_fusefs_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "fusefs[fuse=%d dev=%d mount=%d ctl=%d conn=%d drv=%s]",
        g_fuse, g_dev, g_mount, g_ctl, g_conn,
        wubu_fusefs_driver() ? wubu_fusefs_driver() : "none");
}
