/*
 * wubu_spdifrx.h -- kernel-owned audio SPDIF receiver routing.
 */
#ifndef WUBU_SPDIFRX_H
#define WUBU_SPDIFRX_H

#include <stddef.h>

void wubu_spdifrx_probe(void);
int  wubu_spdifrx_present(void);
int  wubu_spdifrx_rate(void);
int  wubu_spdifrx_lock(void);
int  wubu_spdifrx_format(void);
int  wubu_spdifrx_pcm(void);
const char *wubu_spdifrx_driver(void);
const char *wubu_spdifrx_format_for(const char *f);
const char *wubu_spdifrx_lock_for(const char *l);
int wubu_spdifrx_summary(char *out, size_t cap);

#endif
