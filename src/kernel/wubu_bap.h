/*
 * wubu_bap.h -- kernel-owned Bluetooth BAP routing.
 */
#ifndef WUBU_BAP_H
#define WUBU_BAP_H

#include <stddef.h>

void wubu_bap_probe(void);
int  wubu_bap_present(void);
int  wubu_bap_codec(int sample_rate, int bit_depth);
int  wubu_bap_is_ready(int configured, int connected);
void wubu_bap_summary(char *out, size_t cap);

#endif
