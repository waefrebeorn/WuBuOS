/*
 * wubu_zonseqwrite.h -- kernel-owned ZNS sequential write routing.
 */
#ifndef WUBU_ZONSEQWRITE_H
#define WUBU_ZONSEQWRITE_H

#include <stddef.h>

void wubu_zonseqwrite_probe(void);
int  wubu_zonseqwrite_present(void);
int  wubu_zonseqwrite_ok(int zone_state, int wp, int len, int max_len);
const char *wubu_zonseqwrite_state_str(int state);
void wubu_zonseqwrite_summary(char *out, size_t cap);

#endif
