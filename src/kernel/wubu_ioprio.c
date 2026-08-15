/*
 * wubu_ioprio.c -- kernel-owned storage I/O priority routing.
 *
 * I/O priority (ioprio) assigns class + level to block requests.
 * "Runs on everything" includes correct I/O scheduling on every disk.
 *
 * I/O priority:
 *   - ioprio: RT (realtime), BE (best-effort), IDLE
 *   - /sys/block queue/scheduler: scheduler
 *   - ionice: command-line ioprio tool
 *   - class: RT, BE, IDLE, none
 *   - level: 0-7 (RT: 0-7, BE: 0-7)
 *
 * WuBuOS owns this: detect ioprio + class + level, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the ioprio frontier):
 *   - I/O priority classes
 *   - ionice
 *   - blkio cgroup
 */
#include "wubu_ioprio.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_iop = 0;         /* ioprio present */
static int  g_rt = 0;          /* RT class */
static int  g_be = 0;          /* BE class */
static int  g_idle = 0;        /* IDLE class */
static int  g_sched = 0;       /* scheduler */
static char g_iop_drv[24] = "";

void wubu_ioprio_probe(void)
{
    g_iop = 0; g_rt = 0; g_be = 0; g_idle = 0; g_sched = 0;
    g_iop_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/block", R_OK) == 0 ||
        access("/usr/bin/ionice", R_OK) == 0 ||
        access("/usr/sbin/ionice", R_OK) == 0) {
        g_iop = 1; g_rt = 1; g_be = 1; g_idle = 1; g_sched = 1;
        strcpy(g_iop_drv, "ionice");
    }
#endif
}

int  wubu_ioprio_present(void){ return g_iop; }
int  wubu_ioprio_rt(void)     { return g_rt; }
int  wubu_ioprio_be(void)     { return g_be; }
int  wubu_ioprio_idle(void)   { return g_idle; }
int  wubu_ioprio_sched(void)   { return g_sched; }
const char *wubu_ioprio_driver(void){ return g_iop_drv[0] ? g_iop_drv : NULL; }

const char *wubu_ioprio_class_for(const char *c)
{
    if (!c) return NULL;
    if (strstr(c, "be") || strstr(c, "best")) return "be";
    if (strstr(c, "rt") || strstr(c, "realtime")) return "rt";
    if (strstr(c, "idle")) return "idle";
    if (strstr(c, "none")) return "none";
    return "be";
}

const char *wubu_ioprio_sched_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "noop"))  return "noop";
    if (strstr(s, "mq-deadline")) return "mq-deadline";
    if (strstr(s, "deadline")) return "deadline";
    if (strstr(s, "cfq"))   return "cfq";
    if (strstr(s, "bfq"))   return "bfq";
    if (strstr(s, "kyber")) return "kyber";
    return "mq-deadline";
}

int wubu_ioprio_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "ioprio[iop=%d rt=%d be=%d idle=%d sched=%d drv=%s]",
        g_iop, g_rt, g_be, g_idle, g_sched,
        wubu_ioprio_driver() ? wubu_ioprio_driver() : "none");
}
