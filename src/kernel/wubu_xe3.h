/*
 * wubu_xe3.h -- kernel-owned Intel Xe3 (Celestial) routing.
 */
#ifndef WUBU_XE3_H
#define WUBU_XE3_H

#include <stddef.h>

void wubu_xe3_probe(void);
int  wubu_xe3_present(void);
int  wubu_xe3_uses_xe_driver(int xe_available);
int  wubu_xe3_uses_iris_anv(int mesa_ready);
void wubu_xe3_summary(char *out, size_t cap);

#endif
