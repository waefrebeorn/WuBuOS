/*
 * wubu_gt2xx.h -- kernel-owned NVIDIA GT2xx legacy routing.
 */
#ifndef WUBU_GT2XX_H
#define WUBU_GT2XX_H

#include <stddef.h>

void wubu_gt2xx_probe(void);
int  wubu_gt2xx_present(void);
int  wubu_gt2xx_needs_nouveau(int legacy_eol);
int  wubu_gt2xx_nouveau_available(int nouveau_present);
void wubu_gt2xx_summary(char *out, size_t cap);

#endif
