/*
 * wubu_drm.h -- kernel-owned GPU DRM routing.
 */
#ifndef WUBU_DRM_H
#define WUBU_DRM_H

#include <stddef.h>

void wubu_drm_probe(void);
int  wubu_drm_present(void);
int  wubu_drm_kms(void);
int  wubu_drm_gem(void);
int  wubu_drm_prime(void);
int  wubu_drm_msi(void);
const char *wubu_drm_driver(void);
const char *wubu_drm_subsys_for(const char *s);
const char *wubu_drm_obj_for(const char *o);
int wubu_drm_summary(char *out, size_t cap);

#endif
