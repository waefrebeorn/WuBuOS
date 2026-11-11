/*
 * wubu_vkd3d_swapchain.c -- VKD3D slice 7: the swapchain / present path.
 *
 * See wubu_vkd3d_swapchain.h for the mapping table.  The one thing Vulkan
 * cannot express is a SyncInterval above 1, so the translation says so
 * explicitly instead of pretending FIFO is "every Nth frame".
 */
#include "wubu_vkd3d_swapchain.h"

#include <string.h>

const char *wubu_vkd3d_swapchain_validate(const wubu_vkd3d_swapchain_desc_t *d)
{
    if (!d) return "no swapchain description";
    if (d->width == 0 || d->height == 0) return "a zero-sized swapchain";
    if (d->buffer_count == 0) return "a swapchain needs at least one buffer";
    /* DXGI requires 2+ buffers for the FLIP models.  A single-buffer flip
     * chain is not a thing: there is nothing to flip TO. */
    if (d->effect != VKD3D_SWAP_EFFECT_DISCARD && d->buffer_count < 2)
        return "the flip swap effects need at least two buffers";
    if (d->effect != VKD3D_SWAP_EFFECT_DISCARD && d->effect != VKD3D_SWAP_EFFECT_FLIP_SEQUENTIAL
        && d->effect != VKD3D_SWAP_EFFECT_FLIP_DISCARD)
        return "an unknown swap effect";
    /* NOTE: there is deliberately NO check that max_frame_latency fits the
     * buffer count.  Vulkan may return MORE images than minImageCount asks
     * for, so the frame-latency requirement is met by RAISING the image count
     * (see wubu_vkd3d_swapchain_min_image_count), not by rejecting the desc. */
    if (d->sync_interval > 4)
        return "the sync interval is out of range (0..4)";
    return NULL;
}

uint32_t wubu_vkd3d_swapchain_min_image_count(const wubu_vkd3d_swapchain_desc_t *d)
{
    /* N frames in flight need N+1 images (one being presented/scanned out).
     * Three is the floor every real driver wants: fewer starves the queue. */
    uint32_t need = d->max_frame_latency + 1;
    return need < 3 ? 3 : need;
}

bool wubu_vkd3d_swapchain_vk(const wubu_vkd3d_swapchain_desc_t *d,
                             wubu_vkd3d_swapchain_vk_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (wubu_vkd3d_swapchain_validate(d) != NULL) return false;

    out->valid = true;
    out->needs_app_throttle = false;

    /* -- the present mode -- */
    if (d->sync_interval >= 1) {
        /* Vulkan has FIFO (sync every frame) and IMMEDIATE (sync none).  There
         * is no "sync every Nth", so interval > 1 is a frame limiter the app
         * must run on top of FIFO -- and we say so. */
        out->present_mode = VK_PRESENT_MODE_FIFO_KHR;
        out->needs_app_throttle = (d->sync_interval > 1);
        out->why = out->needs_app_throttle
                       ? "vsync: FIFO plus an app-level limiter (Vulkan has no sync interval above 1)"
                       : "vsync: FIFO";
    } else if (d->tearing_allowed) {
        out->present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        out->why = "no vsync, tearing permitted: IMMEDIATE";
    } else {
        out->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        out->why = "no vsync, tear-free, lowest latency: MAILBOX";
    }

    /* The swap effect is a HINT about the present mode, not a separate knob:
     * DISCARD is the legacy bitblt path and IMMEDIATE is its closest Vulkan
     * expression, while both FLIP modes are what FIFO/MAILBOX already are. */
    if (d->effect == VKD3D_SWAP_EFFECT_DISCARD && d->sync_interval >= 1) {
        out->why = "legacy bitblt swap effect: FIFO";
    }

    /* -- the create info -- */
    out->create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    out->create_info.minImageCount = wubu_vkd3d_swapchain_min_image_count(d);
    out->create_info.imageExtent.width = d->width;
    out->create_info.imageExtent.height = d->height;
    out->create_info.imageArrayLayers = 1;
    out->create_info.presentMode = out->present_mode;
    out->create_info.clipped = VK_TRUE;
    out->create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return true;
}

/* -- frame latency -------------------------------------------------- */

void wubu_vkd3d_frame_latency_init(wubu_vkd3d_frame_latency_t *f,
                                   uint32_t max_latency)
{
    if (!f) return;
    f->max_latency = max_latency;
    f->in_flight = 0;
}

bool wubu_vkd3d_frame_latency_can_present(const wubu_vkd3d_frame_latency_t *f)
{
    if (!f) return false;
    return f->in_flight < f->max_latency;
}

void wubu_vkd3d_frame_latency_submit(wubu_vkd3d_frame_latency_t *f)
{
    if (!f) return;
    if (f->in_flight < f->max_latency) f->in_flight++;
}

void wubu_vkd3d_frame_latency_complete(wubu_vkd3d_frame_latency_t *f)
{
    if (!f) return;
    if (f->in_flight > 0) f->in_flight--;
}

uint32_t wubu_vkd3d_frame_latency_in_flight(const wubu_vkd3d_frame_latency_t *f)
{
    return f ? f->in_flight : 0;
}
