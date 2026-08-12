/*
 * wubu_btaudio.h -- kernel-owned Bluetooth audio profile routing.
 */
#ifndef WUBU_BTAUDIO_H
#define WUBU_BTAUDIO_H

#include <stddef.h>

void wubu_btaudio_probe(void);
int  wubu_btaudio_present(void);
int  wubu_btaudio_auto(int latency_ms);
const char *wubu_btaudio_profile_str(int profile);
void wubu_btaudio_summary(char *out, size_t cap);

#endif
