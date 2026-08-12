/*
 * wubu_mali_g77.h -- kernel-owned ARM Mali G77 routing.
 */
#ifndef WUBU_MALI_G77_H
#define WUBU_MALI_G77_H

#include <stddef.h>

void wubu_mali_g77_probe(void);
int  wubu_mali_g77_present(void);
int  wubu_mali_g77_uses_panfrost(int panfrost_available);
int  wubu_mali_g77_has_panvk(int panvk_available);
void wubu_mali_g77_summary(char *out, size_t cap);

#endif
