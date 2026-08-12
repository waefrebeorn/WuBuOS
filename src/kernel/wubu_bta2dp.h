/*
 * wubu_bta2dp.h -- kernel-owned Bluetooth A2DP audio routing.
 */
#ifndef WUBU_BTA2DP_H
#define WUBU_BTA2DP_H

#include <stddef.h>

void wubu_bta2dp_probe(void);
int  wubu_bta2dp_present(void);
int  wubu_bta2dp_bitrate(int codec);
const char *wubu_bta2dp_codec_str(int codec);
void wubu_bta2dp_summary(char *out, size_t cap);

#endif
