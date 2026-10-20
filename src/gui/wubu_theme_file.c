/*
 * wubu_theme_file.c -- parse a single-source-of-truth theme file.
 * See wubu_theme_file.h. Pure parser; no allocation.
 */
#include "wubu_theme_file.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define MAX_KEYS 64

typedef struct { const char *name; size_t off; } ThemeKey;

static const ThemeKey g_keys[] = {
    { "desktop_bg",             offsetof(WubuThemeColors, desktop_bg) },
    { "win_face",               offsetof(WubuThemeColors, win_face) },
    { "win_title_active",       offsetof(WubuThemeColors, win_title_active) },
    { "win_title_inactive",     offsetof(WubuThemeColors, win_title_inactive) },
    { "win_title_text",         offsetof(WubuThemeColors, win_title_text) },
    { "win_title_text_ina",     offsetof(WubuThemeColors, win_title_text_ina) },
    { "border_light",           offsetof(WubuThemeColors, border_light) },
    { "border_face",            offsetof(WubuThemeColors, border_face) },
    { "border_dark",            offsetof(WubuThemeColors, border_dark) },
    { "border_darkest",         offsetof(WubuThemeColors, border_darkest) },
    { "btn_face",               offsetof(WubuThemeColors, btn_face) },
    { "btn_hover",              offsetof(WubuThemeColors, btn_hover) },
    { "btn_pressed",            offsetof(WubuThemeColors, btn_pressed) },
    { "btn_text",               offsetof(WubuThemeColors, btn_text) },
    { "taskbar_bg",             offsetof(WubuThemeColors, taskbar_bg) },
    { "taskbar_border",         offsetof(WubuThemeColors, taskbar_border) },
    { "start_btn_face",         offsetof(WubuThemeColors, start_btn_face) },
    { "start_btn_text",         offsetof(WubuThemeColors, start_btn_text) },
    { "startmenu_bg",           offsetof(WubuThemeColors, startmenu_bg) },
    { "startmenu_sidebar",      offsetof(WubuThemeColors, startmenu_sidebar) },
    { "startmenu_hover",        offsetof(WubuThemeColors, startmenu_hover) },
    { "startmenu_text",         offsetof(WubuThemeColors, startmenu_text) },
    { "select_bg",              offsetof(WubuThemeColors, select_bg) },
    { "select_text",            offsetof(WubuThemeColors, select_text) },
    { "win_shadow",             offsetof(WubuThemeColors, win_shadow) },
    { "scroll_track",           offsetof(WubuThemeColors, scroll_track) },
    { "scroll_thumb",           offsetof(WubuThemeColors, scroll_thumb) },
    { "icon_text",              offsetof(WubuThemeColors, icon_text) },
    { "icon_bg",                offsetof(WubuThemeColors, icon_bg) },
    { "icon_border",            offsetof(WubuThemeColors, icon_border) },
};
#define NKEYS ((int)(sizeof g_keys / sizeof g_keys[0]))

const char *wubu_theme_key_name(int index) {
    return (index >= 0 && index < NKEYS) ? g_keys[index].name : NULL;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parse exactly 6 hex digits, optional leading '#'. */
static int parse_color(const char *s, uint32_t *out) {
    if (*s == '#') s++;
    uint32_t v = 0;
    for (int i = 0; i < 6; i++) {
        int d = hexval(s[i]);
        if (d < 0) return -1;
        v = (v << 4) | (uint32_t)d;
    }
    if (s[6] != '\0') return -1;      /* trailing junk */
    *out = v;
    return 0;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = '\0';
    return s;
}

int wubu_theme_parse(const char *text, WubuThemeColors *out) {
    if (!text || !out) return -1;
    char line[256];
    const char *p = text;
    int applied = 0;

    while (*p) {
        size_t n = 0;
        while (*p && *p != '\n' && n < sizeof line - 1) line[n++] = *p++;
        if (*p == '\n') p++;
        line[n] = '\0';

        char *s = trim(line);
        if (*s == '\0' || *s == '#') continue;     /* blank or comment */

        char *eq = strchr(s, '=');
        if (!eq) return -1;
        *eq = '\0';
        char *key = trim(s);
        char *val = trim(eq + 1);

        uint32_t rgb;
        if (parse_color(val, &rgb) != 0) return -1;

        int found = 0;
        for (int i = 0; i < NKEYS; i++) {
            if (strcmp(key, g_keys[i].name) == 0) {
                *(uint32_t *)((char *)out + g_keys[i].off) = rgb;
                found = 1;
                applied++;
                break;
            }
        }
        if (!found) return -1;                     /* unknown key is an error */
    }
    return applied;
}

int wubu_theme_load_file(const char *path, WubuThemeColors *out) {
    if (!path || !out) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -2;
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    int too_big = !feof(f) && n == sizeof buf - 1;
    fclose(f);
    if (too_big) return -2;
    buf[n] = '\0';
    return wubu_theme_parse(buf, out);
}