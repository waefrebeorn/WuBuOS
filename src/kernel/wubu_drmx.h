/*
 * wubu_drmx.h -- kernel-owned DRM writeback/overlay + HDR/color routing.
 */
#ifndef WUBU_DRMX_H
#define WUBU_DRMX_H

#include <stddef.h>

/* W1: probe the DRM-advanced topology. */
void wubu_drmx_probe(void);

/* W2: accessors */
int  wubu_drmx_writeback(void);
int  wubu_drmx_overlay(void);
int  wubu_drmx_hdr(void);
int  wubu_drmx_color_mgmt(void);
int  wubu_drmx_vkms(void);
const char *wubu_drmx_driver(void);

/* W3: driver routing. */
const char *wubu_drmx_writeback_driver(const char *gpu);
const char *wubu_drmx_hdr_mode(const char *mode);

/* W4: summary fragment. */
int wubu_drmx_summary(char *out, size_t cap);

#endif /* WUBU_DRMX_H */
