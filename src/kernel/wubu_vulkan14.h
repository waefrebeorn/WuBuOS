/*
 * wubu_vulkan14.h -- kernel-owned Vulkan 1.4 runtime routing.
 */
#ifndef WUBU_VULKAN14_H
#define WUBU_VULKAN14_H

#include <stddef.h>

void wubu_vulkan14_probe(void);
int  wubu_vulkan14_present(void);
int  wubu_vulkan14_full_profile(int full_available);
int  wubu_vulkan14_is_conformant(int radv_conformant);
void wubu_vulkan14_summary(char *out, size_t cap);

#endif
