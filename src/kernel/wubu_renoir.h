/*
 * wubu_renoir.h -- kernel-owned AMD Raven/Renoir APU routing.
 */
#ifndef WUBU_RENOIR_H
#define WUBU_RENOIR_H

#include <stddef.h>

void wubu_renoir_probe(void);
int  wubu_renoir_present(void);
int  wubu_renoir_uses_radv(int vulkan_available);
int  wubu_renoir_is_apu(int integration);
void wubu_renoir_summary(char *out, size_t cap);

#endif
