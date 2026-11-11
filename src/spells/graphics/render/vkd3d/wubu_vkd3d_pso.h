/*
 * wubu_vkd3d_pso.h -- VKD3D slice 4: pipeline state objects.
 *
 * A D3D12 PSO is ONE immutable object: root signature + shader bytecode +
 * blend + rasterizer + depth-stencil + input layout + topology + render
 * target formats + sample description.  There is no partial state -- you either
 * bind a whole PSO or nothing.
 *
 * Two rules make PSOs the expensive part of a D3D12 frame, and both are
 * modelled here:
 *
 *   1. IMMUTABILITY.  A PSO cannot be modified after creation.  The only way to
 *      "change" state is to create a different PSO.
 *   2. EXACT MATCH.  A cached PSO is only reusable when EVERY field matches.
 *      There is no fallback: a near-miss is a miss, not an approximation.
 *
 * Rule 2 is why the cache key must cover the whole state blob -- a missed field
 * silently produces a pipeline that renders with the wrong state, which shows
 * up as a subtle visual bug rather than an error.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_PSO_H
#define WUBU_VKD3D_PSO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#define VKD3D_PSO_INPUT_ELEMENTS 8
#define VKD3D_PSO_RTVS          8

/* Everything that defines a PSO.  Plain uint32_t fields on purpose: the key is
 * a hash of the serialised blob, and bitfields would make the padding
 * unspecified (so two identical descs could hash differently). */
typedef struct {
    uint32_t root_signature_hash;
    uint32_t vs_hash, ps_hash, gs_hash, hs_hash, ds_hash, cs_hash;

    /* blend */
    bool     blend_enable;
    uint32_t blend_src_color;    /* D3D12_BLEND */
    uint32_t blend_dst_color;
    uint32_t blend_op_color;     /* D3D12_BLEND_OP */
    uint32_t blend_src_alpha;
    uint32_t blend_dst_alpha;
    uint32_t blend_op_alpha;
    uint32_t render_target_write_mask;

    /* rasterizer */
    uint32_t fill_mode;          /* D3D12_FILL_MODE */
    uint32_t cull_mode;          /* D3D12_CULL_MODE */
    bool     front_counter_clockwise;
    int32_t  depth_bias;
    float    depth_bias_clamp;
    float    slope_scaled_depth_bias;
    bool     depth_clip_enable;

    /* depth stencil */
    bool     depth_enable;
    bool     depth_write_enable;
    uint32_t depth_func;         /* D3D12_COMPARISON_FUNC */
    bool     stencil_enable;
    uint32_t stencil_read_mask;
    uint32_t stencil_write_mask;

    /* input layout */
    uint32_t input_element_count;
    uint32_t input_strides[VKD3D_PSO_INPUT_ELEMENTS];
    uint32_t input_formats[VKD3D_PSO_INPUT_ELEMENTS];  /* DXGI_FORMAT */

    /* assembly + targets + sampling */
    uint32_t primitive_topology; /* D3D12_PRIMITIVE_TOPOLOGY_TYPE */
    uint32_t rtv_count;
    uint32_t rtv_formats[VKD3D_PSO_RTVS];
    uint32_t dsv_format;
    uint32_t sample_count;
    uint32_t sample_mask;
} wubu_vkd3d_pso_desc_t;

/* The cache key: a hash of the canonical serialisation of the whole desc.
 * Two descs with the same key are the same pipeline; any difference -- even a
 * field the current implementation does not consume -- yields a different key. */
uint32_t wubu_vkd3d_pso_key(const wubu_vkd3d_pso_desc_t *d);
const char *wubu_vkd3d_pso_key_hex(const wubu_vkd3d_pso_desc_t *d,
                                   char *out, size_t n);

/* A PSO cache.  Capacity is a revolver, not a constant. */
typedef struct wubu_vkd3d_pso_cache wubu_vkd3d_pso_cache_t;

wubu_vkd3d_pso_cache_t *wubu_vkd3d_pso_cache_create(uint32_t capacity);
void wubu_vkd3d_pso_cache_destroy(wubu_vkd3d_pso_cache_t *c);

/* Get the pipeline for this exact desc, creating it on a miss.
 * Returns a nonzero pipeline id, or 0 on error (NULL cache/desc, or the cache
 * is full).  *created is set to 1 when a new pipeline was made. */
uint32_t wubu_vkd3d_pso_get(wubu_vkd3d_pso_cache_t *c,
                            const wubu_vkd3d_pso_desc_t *d, int *created);

/* Release a pipeline (the app is done with it).  Returns -1 for an unknown
 * handle -- releasing twice is an app bug, not a no-op. */
int wubu_vkd3d_pso_release(wubu_vkd3d_pso_cache_t *c, uint32_t pipeline);

uint32_t wubu_vkd3d_pso_cache_size(const wubu_vkd3d_pso_cache_t *c);
uint32_t wubu_vkd3d_pso_cache_hits(const wubu_vkd3d_pso_cache_t *c);
uint32_t wubu_vkd3d_pso_cache_misses(const wubu_vkd3d_pso_cache_t *c);

/* -- the Vulkan translation ----------------------------------------- */

/* The mapped Vulkan state values.  Deliberately the VALUES, not the Vk*Info
 * structs: those need a render pass and a pipeline layout that this module
 * does not own.  What matters for correctness is that each D3D12 enum maps to
 * the right Vulkan enum -- a wrong mapping is a visual bug, not an error. */
typedef struct {
    VkPolygonMode    polygon_mode;
    VkCullModeFlags  cull_mode;
    VkFrontFace      front_face;
    VkCompareOp      depth_func;
    VkBlendFactor    src_color, dst_color, src_alpha, dst_alpha;
    VkBlendOp        op_color, op_alpha;
    VkPrimitiveTopology topology;
    bool             topology_valid;
    bool             valid;
    const char      *name;
} wubu_vkd3d_pso_vk_t;

bool wubu_vkd3d_pso_vk(const wubu_vkd3d_pso_desc_t *d, wubu_vkd3d_pso_vk_t *out);

#endif /* WUBU_VKD3D_PSO_H */
