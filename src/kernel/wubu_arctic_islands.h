/*
 * wubu_arctic_islands.h -- kernel-owned AMD GCN4 Arctic routing.
 */
#ifndef WUBU_ARCTIC_ISLANDS_H
#define WUBU_ARCTIC_ISLANDS_H

#include <stddef.h>

void wubu_arctic_islands_probe(void);
int  wubu_arctic_islands_present(void);
int  wubu_arctic_islands_uses_radv(int vulkan_available);
int  wubu_arctic_islands_vulkan_level(int gcn_gen);
void wubu_arctic_islands_summary(char *out, size_t cap);

#endif
