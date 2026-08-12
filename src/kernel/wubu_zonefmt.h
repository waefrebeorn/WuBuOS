/*
 * wubu_zonefmt.h -- kernel-owned ZNS zone format/reset routing.
 */
#ifndef WUBU_ZONEFMT_H
#define WUBU_ZONEFMT_H

#include <stddef.h>

void wubu_zonefmt_probe(void);
int  wubu_zonefmt_present(void);
int  wubu_zonefmt_reset_ok(int zone_state);
const char *wubu_zonefmt_state_str(int state);
void wubu_zonefmt_summary(char *out, size_t cap);

#endif
