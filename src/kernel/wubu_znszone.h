/*
 * wubu_znszone.h -- kernel-owned ZNS (Zoned Namespaces) routing.
 */
#ifndef WUBU_ZNSZONE_H
#define WUBU_ZNSZONE_H

#include <stddef.h>

void wubu_znszone_probe(void);
int  wubu_znszone_present(void);
int  wubu_znszone_active(int total, int used);
const char *wubu_znszone_state_str(int state);
void wubu_znszone_summary(char *out, size_t cap);

#endif
