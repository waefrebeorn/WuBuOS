/*
 * wubu_vc4.h -- kernel-owned Broadcom VideoCore vc4 routing.
 */
#ifndef WUBU_VC4_H
#define WUBU_VC4_H

#include <stddef.h>

void wubu_vc4_probe(void);
int  wubu_vc4_present(void);
int  wubu_vc4_uses_vc4_v3d(int dual_driver);
int  wubu_vc4_has_3d(int v3d_available);
void wubu_vc4_summary(char *out, size_t cap);

#endif
