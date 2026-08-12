/*
 * wubu_spdiftx.h -- kernel-owned SPDIF TX control routing.
 */
#ifndef WUBU_SPDIFTX_H
#define WUBU_SPDIFTX_H

#include <stddef.h>

void wubu_spdiftx_probe(void);
int  wubu_spdiftx_present(void);
int  wubu_spdiftx_iec(void);
int  wubu_spdiftx_ac3(void);
int  wubu_spdiftx_dts(void);
int  wubu_spdiftx_optical(void);
const char *wubu_spdiftx_driver(void);
const char *wubu_spdiftx_enc_for(const char *e);
const char *wubu_spdiftx_media_for(const char *m);
int wubu_spdiftx_summary(char *out, size_t cap);

#endif
