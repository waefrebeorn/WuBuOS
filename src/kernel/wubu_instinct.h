/*
 * wubu_instinct.h -- kernel-owned AMD Instinct MI routing.
 */
#ifndef WUBU_INSTINCT_H
#define WUBU_INSTINCT_H

#include <stddef.h>

void wubu_instinct_probe(void);
int  wubu_instinct_present(void);
int  wubu_instinct_uses_rocm(int rocm_available);
int  wubu_instinct_is_datacenter(int gpu_type);
void wubu_instinct_summary(char *out, size_t cap);

#endif
