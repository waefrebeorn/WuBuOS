/*
 * wubu_btclassic.h -- kernel-owned Bluetooth classic routing.
 */
#ifndef WUBU_BTLASSIC_H
#define WUBU_BTLASSIC_H

#include <stddef.h>

void wubu_btclassic_probe(void);
int  wubu_btclassic_present(void);
int  wubu_btclassic_rate(int sco, int esc);
const char *wubu_btclassic_profile_str(int profile);
void wubu_btclassic_summary(char *out, size_t cap);

#endif
