/*
 * wubu_settings_internal.h -- WuBuOS settings: shared state + internal decls
 *
 * Holds the live settings state (g_settings) and the internal function
 * declarations used by the split settings sub-modules (defaults, I/O, facade).
 * Public API stays in wubu_settings.h; cross-TU implementation details live
 * here. Opaque-safe, minimal includes, no god headers.
 */

#ifndef WUBU_SETTINGS_INTERNAL_H
#define WUBU_SETTINGS_INTERNAL_H

#include "wubu_settings.h"
#include "wubu_json.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Live settings state (defined once, in wubu_settings_defaults.c). */
extern WubuSettings g_settings;
extern bool          g_settings_dirty;

/* -- Default factories (wubu_settings_defaults.c) -- */
void settings_default_theme(ThemeSettings *t);
void settings_default_fonts(FontSettings *f);
void settings_default_keyboard(KeyboardSettings *k);
void settings_default_mouse(MouseSettings *m);
void settings_default_display(DisplaySettings *d);
void settings_default_a11y(A11ySettings *a);
void settings_default_privacy(PrivacySettings *p);
void settings_apply_defaults(WubuSettings *s);

/* Ensure the config directory exists (wubu_settings_defaults.c). */
bool ensure_config_dir(void);

/* -- JSON serialization (wubu_settings_io.c) -- */
int wubu_settings_save(void);
int wubu_settings_load(void);

#endif /* WUBU_SETTINGS_INTERNAL_H */
