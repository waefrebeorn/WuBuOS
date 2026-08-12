/*
 * wubu_gpumem.h -- kernel-owned GPU memory bandwidth routing.
 */
#ifndef WUBU_GPUMEM_H
#define WUBU_GPUMEM_H

#include <stddef.h>

void wubu_gpumem_probe(void);
int  wubu_gpumem_present(void);
int  wubu_gpumem_tier(int bandwidth_gb_per_s);
int  wubu_gpumem_is_gb(void);
const char *wubu_gpumem_tier_str(int tier);
void wubu_gpumem_summary(char *out, size_t cap);

#endif
