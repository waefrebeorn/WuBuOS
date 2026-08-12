/*
 * wubu_vblank.h -- kernel-owned GPU VBLANK routing.
 */
#ifndef WUBU_VBLANK_H
#define WUBU_VBLANK_H

#include <stddef.h>

void wubu_vblank_probe(void);
int  wubu_vblank_present(void);
int  wubu_vblank_counter(void);
int  wubu_vblank_event(void);
int  wubu_vblank_time(void);
int  wubu_vblank_flip(void);
const char *wubu_vblank_driver(void);
const char *wubu_vblank_src_for(const char *s);
const char *wubu_vblank_mode_for(const char *m);
int wubu_vblank_summary(char *out, size_t cap);

#endif
