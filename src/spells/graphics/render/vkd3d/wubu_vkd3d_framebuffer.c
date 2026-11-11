/*
 * wubu_vkd3d_framebuffer.c -- VKD3D slice 11: image views and the framebuffer.
 *
 * See wubu_vkd3d_framebuffer.h.
 */
#include "wubu_vkd3d_framebuffer.h"

#include <string.h>

bool wubu_vkd3d_aspect_flags(VkFormat format, VkImageAspectFlags *out)
{
    if (!out) return false;
    *out = 0;

    switch (format) {
    /* depth only */
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
        *out = VK_IMAGE_ASPECT_DEPTH_BIT;
        return true;
    /* stencil only */
    case VK_FORMAT_S8_UINT:
        *out = VK_IMAGE_ASPECT_STENCIL_BIT;
        return true;
    /* combined depth-stencil: BOTH aspects */
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        *out = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        return true;
    /* plane formats are multi-planar, not a single aspect */
    case VK_FORMAT_G8B8G8R8_422_UNORM:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
        return false;
    /* everything else is a colour format */
    default:
        /* An UNKNOWN format is not a colour format: guessing COLOR for a
         * format we do not recognise would silently mis-bind it. */
        if (format == VK_FORMAT_UNDEFINED) return false;
        if (format > VK_FORMAT_ASTC_12x12_SRGB_BLOCK) return false;
        *out = VK_IMAGE_ASPECT_COLOR_BIT;
        return true;
    }
}

const char *wubu_vkd3d_framebuffer_validate(const wubu_vkd3d_framebuffer_desc_t *d)
{
    if (!d) return "no framebuffer description";
    if (d->attachment_count == 0) return "a framebuffer needs an attachment";
    if (d->attachment_count > VKD3D_MAX_ATTACHMENTS)
        return "too many attachments";
    if (d->width == 0 || d->height == 0) return "a zero-sized framebuffer";
    if (d->layers == 0) return "a framebuffer needs at least one layer";

    for (uint32_t i = 0; i < d->attachment_count; i++) {
        const wubu_vkd3d_framebuffer_attachment_t *a = &d->attachments[i];
        if (a->view == VK_NULL_HANDLE) return "an attachment has no image view";
        if (a->format == VK_FORMAT_UNDEFINED) return "an attachment has no format";

        /* Vulkan requires every attachment to match the framebuffer's extent
         * and sample count.  D3D12 never checks this -- the driver's answer is
         * a validation message, not an error code, so a layer must. */
        if (a->width != d->width || a->height != d->height)
            return "an attachment's size does not match the framebuffer";
        if (a->samples != d->attachments[0].samples)
            return "an attachment's sample count does not match the framebuffer";

        /* A depth-stencil attachment must actually BE one: binding a colour
         * view as depth-stencil is silent corruption. */
        if (a->is_depth_stencil) {
            VkImageAspectFlags f;
            if (!wubu_vkd3d_aspect_flags(a->format, &f))
                return "a depth-stencil attachment has an unknown format";
            if (!(f & VK_IMAGE_ASPECT_DEPTH_BIT) && !(f & VK_IMAGE_ASPECT_STENCIL_BIT))
                return "a depth-stencil attachment is not a depth-stencil format";
        }
    }
    return NULL;
}

bool wubu_vkd3d_framebuffer_create_info(const wubu_vkd3d_framebuffer_desc_t *d,
                                        VkFramebufferCreateInfo *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (wubu_vkd3d_framebuffer_validate(d) != NULL) return false;

    out->sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    out->attachmentCount = d->attachment_count;
    /* The views are copied into a local array; a real caller would keep its
     * own storage, but the create info must point at a contiguous array. */
    static VkImageView views[VKD3D_MAX_ATTACHMENTS];
    for (uint32_t i = 0; i < d->attachment_count; i++)
        views[i] = d->attachments[i].view;
    out->pAttachments = views;
    out->width = d->width;
    out->height = d->height;
    out->layers = d->layers;
    return true;
}
