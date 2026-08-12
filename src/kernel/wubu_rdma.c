/*
 * wubu_rdma.c -- kernel-owned RDMA/InfiniBand driver routing.
 *
 * RDMA (remote direct memory access) bypasses the CPU for data transfer:
 * InfiniBand, RoCE, iWARP. "Runs on everything" includes high-performance
 * compute fabrics. The kernel must route the RDMA NIC to the right driver
 * and expose the verbs/ports topology.
 *
 * RDMA drivers (by vendor):
 *   - Mellanox/NVIDIA: mlx5_ib (ConnectX-4/5/6/7), mlx4_ib
 *   - Intel: irdma (iWARP, X722/X710), qedr (QLogic), i40iw (older)
 *   - Broadcom: bnxt_re (RoCE)
 *   - Cavium: qedr (RoCE, QLogic)
 *   - Software: rxe (soft-RoCE), siw (soft iWARP)
 *   - InfiniBand: ib_core, ib_verbs, ib_cm, ib_umad, infiniband.ko
 *
 * WuBuOS owns this: detect the RDMA NIC (verbs/ports), route to the right
 * driver, and expose the RDMA topology.
 *
 * Research (Kevin-Bacon 7-hop on the RDMA frontier):
 *   - ib_core, ib_verbs: InfiniBand/RDMA core (infiniband.ko)
 *   - mlx5_ib: Mellanox/NVIDIA ConnectX (the HPC standard)
 *   - irdma: Intel iWARP; bnxt_re: Broadcom RoCE
 *   - rxe: soft-RoCE (software RDMA over ethernet)
 *   - /sys/class/infiniband: ibdevs, ports, active_speed
 */
#include "wubu_rdma.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_rdma = 0;
static int  g_ib = 0;           /* InfiniBand */
static int  g_roce = 0;         /* RoCE */
static int  g_iwarp = 0;        /* iWARP */
static int  g_soft_roce = 0;    /* rxe */
static int  g_ports = 0;
static char g_rdma_drv[24] = "";

/* ---- W1: probe the RDMA topology ---- */
void wubu_rdma_probe(void)
{
    g_rdma = 0; g_ib = 0; g_roce = 0; g_iwarp = 0; g_soft_roce = 0;
    g_ports = 0; g_rdma_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* InfiniBand/RDMA class present? */
    if (access("/sys/class/infiniband", R_OK) == 0 ||
        access("/dev/infiniband", R_OK) == 0) {
        g_rdma = 1;
        /* count rdma devices + ports */
        struct dirent **e;
        int n = scandir("/sys/class/infiniband", &e, NULL, alphasort);
        for (int i = 0; i < n; i++) {
            if (e[i]->d_name[0] == '.') continue;
            /* count ports */
            char p[128];
            for (int pt = 1; pt <= 8; pt++) {
                snprintf(p, sizeof(p), "/sys/class/infiniband/%s/ports/%d",
                         e[i]->d_name, pt);
                if (access(p, R_OK) == 0) g_ports++;
                else break;
            }
        }
        /* driver detection */
        if (access("/sys/bus/pci/drivers/mlx5_core", R_OK) == 0) {
            g_ib = 1;
            strcpy(g_rdma_drv, "mlx5_ib");
        } else if (access("/sys/bus/pci/drivers/irdma", R_OK) == 0 ||
                   access("/sys/bus/pci/drivers/i40iw", R_OK) == 0) {
            g_iwarp = 1;
            strcpy(g_rdma_drv, "irdma");
        } else if (access("/sys/bus/pci/drivers/bnxt_re", R_OK) == 0) {
            g_roce = 1;
            strcpy(g_rdma_drv, "bnxt_re");
        } else {
            g_roce = 1;
            strcpy(g_rdma_drv, "rdma-core");
        }
    }
    /* Soft-RoCE (rxe) present? */
    if (access("/sys/module/rdma_rxe", R_OK) == 0) {
        g_soft_roce = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_rdma_present(void)   { return g_rdma; }
int  wubu_rdma_ib(void)        { return g_ib; }
int  wubu_rdma_roce(void)      { return g_roce; }
int  wubu_rdma_iwarp(void)     { return g_iwarp; }
int  wubu_rdma_soft_roce(void) { return g_soft_roce; }
int  wubu_rdma_ports(void)     { return g_ports; }
const char *wubu_rdma_driver(void){ return g_rdma_drv[0] ? g_rdma_drv : NULL; }

/* ---- W3: RDMA driver routing ---- */
const char *wubu_rdma_driver_for(const char *nic)
{
    if (!nic) return NULL;
    if (strstr(nic, "mlx5") || strstr(nic, "connectx") || strstr(nic, "cx-")) return "mlx5_ib";
    if (strstr(nic, "irdma") || strstr(nic, "i40iw") || strstr(nic, "x722")) return "irdma";
    if (strstr(nic, "bnxt"))  return "bnxt_re";
    if (strstr(nic, "qedr") || strstr(nic, "qlogic")) return "qedr";
    if (strstr(nic, "rxe") || strstr(nic, "soft-roce")) return "rdma_rxe";
    if (strstr(nic, "siw") || strstr(nic, "soft-iwarp")) return "siw";
    return "rdma-core";
}

/* ---- W4: summary ---- */
int wubu_rdma_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "rdma[rdma=%d ib=%d roce=%d iwarp=%d soft_roce=%d ports=%d drv=%s]",
        g_rdma, g_ib, g_roce, g_iwarp, g_soft_roce, g_ports,
        wubu_rdma_driver() ? wubu_rdma_driver() : "none");
}
