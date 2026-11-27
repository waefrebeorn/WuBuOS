/*
 * wubu_settings_defaults.c -- WuBuOS settings: state + default factories
 *
 * Self-contained concern split out of wubu_settings.c: the live settings
 * state (g_settings / g_settings_dirty), the config-path resolution, and the
 * per-section default factories. Depends only on the settings public types
 * (wubu_settings_internal.h). No JSON read/write, no lifecycle.
 */

#include "../egojar.h"
#include "wubu_settings_internal.h"
#include "wubu_theme.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* -- Config Path -------------------------------------------------- */

/* Resolved on EVERY call, never cached. A cached path survived a later
 * XDG_CONFIG_HOME change, which is how a test that sandboxes itself after
 * startup still read and rewrote the live user's settings file: the path was
 * already frozen at $HOME/.config by the first access. WUBU_SETTINGS_PATH
 * names one file explicitly (tests, alternate profiles). */
const char *wubu_settings_path(void) {
    static char path[512];
    const char *explicit_file = getenv("WUBU_SETTINGS_PATH");
    if (explicit_file && explicit_file[0]) {
        snprintf(path, sizeof(path), "%s", explicit_file);
        return path;
    }
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(path, sizeof(path), "%s/wubu/settings.conf", xdg);
    } else if (home) {
        snprintf(path, sizeof(path), "%s/.config/wubu/settings.conf", home);
    } else {
        strcpy(path, "/tmp/wubu/settings.conf");
    }
    return path;
}

bool ensure_config_dir(void) {
    char dir[512];
    const char *path = wubu_settings_path();
    strncpy(dir, path, sizeof(dir) - 1);
    char *last = strrchr(dir, '/');
    if (last) *last = '\0';
    struct stat st;
    if (stat(dir, &st) == 0) return true;
    return mkdir(dir, 0755) == 0 || errno == EEXIST;
}

/* -- Default Settings --------------------------------------------- */

WubuSettings g_settings = {0};
bool          g_settings_dirty = false;
/* Set once defaults have been laid down. Until then the store holds a zeroed
 * struct, and writing it out produces a settings file in which every boolean is
 * false, dpi is 0, font_scale 0.00 and the theme is 0 -- exactly the file that
 * made a live desktop boot light-themed with hidden icons and doubled text. */
bool          g_settings_ready = false;

void settings_default_theme(ThemeSettings *t) {
    /* Product default: the Win11-class dark theme.  The classic Win98 theme
     * is one Ctrl+T away, but the compositor session ships the modern shell
     * the user asked for. */
    /* A fresh profile starts on the Fluent look; the retro presets are a choice, not
     * a default. (wubu_theme_init's default is the same id for the no-settings case.) */
    t->theme_id = THEME_DARK;   /* dark only */
    t->wallpaper_path[0] = '\0';
    t->wallpaper_mode = 2; /* STRETCH — full desktop coverage */
    t->use_custom_colors = false;
    memset(t->custom_colors, 0, sizeof(t->custom_colors));
    /* A desktop with no icons is broken by default: show them until the user
     * hides them from the Control Panel. (Unset, this was false and a fresh
     * install surfaced an empty desktop.) */
    t->show_desktop_icons = true;
    t->auto_arrange = true;
}

void settings_default_fonts(FontSettings *f) {
    strcpy(f->font_family, "DejaVu Sans");
    f->font_size = 11;
    f->antialiasing = true;
    f->hinting = true;
    f->dpi = 96;
    strcpy(f->monospace_family, "DejaVu Sans Mono");
    f->monospace_size = 10;
}

void settings_default_keyboard(KeyboardSettings *k) {
    k->repeat_delay = 250;
    k->repeat_rate = 30;
    strcpy(k->layout, "us");
    k->variant[0] = '\0';
    k->numlock_on = true;
    k->capslock_ctrl = false;
    k->compose_key = true;
    k->scroll_tick_lines = 3;
}

void settings_default_mouse(MouseSettings *m) {
    m->speed = 1.0;
    m->acceleration = 0.5;
    m->left_handed = false;
    m->double_click_time = 300;
    m->natural_scroll = false;
    m->scroll_lines = 3;
}

void settings_default_display(DisplaySettings *d) {
    d->width = 1024;
    d->height = 768;
    d->refresh_rate = 60;
    d->scale_factor = 1.0;
    d->fractional_scaling = false;
    d->primary_monitor = 0;
    d->mirror_displays = false;
    for (int i = 0; i < 4; i++) d->rotation[i] = 0;
}

void settings_default_a11y(A11ySettings *a) {
    a->high_contrast = false;
    a->screen_reader = false;
    a->screen_magnifier = false;
    a->sticky_keys = false;
    a->slow_keys = false;
    a->bounce_keys = false;
    a->mouse_keys = false;
    a->font_scale = 1.0;
    a->reduce_animations = false;
}

void settings_default_privacy(PrivacySettings *p) {
    p->remember_recent = true;
    p->remember_history = true;
    p->telemetry = false;
    p->location_services = false;
    p->history_retention_days = 30;
    p->auto_clear_temp = true;
}

void settings_apply_defaults(WubuSettings *s) {
    settings_default_theme(&s->theme);
    settings_default_fonts(&s->fonts);
    settings_default_keyboard(&s->keyboard);
    settings_default_mouse(&s->mouse);
    settings_default_display(&s->display);
    settings_default_a11y(&s->a11y);
    settings_default_privacy(&s->privacy);
    s->trusted_sync_url[0] = '\0';
    memset(s->trusted_sync_urls, 0, sizeof(s->trusted_sync_urls));
    s->trusted_sync_url_count = 0;
    s->sync_auto_enabled = false;    /* no background sync daemon by default */
    s->sync_interval_secs = 0;
    s->sync_backoff_initial_secs = WUBU_SETTINGS_SYNC_BACKOFF_DEFAULT_INITIAL;
    s->sync_backoff_max_secs = WUBU_SETTINGS_SYNC_BACKOFF_DEFAULT_MAX;
    s->sync_backoff_jitter_pct = WUBU_SETTINGS_SYNC_BACKOFF_DEFAULT_JITTER;
    s->version = 1;
    s->last_modified = (uint64_t)time(NULL);
    g_settings_ready = true;
}
