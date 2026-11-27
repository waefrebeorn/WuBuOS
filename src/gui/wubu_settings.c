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
#include "fonts/wubu_font_text.h"   /* WUBU_FONT_TEXT_PX: the base size font_size is measured against */
#include "wubu_theme.h"

#include <string.h>
#include <stdio.h>

/* -- Settings API ------------------------------------------------- */

int wubu_settings_init(void) {
    /* Baseline EVERY key first: the JSON loader only overrides keys it finds,
     * so without this a settings file written by an older build (or one that
     * simply lacks a newer key) leaves that key at its zero value --
     * show_desktop_icons stayed false and a fresh install surfaced an empty
     * desktop. Defaults first, the file overrides on top. */
    settings_apply_defaults(&g_settings);
    if (wubu_settings_load() != 0) {
        wubu_settings_save();
    }
    /* Apply EVERYTHING that has a live effect, not just the theme. This used to set the theme and stop,
     * so a saved accessibility font scale and high-contrast flag were ignored on every boot: they only took
     * effect if the user opened Settings, changed something, and triggered wubu_settings_apply_live(). */
    wubu_settings_apply_live();
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

/* The VBE layer is not linked into every settings consumer (some GUI targets
 * link settings without the kernel framebuffer), so this is a weak reference
 * -- the same pattern wubu_theme.c uses for wubu_theme_node_get. */
extern __attribute__((weak)) void vbe_set_text_scale(int);

void wubu_settings_apply_live(void) {
    wubu_theme_set(g_settings.theme.theme_id);
    wubu_theme_set_high_contrast(g_settings.a11y.high_contrast);
    /* DPI is the BASELINE for text size, not decoration: it was written to the settings file and read
     * back by nobody, so a machine reporting 192 dpi drew exactly like a 96 dpi one. scale = font_scale *
     * dpi/96 keeps both knobs live and gives dpi its standard meaning. */
    double dpi = g_settings.fonts.dpi > 0 ? (double)g_settings.fonts.dpi : 96.0;
    /* font_size was the same kind of decoration dpi was: written at io.c:51, read at io.c:195, consumed by
     * nothing. It is the base size in pixels -- 13 is the face we bake -- so the three knobs compose:
     *   effective px = font_size * font_scale * (dpi / 96)
     * and the renderer takes the nearest integer text scale it can actually draw. */
    /* the base is the face we actually bake, not a magic 13 typed here */
    double base_px = g_settings.fonts.font_size > 0 ? (double)g_settings.fonts.font_size
                                                   : (double)WUBU_FONT_TEXT_PX;
    int ts = (int)((base_px / (double)WUBU_FONT_TEXT_PX) * g_settings.a11y.font_scale * (dpi / 96.0) + 0.5);
    if (ts < 1) ts = 1;
    if (ts > 4) ts = 4;
    if (vbe_set_text_scale) vbe_set_text_scale(ts);
}

/* -- Specific Helpers --------------------------------------------- */

uint32_t wubu_settings_get_font_scale(void) { return (uint32_t)(g_settings.a11y.font_scale * 100); }
uint32_t wubu_settings_get_cursor_size(void) { return 24; }
bool wubu_settings_high_contrast(void) { return g_settings.a11y.high_contrast; }
bool wubu_settings_reduce_motion(void) { return g_settings.a11y.reduce_animations; }
const char *wubu_settings_font_family(void) { return g_settings.fonts.font_family; }
int wubu_settings_font_size(void) { return g_settings.fonts.font_size; }
const char *wubu_settings_trusted_sync_url(void) { return g_settings.trusted_sync_url; }

int wubu_settings_sync_url_count(void) { return g_settings.trusted_sync_url_count; }

const char *wubu_settings_sync_url_at(int idx) {
    if (idx < 0 || idx >= g_settings.trusted_sync_url_count) return "";
    return g_settings.trusted_sync_urls[idx];
}

int wubu_settings_sync_url_set(const char *const *urls, int n) {
    char csv[WUBU_SETTINGS_TRUSTED_SYNC_URL_MAX];
    size_t off = 0;
    int used = 0;
    if (!urls) n = 0;
    if (n < 0) n = 0;
    if (n > (int)WUBU_SETTINGS_TRUSTED_SYNC_URLS_MAX) n = (int)WUBU_SETTINGS_TRUSTED_SYNC_URLS_MAX;
    csv[0] = '\0';
    for (int i = 0; i < n; i++) {
        size_t len;
        if (!urls[i] || !urls[i][0]) continue;
        len = strlen(urls[i]);
        if (len == 0 || len >= WUBU_SETTINGS_TRUSTED_SYNC_URL_MAX) continue;
        if (off + len + 2 > sizeof(csv)) break;   /* bounded: drop the rest */
        if (used) csv[off++] = ',';
        memcpy(csv + off, urls[i], len + 1);
        off += len;
        used++;
    }
    csv[off] = '\0';
    snprintf(g_settings.trusted_sync_url, sizeof(g_settings.trusted_sync_url), "%s", csv);
    g_settings_dirty = true;
    if (wubu_settings_save() != 0) return -1;
    return g_settings.trusted_sync_url_count;
}

/* EOF */
