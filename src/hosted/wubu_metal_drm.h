/*
 * wubu_metal_drm.h -- DRM/KMS display backend module interface.
 *
 * Split from wubu_metal.c (the bare-metal DRM/KMS display engine:
 * atomic-commit helpers, connector/CRTC/plane prop getters, fb creation,
 * init/shutdown/flip/set_mode/get_modes). The wubu_disp_* dispatch in
 * wubu_metal.c calls these; this header is the compiler-enforced boundary
 * between the facade TU and the backend TU.
 *
 * C11 opaque-struct: depends only on wubu_metal.h public types. No god
 * header, no libdrm types leaked here (the backend hides those behind
 * WUBU_USE_DRM internally).
 */

#ifndef WUBU_METAL_DRM_H
#define WUBU_METAL_DRM_H

#include "wubu_metal.h"   /* WubuDisplay, DISP_* enum, g_display extern */

/* Backend entry points (real + stub variants, both behind WUBU_USE_DRM).
 * Declared non-static so wubu_metal.c's wubu_disp_* dispatch links against
 * this separate TU. No other TU should call these directly. */
int  wubu_drm_init(int width, int height);
void wubu_drm_shutdown(void);
void wubu_drm_flip(void);
int  wubu_drm_set_mode(int width, int height, int refresh_hz);
int  wubu_drm_get_modes(int *widths, int *heights, int max);

#endif /* WUBU_METAL_DRM_H */
