/*
 * wubu_encode.h -- kernel-owned GPU video encode routing.
 */
#ifndef WUBU_ENCODE_H
#define WUBU_ENCODE_H

#include <stddef.h>

void wubu_encode_probe(void);
int  wubu_encode_present(void);
int  wubu_encode_h264(void);
int  wubu_encode_h265(void);
int  wubu_encode_vp9(void);
int  wubu_encode_av1(void);
const char *wubu_encode_driver(void);
const char *wubu_encode_codec_for(const char *c);
const char *wubu_encode_api_for(const char *a);
int wubu_encode_summary(char *out, size_t cap);

#endif
