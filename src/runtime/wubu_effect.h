/*
 * wubu_effect.h -- a CPU effect stack over an RGBA surface.
 *
 * The compositor draws into an off-screen buffer (gui_dbuf) and flips it.  An
 * effect is a whole-surface pass applied to that buffer before the flip, so
 * the same mechanism serves a .wet applet, a window or the whole desktop.
 *
 * Pure integer C11, no libm: the passes run in the draw path, so they are
 * fixed-point and branch-light.  The stack is data -- a caller can build one
 * from a script, a theme file or the /n namespace without linking anything.
 *
 * Effects are ordered: they apply in stack order, so "tint then vignette" and
 * "vignette then tint" are different and both are meaningful.
 */
#ifndef WUBU_EFFECT_H
#define WUBU_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WUBU_EFFECT_NONE = 0,
    WUBU_EFFECT_TINT,       /* blend a colour over the surface          */
    WUBU_EFFECT_VIGNETTE,   /* darken toward the edges                  */
    WUBU_EFFECT_GLOW,       /* lift bright pixels additively           */
    WUBU_EFFECT_SCANLINE,   /* CRT rows: dim every other line           */
    WUBU_EFFECT_COUNT
} WubuEffectKind;

typedef struct {
    WubuEffectKind kind;
    uint32_t      color;     /* 0xRRGGBB, used by TINT and GLOW */
    int           strength;  /* 0..100, clamped on add         */
} WubuEffect;

typedef struct WubuEffectStack WubuEffectStack;

/* ------------------------------------------------------------- lifecycle */

WubuEffectStack *wubu_effect_stack_new(void);
void            wubu_effect_stack_free(WubuEffectStack *s);

/* --------------------------------------------------------------- editing */

int               wubu_effect_stack_count(const WubuEffectStack *s);
const WubuEffect   *wubu_effect_stack_at(const WubuEffectStack *s, int i);
bool              wubu_effect_stack_add(WubuEffectStack *s, WubuEffectKind kind,
                                       uint32_t color, int strength);
bool              wubu_effect_stack_remove(WubuEffectStack *s, int index);
void              wubu_effect_stack_clear(WubuEffectStack *s);

/* -------------------------------------------------------------- registry */

int         wubu_effect_kind_count(void);
const char *wubu_effect_name(WubuEffectKind kind);
const char *wubu_effect_name_at(int i);
WubuEffectKind wubu_effect_kind_at(int i);
WubuEffectKind wubu_effect_kind_from_name(const char *name);

/* ------------------------------------------------------------ application */

/* Apply the whole stack, in order, to a w*h surface of 0xRRGGBB pixels.
 * A NULL or empty stack is a no-op. */
void wubu_effect_apply(const WubuEffectStack *s, uint32_t *px, int w, int h);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_EFFECT_H */
