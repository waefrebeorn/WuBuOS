/*
 * wubu_vpuencode.h -- kernel-owned GPU video encode routing.
 */
#ifndef WUBU_VPUENCODE_H
#define WUBU_VPUENCODE_H

#include <stddef.h>

void wubu_vpuencode_probe(void);
int  wubu_vpuencode_present(void);
int  wubu_vpuencode_rate(int width, int height, int fps);
const char *wubu_vpuencode_codec_str(int codec);
void wubu_vpuencode_summary(char *out, size_t cap);

#endif
