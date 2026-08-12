/*
 * wubu_display.h -- kernel-owned display driver routing (DRM/KMS matrix).
 */
#ifndef WUBU_DISPLAY_H
#define WUBU_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

/* W1: probe the display topology (DRM/KMS driver per GPU generation). */
void wubu_display_probe(void);

/* W2: accessors */
int          wubu_display_present(void);
const char *wubu_display_driver(void);       /* "amdgpu"|"i915"|"xe"|"nouveau"|"nvidia"|"simpledrm" */
const char *wubu_display_chip_name(void);
int          wubu_display_xe_preferred(void); /* prefer Intel xe over i915 */
int          wubu_display_has_render_node(void);
int          wubu_display_card_index(void);

/* W3: KMS /dev/dri path helpers */
const char *wubu_display_card_path(void);     /* /dev/dri/cardN */
const char *wubu_display_render_path(void);   /* /dev/dri/renderD128 */

/* W4: summary fragment */
int wubu_display_summary(char *out, size_t cap);

/* W5: KMS feature flags */
int wubu_display_atomic_modeset(void);
int wubu_display_has_edid(void);
int wubu_display_has_mst(void);
int wubu_display_has_dsc(void);
int wubu_display_has_hdcp(void);
int wubu_display_has_vrr(void);

#endif /* WUBU_DISPLAY_H */
