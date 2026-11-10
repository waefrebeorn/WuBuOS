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

#ifdef __cplusplus
}
#endif

#endif /* WUBU_VK_EFFECT_H */
