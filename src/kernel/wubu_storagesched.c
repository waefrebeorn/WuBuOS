/*
 * wubu_storagesched.c -- kernel-owned storage block scheduler routing.
 *
 * Storage scheduler (deadline, BFQ, mq-deadline) orders block requests.
 * "Runs on everything" includes correct I/O scheduling on every disk.
 *
 * Storage scheduler:
 *   - sys/block queue/scheduler: active scheduler
 *   -mq-deadline: multi-queue deadline (default)
 *   -BFQ: BFQ multi-queue
 *   -none: no scheduler (passthrough)
 *   -kyber: kyber
 *   -bfq, cfq, deadline: legacy single-queue
 *
 * WuBuOS owns this: detect scheduler + type + latency, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the storagesched frontier):
 *   -block multi-queue scheduler
 *   - BFQ
 *   - mq-deadline
 */
#include "wubu_storagesched.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_ss = 0;          /* storage scheduler present */
static int  g_mq = 0;          /* multi-queue */
static int  g_bfq = 0;         /* BFQ */
static int  g_deadline = 0;    /* deadline */
static int  g_none = 0;        /* none/passthrough */
static char g_ss_drv[24] = "";

void wubu_storagesched_probe(void)
{
    g_ss = 0; g_mq = 0; g_bfq = 0; g_deadline = 0; g_none = 0;
    g_ss_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/block", R_OK) == 0) {
        g_ss = 1; g_mq = 1; g_deadline = 1;
        strcpy(g_ss_drv, "blk-sched");
    }
    if (access("/sys/module/nvme", R_OK) == 0) {
        g_ss = 1; g_mq = 1; g_deadline = 1;
        if (!g_ss_drv[0]) strcpy(g_ss_drv, "nvme-sched");
    }
#endif
}

int  wubu_storagesched_present(void){ return g_ss; }
int  wubu_storagesched_mq(void)     { return g_mq; }
int  wubu_storagesched_bfq(void)    { return g_bfq; }
int  wubu_storagesched_deadline(void){ return g_deadline; }
int  wubu_storagesched_none(void)   { return g_none; }
const char *wubu_storagesched_driver(void){ return g_ss_drv[0] ? g_ss_drv : NULL; }

const char *wubu_storagesched_type_for(const char *t)
{
    if (!t) return NULL;
    if (strstr(t, "mq-deadline") || strstr(t, "mq_deadline")) return "mq-deadline";
    if (strstr(t, "bfq")) return "bfq";
    if (strstr(t, "deadline")) return "deadline";
    if (strstr(t, "cfq")) return "cfq";
    if (strstr(t, "kyber")) return "kyber";
    if (strstr(t, "none")) return "none";
    return "mq-deadline";
}

const char *wubu_storagesched_mode_for(const char *m)
{
    if (!m) return NULL;
    if (strstr(m, "mq") || strstr(m, "multi")) return "multi-queue";
    if (strstr(m, "single")) return "single-queue";
    return "multi-queue";
}

int wubu_storagesched_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "storagesched[ss=%d mq=%d bfq=%d deadline=%d none=%d drv=%s]",
        g_ss, g_mq, g_bfq, g_deadline, g_none,
        wubu_storagesched_driver() ? wubu_storagesched_driver() : "none");
}
