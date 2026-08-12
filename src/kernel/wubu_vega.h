/*
 * wubu_vega.h -- kernel-owned AMD GCN5 Vega routing.
 */
#ifndef WUBU_VEGA_H
#define WUBU_VEGA_H

#include <stddef.h>

void wubu_vega_probe(void);
int  wubu_vega_present(void);
int  wubu_vega_radv(int vulkan_available);
int  wubu_vega_hbm_memory(int hbm_available);
void wubu_vega_summary(char *out, size_t cap);

#endif
