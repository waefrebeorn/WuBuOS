/*
 * wubu_settings_defaults.c -- WuBuOS settings: state + default factories
 *
 * Self-contained concern split out of wubu_settings.c: the live settings
 * state (g_settings / g_settings_dirty), the config-path resolution, and the
 * per-section default factories. Depends only on the settings public types
 * (wubu_settings_internal.h). No JSON read/write, no lifecycle.
 */

#include "wubu_settings_internal.h"
#include "wubu_theme.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* -- Config Path -------------------------------------------------- */

static char g_config_path[512] = {0};

const char *wubu_settings_path(void) {
    if (g_config_path[0] == '\0') {
        const char *home = getenv("HOME");
        const char *xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && xdg[0]) {
            snprintf(g_config_path, sizeof(g_config_path), "%s/wubu/settings.conf", xdg);
        } else if (home) {
            snprintf(g_config_path, sizeof(g_config_path), "%s/.config/wubu/settings.conf", home);
        } else {
            strcpy(g_config_path, "/tmp/wubu/settings.conf");
        }
    }
    return g_config_path;
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

void settings_default_theme(ThemeSettings *t) {
    t->theme_id = THEME_WIN98_CLASSIC;
    t->wallpaper_path[0] = '\0';
    t->wallpaper_mode = 1;
    t->use_custom_colors = false;
    memset(t->custom_colors, 0, sizeof(t->custom_colors));
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
    s->version = 1;
    s->last_modified = (uint64_t)time(NULL);
}
