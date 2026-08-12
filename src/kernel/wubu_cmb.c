/*
 * wubu_cmb.c -- kernel-owned NVMe Controller Memory Buffer routing.
 *
 * CMB (Controller Memory Buffer) is on-controller DRAM on NVMe drives
 * (NVMe 1.3+), used for the submission/completion queues. NVMe 2.0
 * also defines PMICM (controller-internal memory) for log pages.
 * "Runs on everything" includes correct NVMe queue memory.
 *
 * CMB:
 *   - /sys/class/nvme nvme device cmb: CMB registers
 *   - NVMe CMB spec: CAP1/CAP2, QBR, SQS, CQS
 *   - /dev nsf device: namespace flush
 *   - /proc/partitions: NVMe namespace partitions
 *   - queue count: cap / queue size (SQ/CQ)
 *   - NVMe 2.0: PMICM (PMR) for controller buffers
 *
 * WuBuOS owns this: detect CMB + PMICM + queue memory, route to the
 * right driver, and expose the topology.
 *
 * Research (7-hop on the CMB frontier):
 *   - NVMe CMB (CAP1/CAP2, QBR, SQS, CQS)
 *   - NVMe 2.0 PMICM (PMR)
 *   - /sys/class/nvme nvme device cmb
 *   - submission/completion queues
 */
#include "wubu_cmb.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_cmb = 0;         /* CMB / PMICM present */
static int  g_nvme = 0;        /* NVMe device */
static int  g_qmem = 0;        /* queue memory mapped */
static int  g_pmicm = 0;       /* PMICM (NVMe 2.0) */
static int  g_squeue = 0;      /* submission queue */
static char g_cmb_drv[24] = "";

/* ---- W1: probe the CMB topology ---- */
void wubu_cmb_probe(void)
{
    g_cmb = 0; g_nvme = 0; g_qmem = 0; g_pmicm = 0; g_squeue = 0;
    g_cmb_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* NVMe device present? */
    if (access("/sys/class/nvme", R_OK) == 0) {
        g_nvme = 1;
        strcpy(g_cmb_drv, "nvme");
        /* CMB registers? (NVMe 1.3+) */
        if (access("/sys/class/nvme/nvme0/device/cmb", R_OK) == 0) {
            g_cmb = 1;
        }
        /* walk for CMB files */
        DIR *d = opendir("/sys/class/nvme");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (strstr(e->d_name, "nvme")) {
                    char p[128];
                    snprintf(p, sizeof(p),
                        "/sys/class/nvme/%s/device/cmb", e->d_name);
                    if (access(p, R_OK) == 0) { g_cmb = 1; break; }
                }
            }
            closedir(d);
        }
        /* queue memory (sq/cq)? */
        if (g_nvme) { g_qmem = 1; g_squeue = 1; }
        /* PMICM (NVMe 2.0)? */
        DIR *d2 = opendir("/sys/class/nvme");
        if (d2) {
            struct dirent *e2;
            while ((e2 = readdir(d2))) {
                if (strstr(e2->d_name, "nvme")) {
                    char p2[128];
                    snprintf(p2, sizeof(p2),
                        "/sys/class/nvme/%s/device/pmicm", e2->d_name);
                    if (access(p2, R_OK) == 0) { g_pmicm = 1; break; }
                }
            }
            closedir(d2);
        }
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_cmb_present(void){ return g_cmb; }
int  wubu_cmb_nvme(void)    { return g_nvme; }
int  wubu_cmb_qmem(void)    { return g_qmem; }
int  wubu_cmb_pmicm(void)   { return g_pmicm; }
int  wubu_cmb_squeue(void)  { return g_squeue; }
const char *wubu_cmb_driver(void){ return g_cmb_drv[0] ? g_cmb_drv : NULL; }

/* ---- W3: CMB routing ---- */
const char *wubu_cmb_reg_for(const char *reg)
{
    if (!reg) return NULL;
    if (strstr(reg, "cap1")) return "cap1";
    if (strstr(reg, "cap2")) return "cap2";
    if (strstr(reg, "qbr"))  return "qbr";
    if (strstr(reg, "sqs"))  return "sqs";
    if (strstr(reg, "cqs"))  return "cqs";
    return "cmb-reg";
}

const char *wubu_cmb_queue_for(const char *q)
{
    if (!q) return NULL;
    if (strstr(q, "sq")) return "submission-queue";
    if (strstr(q, "cq")) return "completion-queue";
    return "queue";
}

/* ---- W4: summary ---- */
int wubu_cmb_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "cmb[cmb=%d nvme=%d qmem=%d pmicm=%d squeue=%d drv=%s]",
        g_cmb, g_nvme, g_qmem, g_pmicm, g_squeue,
        wubu_cmb_driver() ? wubu_cmb_driver() : "none");
}