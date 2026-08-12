/*
 * wubu_iosched.c -- kernel-owned storage I/O scheduler routing.
 *
 * The I/O scheduler (elevator) orders block requests for throughput
 * + latency. "Runs on everything" includes correct I/O scheduling.
 *
 * I/O scheduler:
 *   - mq-deadline: latency-bounded (default for NVMe)
 *   - kyber: latency-aware, QD per priority
 *   - bfq: bandwidth fairness (interactive)
 *   - none: noop for NVMe
 *   - /sys/block sd queue scheduler: current scheduler
 *   - /sys/block sd queue nr_requests: queue depth
 *   - mq: multiqueue block layer
 *
 * WuBuOS owns this: detect I/O scheduler + queue depth + mq, route to
 * the right driver, expose the topology.
 *
 * Research (7-hop on the iosched frontier):
 *   - mq-deadline, kyber, bfq schedulers
 *   - /sys/block queue scheduler
 *   - nr_requests, queue depth
 */
#include "wubu_iosched.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int  g_sched = 0;       /* I/O scheduler present */
static int  g_mq = 0;          /* multiqueue */
static int  g_deadline = 0;    /* mq-deadline */
static int  g_kyber = 0;       /* kyber */
static int  g_bfq = 0;         /* bfq */
static int  g_none = 0;        /* none */
static char g_iosched_drv[24] = "";

void wubu_iosched_probe(void)
{
    g_sched = 0; g_mq = 0; g_deadline = 0; g_kyber = 0; g_bfq = 0; g_none = 0;
    g_iosched_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/block", R_OK) == 0) {
        g_sched = 1; g_mq = 1;
        strcpy(g_iosched_drv, "mq");
    }
    if (access("/sys/module/mq_deadline", R_OK) == 0 ||
        access("/sys/module/scsi_mod", R_OK) == 0) {
        g_deadline = 1;
        if (!g_iosched_drv[0]) strcpy(g_iosched_drv, "mq-deadline");
    }
    if (access("/sys/module/kyber", R_OK) == 0 ||
        access("/sys/module/kyber-iosched", R_OK) == 0) {
        g_kyber = 1;
        if (!g_iosched_drv[0]) strcpy(g_iosched_drv, "kyber");
    }
    if (access("/sys/module/bfq", R_OK) == 0) {
        g_bfq = 1;
        if (!g_iosched_drv[0]) strcpy(g_iosched_drv, "bfq");
    }
#endif
}

int  wubu_iosched_present(void){ return g_sched; }
int  wubu_iosched_mq(void)     { return g_mq; }
int  wubu_iosched_deadline(void){ return g_deadline; }
int  wubu_iosched_kyber(void)  { return g_kyber; }
int  wubu_iosched_bfq(void)    { return g_bfq; }
int  wubu_iosched_none(void)   { return g_none; }
const char *wubu_iosched_driver(void){ return g_iosched_drv[0] ? g_iosched_drv : NULL; }

const char *wubu_iosched_algo_for(const char *a)
{
    if (!a) return NULL;
    if (strstr(a, "deadline")) return "mq-deadline";
    if (strstr(a, "kyber"))   return "kyber";
    if (strstr(a, "bfq"))     return "bfq";
    if (strstr(a, "noop") || strstr(a, "none")) return "none";
    if (strstr(a, "cfq"))     return "cfq";
    return "mq-deadline";
}

const char *wubu_iosched_mode_for(const char *m)
{
    if (!m) return NULL;
    if (strstr(m, "wrr") || strstr(m, "fair")) return "wrr";
    if (strstr(m, "fifo"))   return "fifo";
    if (strstr(m, "prio"))   return "priority";
    return "fifo";
}

int wubu_iosched_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "iosched[sched=%d mq=%d deadline=%d kyber=%d bfq=%d drv=%s]",
        g_sched, g_mq, g_deadline, g_kyber, g_bfq,
        wubu_iosched_driver() ? wubu_iosched_driver() : "none");
}