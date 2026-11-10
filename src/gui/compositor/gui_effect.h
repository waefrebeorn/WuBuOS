/*
 * gui_effect.h -- a CPU effect stack over an RGBA surface.
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
#ifndef WUBU_GUI_EFFECT_H
#define WUBU_GUI_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GUI_EFFECT_NONE = 0,
    GUI_EFFECT_TINT,       /* blend a colour over the surface          */
    GUI_EFFECT_VIGNETTE,   /* darken toward the edges                  */
    GUI_EFFECT_GLOW,       /* lift bright pixels additively           */
    GUI_EFFECT_SCANLINE,   /* CRT rows: dim every other line           */
    GUI_EFFECT_COUNT
} GuiEffectKind;

typedef struct {
    GuiEffectKind kind;
    uint32_t      color;     /* 0xRRGGBB, used by TINT and GLOW */
    int           strength;  /* 0..100, clamped on add         */
} GuiEffect;

typedef struct GuiEffectStack GuiEffectStack;

/* ------------------------------------------------------------- lifecycle */

GuiEffectStack *gui_effect_stack_new(void);
void            gui_effect_stack_free(GuiEffectStack *s);

/* --------------------------------------------------------------- editing */

int               gui_effect_stack_count(const GuiEffectStack *s);
const GuiEffect   *gui_effect_stack_at(const GuiEffectStack *s, int i);
bool              gui_effect_stack_add(GuiEffectStack *s, GuiEffectKind kind,
                                       uint32_t color, int strength);
bool              gui_effect_stack_remove(GuiEffectStack *s, int index);
void              gui_effect_stack_clear(GuiEffectStack *s);

/* -------------------------------------------------------------- registry */

int         gui_effect_kind_count(void);
const char *gui_effect_name(GuiEffectKind kind);
const char *gui_effect_name_at(int i);
GuiEffectKind gui_effect_kind_at(int i);

/* ------------------------------------------------------------ application */

/* Apply the whole stack, in order, to a w*h surface of 0xRRGGBB pixels.
 * A NULL or empty stack is a no-op. */
void gui_effect_apply(const GuiEffectStack *s, uint32_t *px, int w, int h);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_GUI_EFFECT_H */
