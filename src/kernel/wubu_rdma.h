/*
 * wubu_rdma.h -- kernel-owned RDMA/InfiniBand driver routing.
 */
#ifndef WUBU_RDMA_H
#define WUBU_RDMA_H

#include <stddef.h>

/* W1: probe the RDMA topology. */
void wubu_rdma_probe(void);

/* W2: accessors */
int  wubu_rdma_present(void);
int  wubu_rdma_ib(void);
int  wubu_rdma_roce(void);
int  wubu_rdma_iwarp(void);
int  wubu_rdma_soft_roce(void);
int  wubu_rdma_ports(void);
const char *wubu_rdma_driver(void);

/* W3: RDMA driver routing. */
const char *wubu_rdma_driver_for(const char *nic);

/* W4: summary fragment. */
int wubu_rdma_summary(char *out, size_t cap);

#endif /* WUBU_RDMA_H */
