/*
 * wubu_settings.c  --  WuBuOS Settings Daemon Implementation
 * Complete implementation: defaults, file I/O, JSON load/save, API
 */

#include "wubu_json.h"
#include "wubu_settings.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "../runtime/styx.h"
#include "../runtime/styxfs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>

/* -- JSON-lite Parser (minimal, no external deps) ----------------- */


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

static bool ensure_config_dir(void) {
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

static WubuSettings g_settings = {0};
static bool g_settings_dirty = false;

static void settings_default_theme(ThemeSettings *t) {
    t->theme_id = THEME_WIN98_CLASSIC;
    t->wallpaper_path[0] = '\0';
    t->wallpaper_mode = 1;
    t->use_custom_colors = false;
    memset(t->custom_colors, 0, sizeof(t->custom_colors));
}

static void settings_default_fonts(FontSettings *f) {
    strcpy(f->font_family, "DejaVu Sans");
    f->font_size = 11;
    f->antialiasing = true;
    f->hinting = true;
    f->dpi = 96;
    strcpy(f->monospace_family, "DejaVu Sans Mono");
    f->monospace_size = 10;
}

static void settings_default_keyboard(KeyboardSettings *k) {
    k->repeat_delay = 250;
    k->repeat_rate = 30;
    strcpy(k->layout, "us");
    k->variant[0] = '\0';
    k->numlock_on = true;
    k->capslock_ctrl = false;
    k->compose_key = true;
    k->scroll_tick_lines = 3;
}

static void settings_default_mouse(MouseSettings *m) {
    m->speed = 1.0;
    m->acceleration = 0.5;
    m->left_handed = false;
    m->double_click_time = 300;
    m->natural_scroll = false;
    m->scroll_lines = 3;
}

static void settings_default_display(DisplaySettings *d) {
    d->width = 1024;
    d->height = 768;
    d->refresh_rate = 60;
    d->scale_factor = 1.0;
    d->fractional_scaling = false;
    d->primary_monitor = 0;
    d->mirror_displays = false;
    for (int i = 0; i < 4; i++) d->rotation[i] = 0;
}

static void settings_default_a11y(A11ySettings *a) {
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

static void settings_default_privacy(PrivacySettings *p) {
    p->remember_recent = true;
    p->remember_history = true;
    p->telemetry = false;
    p->location_services = false;
    p->history_retention_days = 30;
    p->auto_clear_temp = true;
}

static void settings_apply_defaults(WubuSettings *s) {
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

/* -- JSON Writer -------------------------------------------------- */

static void json_write_key(FILE *f, const char *key, int indent) {
    fprintf(f, "%*s\"%s\": ", indent, "", key);
}
static void json_write_string(FILE *f, const char *val) {
    fprintf(f, "\"%s\"", val ? val : "");
}
static void json_write_int(FILE *f, int val) { fprintf(f, "%d", val); }
static void json_write_bool(FILE *f, bool val) { fprintf(f, "%s", val ? "true" : "false"); }
static void json_write_double(FILE *f, double val) { fprintf(f, "%.2f", val); }
static void json_write_uint(FILE *f, uint32_t val) { fprintf(f, "%u", val); }
static void json_write_ulong(FILE *f, uint64_t val) { fprintf(f, "%llu", (unsigned long long)val); }

static void save_theme(FILE *f, const ThemeSettings *t, int indent) {
    json_write_key(f, "theme_id", indent); json_write_int(f, t->theme_id); fprintf(f, ",\n");
    json_write_key(f, "wallpaper_path", indent); json_write_string(f, t->wallpaper_path); fprintf(f, ",\n");
    json_write_key(f, "wallpaper_mode", indent); json_write_int(f, t->wallpaper_mode); fprintf(f, ",\n");
    json_write_key(f, "use_custom_colors", indent); json_write_bool(f, t->use_custom_colors); fprintf(f, ",\n");
    json_write_key(f, "icon_layout_count", indent); json_write_int(f, t->icon_layout_count); fprintf(f, ",\n");
    for (int i = 0; i < t->icon_layout_count && i < WUBU_ICON_LAYOUT_MAX; i++) {
        fprintf(f, "%*s\"icon_layout_%d\": { \"name\": \"%s\", \"grid_x\": %d, \"grid_y\": %d, \"alive\": %s }%s\n",
                indent, "", i,
                t->icon_layout[i].name, t->icon_layout[i].grid_x, t->icon_layout[i].grid_y,
                t->icon_layout[i].alive ? "true" : "false",
                (i == t->icon_layout_count - 1) ? "" : ",");
    }
}
static void save_fonts(FILE *f, const FontSettings *fnt, int indent) {
    json_write_key(f, "font_family", indent); json_write_string(f, fnt->font_family); fprintf(f, ",\n");
    json_write_key(f, "font_size", indent); json_write_int(f, fnt->font_size); fprintf(f, ",\n");
    json_write_key(f, "antialiasing", indent); json_write_bool(f, fnt->antialiasing); fprintf(f, ",\n");
    json_write_key(f, "hinting", indent); json_write_bool(f, fnt->hinting); fprintf(f, ",\n");
    json_write_key(f, "dpi", indent); json_write_int(f, fnt->dpi); fprintf(f, ",\n");
    json_write_key(f, "monospace_family", indent); json_write_string(f, fnt->monospace_family); fprintf(f, ",\n");
    json_write_key(f, "monospace_size", indent); json_write_int(f, fnt->monospace_size);
}
static void save_keyboard(FILE *f, const KeyboardSettings *k, int indent) {
    json_write_key(f, "repeat_delay", indent); json_write_int(f, k->repeat_delay); fprintf(f, ",\n");
    json_write_key(f, "repeat_rate", indent); json_write_int(f, k->repeat_rate); fprintf(f, ",\n");
    json_write_key(f, "layout", indent); json_write_string(f, k->layout); fprintf(f, ",\n");
    json_write_key(f, "variant", indent); json_write_string(f, k->variant); fprintf(f, ",\n");
    json_write_key(f, "numlock_on", indent); json_write_bool(f, k->numlock_on); fprintf(f, ",\n");
    json_write_key(f, "capslock_ctrl", indent); json_write_bool(f, k->capslock_ctrl); fprintf(f, ",\n");
    json_write_key(f, "compose_key", indent); json_write_bool(f, k->compose_key); fprintf(f, ",\n");
    json_write_key(f, "scroll_tick_lines", indent); json_write_int(f, k->scroll_tick_lines);
}
static void save_mouse(FILE *f, const MouseSettings *m, int indent) {
    json_write_key(f, "speed", indent); json_write_double(f, m->speed); fprintf(f, ",\n");
    json_write_key(f, "acceleration", indent); json_write_double(f, m->acceleration); fprintf(f, ",\n");
    json_write_key(f, "left_handed", indent); json_write_bool(f, m->left_handed); fprintf(f, ",\n");
    json_write_key(f, "double_click_time", indent); json_write_int(f, m->double_click_time); fprintf(f, ",\n");
    json_write_key(f, "natural_scroll", indent); json_write_bool(f, m->natural_scroll); fprintf(f, ",\n");
    json_write_key(f, "scroll_lines", indent); json_write_int(f, m->scroll_lines);
}
static void save_display(FILE *f, const DisplaySettings *d, int indent) {
    json_write_key(f, "width", indent); json_write_int(f, d->width); fprintf(f, ",\n");
    json_write_key(f, "height", indent); json_write_int(f, d->height); fprintf(f, ",\n");
    json_write_key(f, "refresh_rate", indent); json_write_int(f, d->refresh_rate); fprintf(f, ",\n");
    json_write_key(f, "scale_factor", indent); json_write_double(f, d->scale_factor); fprintf(f, ",\n");
    json_write_key(f, "fractional_scaling", indent); json_write_bool(f, d->fractional_scaling); fprintf(f, ",\n");
    json_write_key(f, "primary_monitor", indent); json_write_int(f, d->primary_monitor); fprintf(f, ",\n");
    json_write_key(f, "mirror_displays", indent); json_write_bool(f, d->mirror_displays);
}
static void save_a11y(FILE *f, const A11ySettings *a, int indent) {
    json_write_key(f, "high_contrast", indent); json_write_bool(f, a->high_contrast); fprintf(f, ",\n");
    json_write_key(f, "screen_reader", indent); json_write_bool(f, a->screen_reader); fprintf(f, ",\n");
    json_write_key(f, "screen_magnifier", indent); json_write_bool(f, a->screen_magnifier); fprintf(f, ",\n");
    json_write_key(f, "sticky_keys", indent); json_write_bool(f, a->sticky_keys); fprintf(f, ",\n");
    json_write_key(f, "slow_keys", indent); json_write_bool(f, a->slow_keys); fprintf(f, ",\n");
    json_write_key(f, "bounce_keys", indent); json_write_bool(f, a->bounce_keys); fprintf(f, ",\n");
    json_write_key(f, "mouse_keys", indent); json_write_bool(f, a->mouse_keys); fprintf(f, ",\n");
    json_write_key(f, "font_scale", indent); json_write_double(f, a->font_scale); fprintf(f, ",\n");
    json_write_key(f, "reduce_animations", indent); json_write_bool(f, a->reduce_animations);
}
static void save_privacy(FILE *f, const PrivacySettings *p, int indent) {
    json_write_key(f, "remember_recent", indent); json_write_bool(f, p->remember_recent); fprintf(f, ",\n");
    json_write_key(f, "remember_history", indent); json_write_bool(f, p->remember_history); fprintf(f, ",\n");
    json_write_key(f, "telemetry", indent); json_write_bool(f, p->telemetry); fprintf(f, ",\n");
    json_write_key(f, "location_services", indent); json_write_bool(f, p->location_services); fprintf(f, ",\n");
    json_write_key(f, "history_retention_days", indent); json_write_int(f, p->history_retention_days); fprintf(f, ",\n");
    json_write_key(f, "auto_clear_temp", indent); json_write_bool(f, p->auto_clear_temp);
}

/* Save all settings to file */
int wubu_settings_save(void) {
    if (!ensure_config_dir()) return -1;
    FILE *f = fopen(wubu_settings_path(), "w");
    if (!f) return -1;
    g_settings.last_modified = (uint64_t)time(NULL);
    fprintf(f, "{\n");
    fprintf(f, "%*s\"version\": ", 2, ""); json_write_uint(f, g_settings.version); fprintf(f, ",\n");
    fprintf(f, "%*s\"last_modified\": ", 2, ""); json_write_ulong(f, g_settings.last_modified); fprintf(f, ",\n");
    fprintf(f, "%*s\"theme\": {\n", 2, ""); save_theme(f, &g_settings.theme, 4); fprintf(f, "\n%*s},\n", 2, "");
    fprintf(f, "%*s\"fonts\": {\n", 2, ""); save_fonts(f, &g_settings.fonts, 4); fprintf(f, "\n%*s},\n", 2, "");
    fprintf(f, "%*s\"keyboard\": {\n", 2, ""); save_keyboard(f, &g_settings.keyboard, 4); fprintf(f, "\n%*s},\n", 2, "");
    fprintf(f, "%*s\"mouse\": {\n", 2, ""); save_mouse(f, &g_settings.mouse, 4); fprintf(f, "\n%*s},\n", 2, "");
    fprintf(f, "%*s\"display\": {\n", 2, ""); save_display(f, &g_settings.display, 4); fprintf(f, "\n%*s},\n", 2, "");
    fprintf(f, "%*s\"a11y\": {\n", 2, ""); save_a11y(f, &g_settings.a11y, 4); fprintf(f, "\n%*s},\n", 2, "");
    fprintf(f, "%*s\"privacy\": {\n", 2, ""); save_privacy(f, &g_settings.privacy, 4); fprintf(f, "\n%*s}\n", 2, "");
    fprintf(f, "}\n");
    fclose(f);
    g_settings_dirty = false;
    return 0;
}

/* -- Load Helpers ------------------------------------------------- */

static void load_theme(JsonToken *obj, ThemeSettings *t) {
    JsonToken *v;
    if ((v = json_find(obj, "theme_id")) && v->type == JSON_TYPE_NUMBER) t->theme_id = v->int_value;
    if ((v = json_find(obj, "wallpaper_path")) && v->type == JSON_TYPE_STRING) strncpy(t->wallpaper_path, v->str_value, sizeof(t->wallpaper_path)-1);
    if ((v = json_find(obj, "wallpaper_mode")) && v->type == JSON_TYPE_NUMBER) t->wallpaper_mode = v->int_value;
    if ((v = json_find(obj, "use_custom_colors")) && v->type == JSON_TYPE_BOOL) t->use_custom_colors = v->bool_value;
    if ((v = json_find(obj, "icon_layout_count")) && v->type == JSON_TYPE_NUMBER) t->icon_layout_count = v->int_value;
    for (int i = 0; i < WUBU_ICON_LAYOUT_MAX; i++) {
        char key[32]; snprintf(key, sizeof(key), "icon_layout_%d", i);
        JsonToken *e = json_find(obj, key);
        if (e && e->type == JSON_TYPE_OBJECT) {
            JsonToken *n  = json_find(e, "name");
            JsonToken *gx = json_find(e, "grid_x");
            JsonToken *gy = json_find(e, "grid_y");
            JsonToken *al = json_find(e, "alive");
            if (n  && n->type  == JSON_TYPE_STRING) strncpy(t->icon_layout[i].name, n->str_value, sizeof(t->icon_layout[i].name)-1);
            if (gx && gx->type == JSON_TYPE_NUMBER) t->icon_layout[i].grid_x = gx->int_value;
            if (gy && gy->type == JSON_TYPE_NUMBER) t->icon_layout[i].grid_y = gy->int_value;
            if (al && al->type == JSON_TYPE_BOOL)   t->icon_layout[i].alive  = al->bool_value;
        }
    }
}
static void load_fonts(JsonToken *obj, FontSettings *f) {
    JsonToken *v;
    if ((v = json_find(obj, "font_family")) && v->type == JSON_TYPE_STRING) strncpy(f->font_family, v->str_value, sizeof(f->font_family)-1);
    if ((v = json_find(obj, "font_size")) && v->type == JSON_TYPE_NUMBER) f->font_size = v->int_value;
    if ((v = json_find(obj, "antialiasing")) && v->type == JSON_TYPE_BOOL) f->antialiasing = v->bool_value;
    if ((v = json_find(obj, "hinting")) && v->type == JSON_TYPE_BOOL) f->hinting = v->bool_value;
    if ((v = json_find(obj, "dpi")) && v->type == JSON_TYPE_NUMBER) f->dpi = v->int_value;
    if ((v = json_find(obj, "monospace_family")) && v->type == JSON_TYPE_STRING) strncpy(f->monospace_family, v->str_value, sizeof(f->monospace_family)-1);
    if ((v = json_find(obj, "monospace_size")) && v->type == JSON_TYPE_NUMBER) f->monospace_size = v->int_value;
}
static void load_keyboard(JsonToken *obj, KeyboardSettings *k) {
    JsonToken *v;
    if ((v = json_find(obj, "repeat_delay")) && v->type == JSON_TYPE_NUMBER) k->repeat_delay = v->int_value;
    if ((v = json_find(obj, "repeat_rate")) && v->type == JSON_TYPE_NUMBER) k->repeat_rate = v->int_value;
    if ((v = json_find(obj, "layout")) && v->type == JSON_TYPE_STRING) strncpy(k->layout, v->str_value, sizeof(k->layout)-1);
    if ((v = json_find(obj, "variant")) && v->type == JSON_TYPE_STRING) strncpy(k->variant, v->str_value, sizeof(k->variant)-1);
    if ((v = json_find(obj, "numlock_on")) && v->type == JSON_TYPE_BOOL) k->numlock_on = v->bool_value;
    if ((v = json_find(obj, "capslock_ctrl")) && v->type == JSON_TYPE_BOOL) k->capslock_ctrl = v->bool_value;
    if ((v = json_find(obj, "compose_key")) && v->type == JSON_TYPE_BOOL) k->compose_key = v->bool_value;
    if ((v = json_find(obj, "scroll_tick_lines")) && v->type == JSON_TYPE_NUMBER) k->scroll_tick_lines = v->int_value;
}
static void load_mouse(JsonToken *obj, MouseSettings *m) {
    JsonToken *v;
    if ((v = json_find(obj, "speed")) && v->type == JSON_TYPE_NUMBER) m->speed = (double)v->int_value;
    if ((v = json_find(obj, "acceleration")) && v->type == JSON_TYPE_NUMBER) m->acceleration = (double)v->int_value;
    if ((v = json_find(obj, "left_handed")) && v->type == JSON_TYPE_BOOL) m->left_handed = v->bool_value;
    if ((v = json_find(obj, "double_click_time")) && v->type == JSON_TYPE_NUMBER) m->double_click_time = v->int_value;
    if ((v = json_find(obj, "natural_scroll")) && v->type == JSON_TYPE_BOOL) m->natural_scroll = v->bool_value;
    if ((v = json_find(obj, "scroll_lines")) && v->type == JSON_TYPE_NUMBER) m->scroll_lines = v->int_value;
}
static void load_display(JsonToken *obj, DisplaySettings *d) {
    JsonToken *v;
    if ((v = json_find(obj, "width")) && v->type == JSON_TYPE_NUMBER) d->width = v->int_value;
    if ((v = json_find(obj, "height")) && v->type == JSON_TYPE_NUMBER) d->height = v->int_value;
    if ((v = json_find(obj, "refresh_rate")) && v->type == JSON_TYPE_NUMBER) d->refresh_rate = v->int_value;
    if ((v = json_find(obj, "scale_factor")) && v->type == JSON_TYPE_NUMBER) d->scale_factor = (double)v->int_value;
    if ((v = json_find(obj, "fractional_scaling")) && v->type == JSON_TYPE_BOOL) d->fractional_scaling = v->bool_value;
    if ((v = json_find(obj, "primary_monitor")) && v->type == JSON_TYPE_NUMBER) d->primary_monitor = v->int_value;
    if ((v = json_find(obj, "mirror_displays")) && v->type == JSON_TYPE_BOOL) d->mirror_displays = v->bool_value;
}
static void load_a11y(JsonToken *obj, A11ySettings *a) {
    JsonToken *v;
    if ((v = json_find(obj, "high_contrast")) && v->type == JSON_TYPE_BOOL) a->high_contrast = v->bool_value;
    if ((v = json_find(obj, "screen_reader")) && v->type == JSON_TYPE_BOOL) a->screen_reader = v->bool_value;
    if ((v = json_find(obj, "screen_magnifier")) && v->type == JSON_TYPE_BOOL) a->screen_magnifier = v->bool_value;
    if ((v = json_find(obj, "sticky_keys")) && v->type == JSON_TYPE_BOOL) a->sticky_keys = v->bool_value;
    if ((v = json_find(obj, "slow_keys")) && v->type == JSON_TYPE_BOOL) a->slow_keys = v->bool_value;
    if ((v = json_find(obj, "bounce_keys")) && v->type == JSON_TYPE_BOOL) a->bounce_keys = v->bool_value;
    if ((v = json_find(obj, "mouse_keys")) && v->type == JSON_TYPE_BOOL) a->mouse_keys = v->bool_value;
    if ((v = json_find(obj, "font_scale")) && v->type == JSON_TYPE_NUMBER) a->font_scale = (double)v->int_value;
    if ((v = json_find(obj, "reduce_animations")) && v->type == JSON_TYPE_BOOL) a->reduce_animations = v->bool_value;
}
static void load_privacy(JsonToken *obj, PrivacySettings *p) {
    JsonToken *v;
    if ((v = json_find(obj, "remember_recent")) && v->type == JSON_TYPE_BOOL) p->remember_recent = v->bool_value;
    if ((v = json_find(obj, "remember_history")) && v->type == JSON_TYPE_BOOL) p->remember_history = v->bool_value;
    if ((v = json_find(obj, "telemetry")) && v->type == JSON_TYPE_BOOL) p->telemetry = v->bool_value;
    if ((v = json_find(obj, "location_services")) && v->type == JSON_TYPE_BOOL) p->location_services = v->bool_value;
    if ((v = json_find(obj, "history_retention_days")) && v->type == JSON_TYPE_NUMBER) p->history_retention_days = v->int_value;
    if ((v = json_find(obj, "auto_clear_temp")) && v->type == JSON_TYPE_BOOL) p->auto_clear_temp = v->bool_value;
}

/* Load settings from file */
int wubu_settings_load(void) {
    const char *path = wubu_settings_path();
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f); buf[len] = '\0'; fclose(f);
    const char *p = buf;
    JsonToken *root = json_parse_value(&p);
    int ret = 0;
    if (root && !g_json_parse_error) {
        JsonToken *v;
        if ((v = json_find(root, "version")) && v->type == JSON_TYPE_NUMBER) g_settings.version = v->int_value;
        if ((v = json_find(root, "last_modified")) && v->type == JSON_TYPE_NUMBER) g_settings.last_modified = v->int_value;
        if ((v = json_find(root, "theme"))) load_theme(v, &g_settings.theme);
        if ((v = json_find(root, "fonts"))) load_fonts(v, &g_settings.fonts);
        if ((v = json_find(root, "keyboard"))) load_keyboard(v, &g_settings.keyboard);
        if ((v = json_find(root, "mouse"))) load_mouse(v, &g_settings.mouse);
        if ((v = json_find(root, "display"))) load_display(v, &g_settings.display);
        if ((v = json_find(root, "a11y"))) load_a11y(v, &g_settings.a11y);
        if ((v = json_find(root, "privacy"))) load_privacy(v, &g_settings.privacy);
    } else {
        ret = -1;
    }
    json_free(root);
    free(buf);
    return ret;
}

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
