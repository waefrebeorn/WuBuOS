/*
 * wubu_pcmplugin.h -- kernel-owned audio PCM plugin routing.
 */
#ifndef WUBU_PCMPLUGIN_H
#define WUBU_PCMPLUGIN_H

#include <stddef.h>

void wubu_pcmplugin_probe(void);
int  wubu_pcmplugin_present(void);
int  wubu_pcmplugin_rate(void);
int  wubu_pcmplugin_vol(void);
int  wubu_pcmplugin_copy(void);
int  wubu_pcmplugin_plugtype(void);
const char *wubu_pcmplugin_driver(void);
const char *wubu_pcmplugin_type_for(const char *t);
const char *wubu_pcmplugin_chain_for(const char *c);
int wubu_pcmplugin_summary(char *out, size_t cap);

#endif
