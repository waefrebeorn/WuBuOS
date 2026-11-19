/*
 * wm_shadow.h -- cached elevation shadow for the focused window.
 *
 * Design ref: rizonesoft/impossible-os@be8c286 docs/design/tokens.json
 * elevation.window_active (GPL-3.0-only). Ground-up WuBuOS C11; no upstream
 * code copied. One concern: build the blurred mask once per geometry, then
 * composite it through the gui/gfx alpha path.
 */
#ifndef WM_SHADOW_H
#define WM_SHADOW_H

#include "../wm.h"

/* Composite the window's elevation shadow onto the VBE back buffer.
 * Call BEFORE the window's chrome, inside the window's clip. No-op outside
 * THEME_DARK, under high contrast, or for a dead/NULL window. */
void wm_shadow_render(const DosGuiWindow *win);

/* Gate oracle: how many times the mask has been (re)blurred. Unchanged
 * geometry must reuse the cache, so this stays flat across idle frames. */
unsigned wm_shadow_blur_count(void);

#endif /* WM_SHADOW_H */