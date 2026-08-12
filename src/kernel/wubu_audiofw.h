/*
 * wubu_audiofw.h -- kernel-owned audio firmware routing.
 */
#ifndef WUBU_AUDIOFW_H
#define WUBU_AUDIOFW_H

#include <stddef.h>

void wubu_audiofw_probe(void);
int  wubu_audiofw_present(void);
int  wubu_audiofw_codec(void);
int  wubu_audiofw_dsp(void);
int  wubu_audiofw_loader(void);
int  wubu_audiofw_bios(void);
const char *wubu_audiofw_driver(void);
const char *wubu_audiofw_codec_for(const char *c);
const char *wubu_audiofw_loader_for(const char *l);
int wubu_audiofw_summary(char *out, size_t cap);

#endif
