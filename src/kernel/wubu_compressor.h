/*
 * wubu_compressor.h -- kernel-owned audio compressor routing.
 */
#ifndef WUBU_COMPRESSOR_H
#define WUBU_COMPRESSOR_H

#include <stddef.h>

void wubu_compressor_probe(void);
int  wubu_compressor_present(void);
int  wubu_compressor_thresh(void);
int  wubu_compressor_ratio(void);
int  wubu_compressor_attack(void);
int  wubu_compressor_release(void);
const char *wubu_compressor_driver(void);
const char *wubu_compressor_ratio_for(const char *r);
const char *wubu_compressor_knee_for(const char *k);
int wubu_compressor_summary(char *out, size_t cap);

#endif
