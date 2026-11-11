/*
 * wubu_vkd3d_renderpass.c -- VKD3D slice 10: the render pass.
 *
 * See wubu_vkd3d_renderpass.h.  Every rule here is one Vulkan or D3D12 rejects,
 * and both rejections are opaque.
 */
#include "wubu_vkd3d_renderpass.h"

#include <string.h>

static bool is_sample_count(uint32_t n)
{
    return n == 1 || n == 2 || n == 4 || n == 8 || n == 16;
}

const char *wubu_vkd3d_renderpass_validate(const wubu_vkd3d_renderpass_desc_t *d)
{
    if (!d) return "no render pass description";
    if (d->attachment_count == 0) return "a render pass needs an attachment";
    if (d->attachment_count > VKD3D_MAX_ATTACHMENTS)
        return "too many attachments";
    if (d->has_depth_stencil && d->depth_stencil_attachment >= d->attachment_count)
        return "the depth-stencil attachment index is out of range";

    for (uint32_t i = 0; i < d->attachment_count; i++) {
        const wubu_vkd3d_attachment_t *a = &d->attachments[i];
        if (!is_sample_count(a->samples))
            return "a sample count must be a power of two (1,2,4,8,16)";
        /* Loading implies the contents already exist, so the initial layout
         * cannot be UNDEFINED -- that is a contradiction Vulkan rejects and
         * D3D12's PRESERVE would be lying about. */
        if (a->begin == VKD3D_BEGIN_PRESERVE &&
            a->initial_layout == VK_IMAGE_LAYOUT_UNDEFINED)
            return "a PRESERVE (load) attachment needs a real initial layout";
        if (a->begin == VKD3D_BEGIN_NO_ACCESS &&
            a->initial_layout == VK_IMAGE_LAYOUT_UNDEFINED)
            return "a NO_ACCESS attachment still needs a valid initial layout";
    }
    return NULL;
}

int wubu_vkd3d_renderpass_attachments(const wubu_vkd3d_renderpass_desc_t *d,
                                      VkAttachmentDescription *out,
                                      uint32_t max)
{
    if (wubu_vkd3d_renderpass_validate(d) != NULL) return -1;
    if (!out || max < d->attachment_count) return -1;

    for (uint32_t i = 0; i < d->attachment_count; i++) {
        const wubu_vkd3d_attachment_t *a = &d->attachments[i];
        VkAttachmentDescription *o = &out[i];
        memset(o, 0, sizeof(*o));
        o->flags = 0;
        o->format = a->format;
        o->samples = (VkSampleCountFlagBits)a->samples;
        /* NO_ACCESS -> DONT_CARE on the load side: the contents are undefined
         * afterwards, so loading them would read garbage. */
        o->loadOp = (a->begin == VKD3D_BEGIN_PRESERVE) ? VK_ATTACHMENT_LOAD_OP_LOAD
                   : (a->begin == VKD3D_BEGIN_CLEAR)   ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                       : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        o->storeOp = (a->end == VKD3D_END_PRESERVE) ? VK_ATTACHMENT_STORE_OP_STORE
                                                    : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        o->stencilLoadOp = o->loadOp;
        o->stencilStoreOp = o->storeOp;
        o->initialLayout = a->initial_layout;
        o->finalLayout = a->final_layout;
    }
    return (int)d->attachment_count;
}

int wubu_vkd3d_renderpass_color_refs(const wubu_vkd3d_renderpass_desc_t *d,
                                     VkAttachmentReference *out,
                                     uint32_t max)
{
    if (wubu_vkd3d_renderpass_validate(d) != NULL) return -1;

    uint32_t n = 0;
    for (uint32_t i = 0; i < d->attachment_count; i++) {
        if (d->has_depth_stencil && i == d->depth_stencil_attachment) continue;
        if (!out || n >= max) return -1;
        out[n].attachment = i;
        out[n].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        n++;
    }
    return (int)n;
}

int wubu_vkd3d_renderpass_depth_ref(const wubu_vkd3d_renderpass_desc_t *d,
                                    VkAttachmentReference *out)
{
    if (wubu_vkd3d_renderpass_validate(d) != NULL) return -1;
    if (!out) return -1;
    if (!d->has_depth_stencil) return -1;
    out->attachment = d->depth_stencil_attachment;
    out->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return (int)d->depth_stencil_attachment;
}
