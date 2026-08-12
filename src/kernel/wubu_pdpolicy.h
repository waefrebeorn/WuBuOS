/*
 * wubu_pdpolicy.h -- kernel-owned USB PD policy routing.
 */
#ifndef WUBU_PDPOLICY_H
#define WUBU_PDPOLICY_H

#include <stddef.h>

void wubu_pdpolicy_probe(void);
int  wubu_pdpolicy_present(void);
int  wubu_pdpolicy_contract(void);
int  wubu_pdpolicy_pdo(void);
int  wubu_pdpolicy_pps(void);
int  wubu_pdpolicy_dual(void);
const char *wubu_pdpolicy_driver(void);
const char *wubu_pdpolicy_role_for(const char *role);
const char *wubu_pdpolicy_pdo_for(const char *pdo);
int wubu_pdpolicy_summary(char *out, size_t cap);

#endif
