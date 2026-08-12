/*
 * wubu_zoneappend.h -- kernel-owned ZNS zone append routing.
 */
#ifndef WUBU_ZONEAPPEND_H
#define WUBU_ZONEAPPEND_H

#include <stddef.h>

void wubu_zoneappend_probe(void);
int  wubu_zoneappend_present(void);
int  wubu_zoneappend_ok(int zone_state, int seq_pos);
const char *wubu_zoneappend_state_str(int state);
void wubu_zoneappend_summary(char *out, size_t cap);

#endif
