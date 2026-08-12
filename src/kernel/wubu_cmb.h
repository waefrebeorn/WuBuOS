/*
 * wubu_cmb.h -- kernel-owned NVMe Controller Memory Buffer routing.
 */
#ifndef WUBU_CMB_H
#define WUBU_CMB_H

#include <stddef.h>

/* W1: probe the CMB topology. */
void wubu_cmb_probe(void);

/* W2: accessors */
int  wubu_cmb_present(void);
int  wubu_cmb_nvme(void);
int  wubu_cmb_qmem(void);
int  wubu_cmb_pmicm(void);
int  wubu_cmb_squeue(void);
const char *wubu_cmb_driver(void);

/* W3: CMB routing. */
const char *wubu_cmb_reg_for(const char *reg);
const char *wubu_cmb_queue_for(const char *q);

/* W4: summary fragment. */
int wubu_cmb_summary(char *out, size_t cap);

#endif /* WUBU_CMB_H */
