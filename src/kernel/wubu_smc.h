/*
 * wubu_smc.h -- kernel-owned GPU SMC firmware routing.
 */
#ifndef WUBU_SMC_H
#define WUBU_SMC_H

#include <stddef.h>

void wubu_smc_probe(void);
int  wubu_smc_present(void);
int  wubu_smc_smu(void);
int  wubu_smc_vcn(void);
int  wubu_smc_uvd(void);
int  wubu_smc_fw(void);
const char *wubu_smc_driver(void);
const char *wubu_smc_block_for(const char *b);
const char *wubu_smc_state_for(const char *s);
int wubu_smc_summary(char *out, size_t cap);

#endif
