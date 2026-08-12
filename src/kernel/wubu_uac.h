/*
 * wubu_uac.h -- kernel-owned USB audio routing.
 */
#ifndef WUBU_UAC_H
#define WUBU_UAC_H

#include <stddef.h>

void wubu_uac_probe(void);
int  wubu_uac_present(void);
int  wubu_uac_uac1(void);
int  wubu_uac_uac2(void);
int  wubu_uac_iso(void);
int  wubu_uac_alt(void);
const char *wubu_uac_driver(void);
const char *wubu_uac_version_for(const char *v);
const char *wubu_uac_ep_for(const char *e);
int wubu_uac_summary(char *out, size_t cap);

#endif
