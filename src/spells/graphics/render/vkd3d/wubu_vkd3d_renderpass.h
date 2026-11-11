/*
 * wubu_vkd3d_renderpass.h -- VKD3D slice 10: the render pass.
 *
 * D3D12 (12_1+) describes a render pass explicitly: for every attachment, what
 * happens to its contents at the START (load) and at the END (store).  Vulkan
 * splits the same idea across VkAttachmentDescription (loadOp/storeOp) and
 * VkAttachmentReference (the layout used while it is bound).
 *
 * The mapping, and the rules that are easy to get wrong:
 *
 *   D3D12 beginning access   PRESERVE -> LOAD_OP_LOAD
 *                            CLEAR    -> LOAD_OP_CLEAR
 *                            NO_ACCESS -> LOAD_OP_DONT_CARE
 *   D3D12 ending access      PRESERVE -> STORE_OP_STORE
 *                            DISCARD  -> STORE_OP_DONT_CARE
 *
 *   * NO_ACCESS maps to DONT_CARE on BOTH sides.  A translation that maps it to
 *     LOAD would read stale contents; one that maps it to STORE would keep them
 *     -- and D3D12's contract is that the contents are UNDEFINED afterwards.
 *   * LOAD_OP_LOAD requires a real initial layout.  An UNDEFINED initial layout
 *     with LOAD is a contradiction Vulkan rejects, because loading implies the
 *     contents already exist in that layout.
 *   * A depth-stencil attachment index must be within the attachment list.
 *   * sample counts must be a power of two (1,2,4,8,16): Vulkan validates this
 *     and D3D12 only ever uses those values.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_RENDERPASS_H
#define WUBU_VKD3D_RENDERPASS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#define VKD3D_MAX_ATTACHMENTS 8

typedef enum {
    VKD3D_BEGIN_PRESERVE = 0,
    VKD3D_BEGIN_CLEAR = 1,
    VKD3D_BEGIN_NO_ACCESS = 2
} wubu_vkd3d_begin_access_t;

typedef enum {
    VKD3D_END_PRESERVE = 0,
    VKD3D_END_DISCARD = 1
} wubu_vkd3d_end_access_t;

typedef struct {
    VkFormat               format;
    uint32_t               samples;          /* 1,2,4,8,16 */
    wubu_vkd3d_begin_access_t begin;
    wubu_vkd3d_end_access_t   end;
    VkImageLayout          initial_layout;
    VkImageLayout          final_layout;
} wubu_vkd3d_attachment_t;

typedef struct {
    uint32_t                attachment_count;
    wubu_vkd3d_attachment_t attachments[VKD3D_MAX_ATTACHMENTS];
    bool                    has_depth_stencil;
    uint32_t                depth_stencil_attachment;   /* index, or 0xFF */
    VkPipelineBindPoint     bind_point;
} wubu_vkd3d_renderpass_desc_t;

/* NULL when legal, else a static reason. */
const char *wubu_vkd3d_renderpass_validate(const wubu_vkd3d_renderpass_desc_t *d);

/* Translate the attachments.  Returns the count, or -1 on an invalid desc. */
int wubu_vkd3d_renderpass_attachments(const wubu_vkd3d_renderpass_desc_t *d,
                                      VkAttachmentDescription *out,
                                      uint32_t max);

/* Translate the color attachment references (everything except the
 * depth-stencil attachment, in order).  Returns the count, or -1. */
int wubu_vkd3d_renderpass_color_refs(const wubu_vkd3d_renderpass_desc_t *d,
                                     VkAttachmentReference *out,
                                     uint32_t max);

/* The depth-stencil reference, or -1 when there is none. */
int wubu_vkd3d_renderpass_depth_ref(const wubu_vkd3d_renderpass_desc_t *d,
                                    VkAttachmentReference *out);

#endif /* WUBU_VKD3D_RENDERPASS_H */
