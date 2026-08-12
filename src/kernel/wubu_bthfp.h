/*
 * wubu_bthfp.h -- kernel-owned Bluetooth HSP/HFP routing.
 */
#ifndef WUBU_BTHFP_H
#define WUBU_BTHFP_H

#include <stddef.h>

void wubu_bthfp_probe(void);
int  wubu_bthfp_present(void);
int  wubu_bthfp_call_state(int sco_active, int at_cmd_ready);
const char *wubu_bthfp_state_str(int state);
void wubu_bthfp_summary(char *out, size_t cap);

#endif
