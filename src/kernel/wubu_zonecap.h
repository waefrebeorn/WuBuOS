/*
 * wubu_zonecap.h -- kernel-owned ZNS zone capacity routing.
 */
#ifndef WUBU_ZONECAP_H
#define WUBU_ZONECAP_H

#include <stddef.h>

void wubu_zonecap_probe(void);
int  wubu_zonecap_present(void);
int  wubu_zonecap_safe(int wp, int zone_cap, int len);
int  wubu_zonecap_full(int wp, int zone_cap);
void wubu_zonecap_summary(char *out, size_t lim);

#endif
