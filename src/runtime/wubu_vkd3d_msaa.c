/*
 * wubu_vkd3d_msaa.c -- VKD3D slice 14: sample descriptions (MSAA).
 *
 * See wubu_vkd3d_msaa.h.
 */
#include "wubu_vkd3d_msaa.h"

#include <string.h>

const char *wubu_vkd3d_sample_desc_validate(const wubu_vkd3d_sample_desc_t *d)
{
    if (!d) return "no sample description";
    switch (d->count) {
    case 1: case 2: case 4: case 8: case 16:
        break;
    default:
        return "a sample count must be 1, 2, 4, 8 or 16";
    }
    /* At 1x D3D12 ignores Quality, and the only legal value is 0. */
    if (d->count == 1 && d->quality != 0)
        return "at 1x the quality level must be 0";
    return NULL;
}

bool wubu_vkd3d_msaa_vk(const wubu_vkd3d_sample_desc_t *d,
                        wubu_vkd3d_msaa_vk_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    const char *why = wubu_vkd3d_sample_desc_validate(d);
    if (why) {
        out->valid = false;
        out->reason = why;
        return false;
    }

    out->rasterization_samples = (VkSampleCountFlagBits)d->count;
    /* D3D12's Quality is a vendor-specific level; the standard translation is
     * Quality 0 -> sample shading OFF, Quality > 0 -> ON at 1.0. */
    out->sample_shading_enable = (d->quality > 0);
    out->min_sample_shading = out->sample_shading_enable ? 1.0f : 0.0f;
    out->needs_resolve = (d->count > 1);
    out->valid = true;
    return true;
}

bool wubu_vkd3d_needs_resolve(uint32_t count)
{
    return count > 1;
}

bool wubu_vkd3d_sample_counts_compatible(uint32_t pipeline_samples,
                                         uint32_t attachment_samples)
{
    /* They must be EQUAL: Vulkan rejects a pipeline whose rasterizationSamples
     * differs from the attachment's sampleCount.  There is no "at most". */
    return pipeline_samples == attachment_samples;
}

uint32_t wubu_vkd3d_max_color_samples(VkSampleCountFlags flags)
{
    if (flags & VK_SAMPLE_COUNT_64_BIT) return 64;
    if (flags & VK_SAMPLE_COUNT_32_BIT) return 32;
    if (flags & VK_SAMPLE_COUNT_16_BIT) return 16;
    if (flags & VK_SAMPLE_COUNT_8_BIT)  return 8;
    if (flags & VK_SAMPLE_COUNT_4_BIT)  return 4;
    if (flags & VK_SAMPLE_COUNT_2_BIT)  return 2;
    if (flags & VK_SAMPLE_COUNT_1_BIT)  return 1;
    return 0;
}
