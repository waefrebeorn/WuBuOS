/*
 * wubu_vpudecode.h -- kernel-owned GPU video decode routing.
 */
#ifndef WUBU_VPUDECODE_H
#define WUBU_VPUDECODE_H

#include <stddef.h>

void wubu_vpudecode_probe(void);
int  wubu_vpudecode_present(void);
int  wubu_vpudecode_supported(int codec);
const char *wubu_vpudecode_codec_str(int codec);
void wubu_vpudecode_summary(char *out, size_t cap);

#endif
