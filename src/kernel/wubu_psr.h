/*
 * wubu_psr.h -- kernel-owned display PSR + NIC SR-IOV routing.
 */
#ifndef WUBU_PSR_H
#define WUBU_PSR_H

#include <stddef.h>

/* W1: probe the PSR/SR-IOV topology. */
void wubu_psr_probe(void);

/* W2: accessors */
int  wubu_psr_supported(void);
int  wubu_psr_sriov(void);
int  wubu_psr_vf(void);
int  wubu_psr_num_vfs(void);
const char *wubu_psr_driver(void);

/* W3: routing. */
const char *wubu_psr_driver_for(const char *gpu);
const char *wubu_psr_sriov_for(const char *nic);

/* W4: summary fragment. */
int wubu_psr_summary(char *out, size_t cap);

#endif /* WUBU_PSR_H */
