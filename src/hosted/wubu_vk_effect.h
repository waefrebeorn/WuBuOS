/*
 * wubu_vk_effect.h -- the .WET surface effects as a Vulkan compute pass.
 *
 * The effect engine (src/runtime/wubu_effect.c) is pure C11 so the CPU can run
 * it anywhere.  This is the SAME engine on the GPU: the shader in
 * wubu_vk_effect.comp mirrors the integer formulas line for line, so the GPU
 * output is byte-identical to the CPU output -- and test_vk_effect proves it.
 *
 * Built entirely on the public wubu_vulkan.h API (instance, device, allocator,
 * descriptor sets, dispatch); there is no second Vulkan setup here.
 */
#ifndef WUBU_VK_EFFECT_H
#define WUBU_VK_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

#include "wubu_effect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WubuVkEffect WubuVkEffect;

/* Create the context (instance, device, pipeline).  Returns NULL when this
 * host has no usable Vulkan device -- the CPU path is then the fallback. */
WubuVkEffect *wubu_vk_effect_new(const char *spirv_path);

void wubu_vk_effect_free(WubuVkEffect *e);

/* A short description of the device the passes run on, or NULL.  Used by the
 * gate to prove the pass really ran on a GPU rather than being skipped. */
const char *wubu_vk_effect_device_name(WubuVkEffect *e);

/* True when a GPU context exists. */
bool wubu_vk_effect_available(WubuVkEffect *e);

/*
 * Apply ONE pass to a w*h surface of 0xRRGGBB pixels on the GPU.
 * Returns 0 on success, -1 with *err set to a static message on failure.
 * `px` must hold w*h entries.
 */
int wubu_vk_effect_apply(WubuVkEffect *e, uint32_t *px, int w, int h,
                         WubuEffectKind kind, uint32_t color, int strength,
                         const char **err);

/*
 * Apply a whole stack, in order, on the GPU.  Each pass is one dispatch over
 * the same buffer; passes are sequential, so ordering is preserved exactly.
 * Returns 0 on success, -1 with *err set when the GPU could not run it (the
 * caller should fall back to wubu_effect_apply).
 */
int wubu_vk_effect_apply_stack(WubuVkEffect *e, const WubuEffectStack *s,
                               uint32_t *px, int w, int h, const char **err);

/*
 * The desktop's effect path: apply a stack on the GPU when this host has
 * Vulkan, otherwise leave the surface alone and return 0.
 *
 * Returns 1 when the stack ran on the GPU, 0 when it did not (no device, no
 * shader, or a dispatch failure) -- the caller then runs the CPU engine.
 *
 * Declared weak in wm_render.c, which must keep linking without Vulkan; the
 * weak symbol is tested before it is called.
 */
int wubu_vk_effect_desktop_apply(const WubuEffectStack *s, uint32_t *px,
                                 int w, int h);

/* The device the desktop passes run on, or "none" -- for the boot log. */
const char *wubu_vk_effect_desktop_name(void);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_VK_EFFECT_H */
