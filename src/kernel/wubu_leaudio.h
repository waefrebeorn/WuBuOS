/*
 * wubu_leaudio.h -- kernel-owned Bluetooth LE Audio routing.
 */
#ifndef WUBU_LEAUDIO_H
#define WUBU_LEAUDIO_H

#include <stddef.h>

void wubu_leaudio_probe(void);
int  wubu_leaudio_present(void);
int  wubu_leaudio_latency(int frame_length);
const char *wubu_leaudio_codec_str(int codec);
void wubu_leaudio_summary(char *out, size_t cap);

#endif
