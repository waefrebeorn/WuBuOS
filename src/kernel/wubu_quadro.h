/*
 * wubu_quadro.h -- kernel-owned NVIDIA Quadro professional routing.
 */
#ifndef WUBU_QUADRO_H
#define WUBU_QUADRO_H

#include <stddef.h>

void wubu_quadro_probe(void);
int  wubu_quadro_present(void);
int  wubu_quadro_is_professional(int gpu_type);
int  wubu_quadro_has_isv(int isv_certified);
void wubu_quadro_summary(char *out, size_t cap);

#endif
