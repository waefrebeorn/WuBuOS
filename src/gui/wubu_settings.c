/*
 * wubu_settings.c  --  WuBuOS Settings Daemon (facade)
 *
 * Owns the settings lifecycle + public accessor API. The live state and
 * default factories live in wubu_settings_defaults.c; JSON (de)serialization
 * lives in wubu_settings_io.c. This file wires them together and exposes the
 * stable public API declared in wubu_settings.h. C11, opaque-safe, no god
 * header (implementation details live in wubu_settings_internal.h).
 */

#include "wubu_settings_internal.h"
#include "wubu_theme.h"

#include <string.h>
#include <stdio.h>

/* -- Settings API ------------------------------------------------- */

int wubu_settings_init(void) {
    if (wubu_settings_load() != 0) {
        settings_apply_defaults(&g_settings);
        wubu_settings_save();
    }
    /* Apply theme immediately */
    wubu_theme_set(g_settings.theme.theme_id);
    return 0;
}

void wubu_settings_shutdown(void) {
    if (g_settings_dirty) wubu_settings_save();
}

const WubuSettings *wubu_settings_get(void) { return &g_settings; }

WubuSettings *wubu_settings_mut(void) { g_settings_dirty = true; return &g_settings; }

void wubu_settings_reset_category(SettingsCategory cat) {
    switch (cat) {
        case SETTINGS_CAT_THEME:     settings_default_theme(&g_settings.theme); break;
        case SETTINGS_CAT_FONTS:     settings_default_fonts(&g_settings.fonts); break;
        case SETTINGS_CAT_KEYBOARD:  settings_default_keyboard(&g_settings.keyboard); break;
        case SETTINGS_CAT_MOUSE:     settings_default_mouse(&g_settings.mouse); break;
        case SETTINGS_CAT_DISPLAY:   settings_default_display(&g_settings.display); break;
        case SETTINGS_CAT_A11Y:      settings_default_a11y(&g_settings.a11y); break;
        case SETTINGS_CAT_PRIVACY:   settings_default_privacy(&g_settings.privacy); break;
        default: return;
    }
    g_settings_dirty = true;
}

void wubu_settings_reset_all(void) {
    settings_apply_defaults(&g_settings);
    g_settings_dirty = true;
}

void wubu_settings_apply_live(void) {
    wubu_theme_set(g_settings.theme.theme_id);
    /* Font scale, high contrast, etc. would apply to running UI here */
}

/* -- Specific Helpers --------------------------------------------- */

uint32_t wubu_settings_get_font_scale(void) { return (uint32_t)(g_settings.a11y.font_scale * 100); }
uint32_t wubu_settings_get_cursor_size(void) { return 24; }
bool wubu_settings_high_contrast(void) { return g_settings.a11y.high_contrast; }
bool wubu_settings_reduce_motion(void) { return g_settings.a11y.reduce_animations; }
const char *wubu_settings_font_family(void) { return g_settings.fonts.font_family; }
int wubu_settings_font_size(void) { return g_settings.fonts.font_size; }

/* EOF */
