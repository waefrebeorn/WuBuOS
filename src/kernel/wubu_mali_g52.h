/*
 * wubu_mali_g52.h -- kernel-owned ARM Mali G52 routing.
 */
#ifndef WUBU_MALI_G52_H
#define WUBU_MALI_G52_H

#include <stddef.h>

void wubu_mali_g52_probe(void);
int  wubu_mali_g52_present(void);
int  wubu_mali_g52_uses_panfrost(int panfrost_available);
int  wubu_mali_g52_opengl_es(int gles_level);
void wubu_mali_g52_summary(char *out, size_t cap);

#endif
