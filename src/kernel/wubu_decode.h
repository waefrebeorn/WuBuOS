/*
 * wubu_decode.h -- kernel-owned GPU video decode routing.
 */
#ifndef WUBU_DECODE_H
#define WUBU_DECODE_H

#include <stddef.h>

void wubu_decode_probe(void);
int  wubu_decode_present(void);
int  wubu_decode_h264(void);
int  wubu_decode_h265(void);
int  wubu_decode_av1(void);
int  wubu_decode_vp9(void);
const char *wubu_decode_driver(void);
const char *wubu_decode_codec_for(const char *c);
const char *wubu_decode_api_for(const char *a);
int wubu_decode_summary(char *out, size_t cap);

#endif
