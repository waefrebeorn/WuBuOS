/*
 * wubu_vkd3d_msaa.h -- VKD3D slice 14: sample descriptions (MSAA).
 *
 * D3D12 describes antialiasing as a SAMPLE_DESC: { Count, Quality }.  Vulkan
 * splits it across two places:
 *
 *   Count   -> VkPipelineMultisampleStateCreateInfo::rasterizationSamples
 *              AND the VkImage's sampleCount (the attachment) -- these two MUST
 *              agree or the render pass is invalid.
 *   Quality -> VkPipelineMultisampleStateCreateInfo::sampleShadingEnable /
 *              minSampleShading.  D3D12's Quality is a vendor-specific quality
 *              level; the standard translation is Quality 0 -> sample shading
 *              OFF, Quality > 0 -> ON with minSampleShading = 1.0.
 *
 * The rules that are easy to get wrong:
 *
 *   * Counts are 1,2,4,8,16 -- a power of two, never 3 or 6.
 *   * Count 1 must have Quality 0 (D3D12 ignores Quality at 1x, but the only
 *     valid value there is 0).
 *   * A MULTISAMPLED image needs an explicit RESOLVE before it can be sampled
 *     or presented.  D3D12 hides this behind ResolveSubresource; Vulkan wants a
 *     second, single-sampled attachment and VkRenderPass::pResolveAttachments.
 *   * The pipeline's rasterizationSamples must equal the attachment's
 *     sampleCount.  A mismatch is a validation error that some drivers only
 *     report at submit time.
 *   * sampleShadingEnable with minSampleShading > 0 costs performance and is
 *     NOT implied by Count > 1 -- the two are independent knobs.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_MSAA_H
#define WUBU_VKD3D_MSAA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

typedef struct {
    uint32_t count;      /* D3D12_SAMPLE_DESC::Count */
    uint32_t quality;    /* D3D12_SAMPLE_DESC::Quality */
} wubu_vkd3d_sample_desc_t;

typedef struct {
    VkSampleCountFlagBits rasterization_samples;
    bool                  sample_shading_enable;
    float                 min_sample_shading;
    bool                  needs_resolve;      /* Count > 1 */
    bool                  valid;
    const char           *reason;
} wubu_vkd3d_msaa_vk_t;

/* NULL when legal, else a static reason. */
const char *wubu_vkd3d_sample_desc_validate(const wubu_vkd3d_sample_desc_t *d);

bool wubu_vkd3d_msaa_vk(const wubu_vkd3d_sample_desc_t *d,
                        wubu_vkd3d_msaa_vk_t *out);

/* Does this sample count require an explicit resolve to a single-sampled
 * target? */
bool wubu_vkd3d_needs_resolve(uint32_t count);

/* May a pipeline with `pipeline_samples` draw into a render pass whose
 * attachments use `attachment_samples`?  They must be EQUAL. */
bool wubu_vkd3d_sample_counts_compatible(uint32_t pipeline_samples,
                                         uint32_t attachment_samples);

/* The largest sample count a device supports for a colour attachment. */
uint32_t wubu_vkd3d_max_color_samples(VkSampleCountFlags flags);

#endif /* WUBU_VKD3D_MSAA_H */
