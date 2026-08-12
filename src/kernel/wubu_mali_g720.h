/*
 * wubu_mali_g720.h -- kernel-owned ARM Mali G720 routing.
 */
#ifndef WUBU_MALI_G720_H
#define WUBU_MALI_G720_H

#include <stddef.h>

void wubu_mali_g720_probe(void);
int  wubu_mali_g720_present(void);
int  wubu_mali_g720_uses_panthor(int panthor_available);
int  wubu_mali_g720_vulkan(int vk_level);
void wubu_mali_g720_summary(char *out, size_t cap);

#endif
