/*
 * wubu_vram.h -- kernel-owned GPU VRAM + framebuffer memory routing.
 */
#ifndef WUBU_VRAM_H
#define WUBU_VRAM_H

#include <stddef.h>

/* W1: probe the VRAM topology. */
void wubu_vram_probe(void);

/* W2: accessors */
int  wubu_vram_present(void);
int  wubu_vram_fb(void);
int  wubu_vram_stolen(void);
int  wubu_vram_ttm(void);
int  wubu_vram_drm_mm(void);
const char *wubu_vram_driver(void);

/* W3: VRAM routing. */
const char *wubu_vram_pool_for(const char *pool);
const char *wubu_vram_alloc_for(const char *hint);

/* W4: summary fragment. */
int wubu_vram_summary(char *out, size_t cap);

#endif /* WUBU_VRAM_H */
