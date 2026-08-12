/*
 * wubu_memmgr.h -- kernel-owned GPU memory manager routing.
 */
#ifndef WUBU_MEMMGR_H
#define WUBU_MEMMGR_H

#include <stddef.h>

void wubu_memmgr_probe(void);
int  wubu_memmgr_present(void);
int  wubu_memmgr_gem(void);
int  wubu_memmgr_ttm(void);
int  wubu_memmgr_vram(void);
int  wubu_memmgr_gtt(void);
const char *wubu_memmgr_driver(void);
const char *wubu_memmgr_heap_for(const char *h);
const char *wubu_memmgr_type_for(const char *t);
int wubu_memmgr_summary(char *out, size_t cap);

#endif
