/*
 * gui_effect.c -- the effect stack and its passes.
 *
 * See gui_effect.h for the model.  Everything here is integer: a pass runs per
 * pixel in the draw path, so there is no libm and no floating point.
 */
#include "../runtime/wubu_effect.h"

#include <stdlib.h>
#include <string.h>

#define GUI_EFFECT_MAX 8

struct WubuEffectStack {
    WubuEffect used[GUI_EFFECT_MAX];
    int       n;
};

/* -------------------------------------------------------------- registry */

static const char *const g_names[WUBU_EFFECT_COUNT] = {
    "none", "tint", "vignette", "glow", "scanline",
};

int         wubu_effect_kind_count(void) { return WUBU_EFFECT_COUNT; }

const char *wubu_effect_name(WubuEffectKind kind) {
    if (kind <= WUBU_EFFECT_NONE || kind >= WUBU_EFFECT_COUNT) return NULL;
    return g_names[kind];
}

const char *wubu_effect_name_at(int i) {
    if (i <= 0 || i >= WUBU_EFFECT_COUNT) return NULL;
    return g_names[i];
}

WubuEffectKind wubu_effect_kind_at(int i) {
    if (i < 0 || i >= WUBU_EFFECT_COUNT) return WUBU_EFFECT_NONE;
    return (WubuEffectKind)i;
}

/* Resolve a pass name ("tint", "vignette", "glow", "scanline") to its kind.
 * Returns WUBU_EFFECT_NONE when unknown, so a caller can tell "no such pass"
 * apart from "the none pass". */
WubuEffectKind wubu_effect_kind_from_name(const char *name) {
    if (!name) return WUBU_EFFECT_NONE;
    for (int i = WUBU_EFFECT_NONE + 1; i < WUBU_EFFECT_COUNT; i++)
        if (g_names[i] && strcmp(g_names[i], name) == 0) return (WubuEffectKind)i;
    return WUBU_EFFECT_NONE;
}

/* ------------------------------------------------------------- lifecycle */

WubuEffectStack *wubu_effect_stack_new(void) {
    return (WubuEffectStack *)calloc(1, sizeof(WubuEffectStack));
}

void wubu_effect_stack_free(WubuEffectStack *s) {
    free(s);
}

/* --------------------------------------------------------------- editing */

int             wubu_effect_stack_count(const WubuEffectStack *s) { return s ? s->n : 0; }

const WubuEffect *wubu_effect_stack_at(const WubuEffectStack *s, int i) {
    if (!s || i < 0 || i >= s->n) return NULL;
    return &s->used[i];
}

bool wubu_effect_stack_add(WubuEffectStack *s, WubuEffectKind kind,
                          uint32_t color, int strength) {
    if (!s || s->n >= GUI_EFFECT_MAX) return false;
    if (kind <= WUBU_EFFECT_NONE || kind >= WUBU_EFFECT_COUNT) return false;
    WubuEffect *e = &s->used[s->n];
    e->kind     = kind;
    e->color    = color & 0xFFFFFFu;
    e->strength = strength < 0 ? 0 : (strength > 100 ? 100 : strength);
    s->n++;
    return true;
}

bool wubu_effect_stack_remove(WubuEffectStack *s, int index) {
    if (!s || index < 0 || index >= s->n) return false;
    for (int i = index; i + 1 < s->n; i++)
        s->used[i] = s->used[i + 1];
    s->n--;
    return true;
}

void wubu_effect_stack_clear(WubuEffectStack *s) {
    if (s) s->n = 0;
}

/* ---------------------------------------------------------------- passes */

static inline uint32_t clamp8(int v) {
    return (uint32_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static inline uint32_t pack8(uint32_t r, uint32_t g, uint32_t b) {
    return (r << 16) | (g << 8) | b;
}

/* out = src + (target - src) * strength / 100, per channel. */
static void pass_tint(uint32_t *px, int n, uint32_t color, int strength) {
    if (strength <= 0) return;
    uint32_t tr = (color >> 16) & 0xFF, tg = (color >> 8) & 0xFF, tb = color & 0xFF;
    for (int i = 0; i < n; i++) {
        uint32_t p = px[i];
        uint32_t r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
        r = clamp8(r + (int)((tr - r) * strength) / 100);
        g = clamp8(g + (int)((tg - g) * strength) / 100);
        b = clamp8(b + (int)((tb - b) * strength) / 100);
        px[i] = pack8(r, g, b);
    }
}

/* Darken by a factor that grows with distance from the centre.  The centre is
 * untouched, so a vignette never changes where the eye is. */
static void pass_vignette(uint32_t *px, int w, int h, int strength) {
    if (strength <= 0 || w <= 0 || h <= 0) return;
    int cx = w / 2, cy = h / 2;
    int maxd = cx * cx + cy * cy;
    if (maxd <= 0) return;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int dx = x - cx, dy = y - cy;
            int d = dx * dx + dy * dy;
            /* factor = 100 - strength * d / maxd, in percent */
            int factor = 100 - strength * d / maxd;
            if (factor >= 100) continue;
            if (factor < 0) factor = 0;
            uint32_t p = px[y * w + x];
            uint32_t r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            px[y * w + x] = pack8(clamp8(r * factor / 100),
                                  clamp8(g * factor / 100),
                                  clamp8(b * factor / 100));
        }
    }
}

/* Lift a pixel toward a colour, scaled by how bright it already is: a glow
 * blooms highlights rather than washing the shadows. */
static void pass_glow(uint32_t *px, int n, uint32_t color, int strength) {
    if (strength <= 0) return;
    uint32_t tr = (color >> 16) & 0xFF, tg = (color >> 8) & 0xFF, tb = color & 0xFF;
    for (int i = 0; i < n; i++) {
        uint32_t p = px[i];
        uint32_t r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
        int lum = (r + g + b) / 3;
        int k = strength * lum / 255;   /* 0..strength */
        r = clamp8(r + tr * k / 100);
        g = clamp8(g + tg * k / 100);
        b = clamp8(b + tb * k / 100);
        px[i] = pack8(r, g, b);
    }
}

/* Dim every other row.  row 0 stays, so the effect never shifts content. */
static void pass_scanline(uint32_t *px, int w, int h, int strength) {
    if (strength <= 0 || w <= 0 || h <= 0) return;
    int factor = 100 - strength;
    for (int y = 1; y < h; y += 2) {
        uint32_t *row = px + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            uint32_t p = row[x];
            uint32_t r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            row[x] = pack8(clamp8(r * factor / 100),
                           clamp8(g * factor / 100),
                           clamp8(b * factor / 100));
        }
    }
}

/* ------------------------------------------------------------ application */

void wubu_effect_apply(const WubuEffectStack *s, uint32_t *px, int w, int h) {
    if (!s || !px || w <= 0 || h <= 0) return;
    int n = w * h;
    for (int i = 0; i < s->n; i++) {
        const WubuEffect *e = &s->used[i];
        switch (e->kind) {
        case WUBU_EFFECT_TINT:     pass_tint(px, n, e->color, e->strength);      break;
        case WUBU_EFFECT_VIGNETTE: pass_vignette(px, w, h, e->strength);         break;
        case WUBU_EFFECT_GLOW:     pass_glow(px, n, e->color, e->strength);      break;
        case WUBU_EFFECT_SCANLINE: pass_scanline(px, w, h, e->strength);         break;
        case WUBU_EFFECT_NONE:
        case WUBU_EFFECT_COUNT:
        default: break;
        }
    }
}
