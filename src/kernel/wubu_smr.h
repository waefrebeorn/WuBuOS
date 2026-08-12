/*
 * wubu_smr.h -- kernel-owned storage SMR routing.
 */
#ifndef WUBU_SMR_H
#define WUBU_SMR_H

#include <stddef.h>

void wubu_smr_probe(void);
int  wubu_smr_present(void);
int  wubu_smr_zone(void);
int  wubu_smr_zns(void);
int  wubu_smr_wp(void);
int  wubu_smr_zonefs(void);
const char *wubu_smr_driver(void);
const char *wubu_smr_zone_for(const char *z);
const char *wubu_smr_op_for(const char *op);
int wubu_smr_summary(char *out, size_t cap);

#endif
