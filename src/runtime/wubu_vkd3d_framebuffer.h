/*
 * wubu_vkd3d_framebuffer.h -- VKD3D slice 11: image views and the framebuffer.
 *
 * A framebuffer is the set of image views a render pass executes against, and
 * it is the last piece before a render pass can be created on a real device.
 *
 * Two mappings here, both silent-corruption shaped:
 *
 *   ASPECT FLAGS.  D3D12 says "depth-stencil view" and lets the format decide
 *   what it contains.  Vulkan requires the aspect to be named EXPLICITLY, and
 *   the format is what decides it:
 *       D32_SFLOAT / D16_UNORM            -> DEPTH
 *       S8_UINT                             -> STENCIL
 *       D24_UNORM_S8_UINT / D32_SFLOAT_S8  -> DEPTH | STENCIL
 *       anything else                      -> COLOR
 *   Asking for the wrong aspect does not error: it binds a view the driver
 *   interprets differently, and the depth test reads garbage.
 *
 *   FRAMEBUFFER CONSISTENCY.  Vulkan requires every attachment to have the
 *   SAME width, height, and sample count as the framebuffer.  D3D12 does not
 *   check this at all -- it is the app's job -- so a layer must check, because
 *   the driver's answer is a validation message, not an error code.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_FRAMEBUFFER_H
#define WUBU_VKD3D_FRAMEBUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "wubu_vkd3d_renderpass.h"

typedef struct {
    VkImageView view;
    VkFormat    format;
    uint32_t    width;
    uint32_t    height;
    uint32_t    samples;
    bool        is_depth_stencil;
} wubu_vkd3d_framebuffer_attachment_t;

typedef struct {
    uint32_t                          attachment_count;
    wubu_vkd3d_framebuffer_attachment_t attachments[VKD3D_MAX_ATTACHMENTS];
    uint32_t                          width;
    uint32_t                          height;
    uint32_t                          layers;
} wubu_vkd3d_framebuffer_desc_t;

/* The aspect flags a format's image view must be created with.  false when the
 * format is unknown. */
bool wubu_vkd3d_aspect_flags(VkFormat format, VkImageAspectFlags *out);

/* NULL when legal, else a static reason. */
const char *wubu_vkd3d_framebuffer_validate(const wubu_vkd3d_framebuffer_desc_t *d);

/* Translate to a VkFramebufferCreateInfo (the views are copied in order).
 * Returns false on an invalid desc. */
bool wubu_vkd3d_framebuffer_create_info(const wubu_vkd3d_framebuffer_desc_t *d,
                                        VkFramebufferCreateInfo *out);

#endif /* WUBU_VKD3D_FRAMEBUFFER_H */
