/*
 * wubu_vkd3d_device.c -- VKD3D slice 5: the feature-level device.
 *
 * See wubu_vkd3d_device.h.  The device is created AT a level and never
 * downgrades: a feature level is a contract, and silently reporting a lower
 * one would make an app target hardware that cannot honour it.
 */
#include "wubu_vkd3d_device.h"

#include "wubu_vkd3d.h"       /* the D3D12_FEATURE_LEVEL enum */

#include "wubu_vkd3d_cmd.h"

#include <stdlib.h>
#include <string.h>

struct wubu_vkd3d_device {
    uint32_t                level;
    wubu_vkd3d_device_caps_t caps;
};

/* The caps each feature level implies.  Values follow the D3D12 feature-level
 * specification: a device at level X guarantees everything level X requires,
 * and may guarantee more -- so these are the MINIMUM guarantees, not the
 * device's actual limits. */
static void caps_for_level(uint32_t level, wubu_vkd3d_device_caps_t *c)
{
    memset(c, 0, sizeof(*c));
    c->feature_level = level;

    /* Everything at or above 11.0 gets the baseline.  Each later level is an
     * INHERITANCE, not a replacement: 11.1 is everything 11.0 guarantees plus
     * more.  (A fall-through switch got this wrong on the first attempt --
     * 11.1 silently lost tessellation and geometry shaders.) */
    c->max_render_targets = 8;
    c->max_texture_dimension = 16384;
    c->max_gpu_visible_descriptors = 100000;
    c->max_shader_model = 5;
    c->compute_shaders = true;
    c->geometry_shaders = true;
    c->tessellation = true;
    c->resource_binding_tier = 2;
    c->tiled_resources_tier = 1;

    if (level >= D3D12_FEATURE_LEVEL_11_1) {
        c->tiled_resources_tier = 2;
        c->depth_bounds = true;
    }
    if (level >= D3D12_FEATURE_LEVEL_12_0) {
        c->max_shader_model = 6;
        c->resource_binding_tier = 3;
        c->tiled_resources_tier = 3;
        c->max_gpu_visible_descriptors = 1000000;
        c->conservative_raster = true;
        c->rasterizer_ordered_views = true;
    }
    if (level >= D3D12_FEATURE_LEVEL_12_1) {
        c->max_shader_model = 6;
    }
    if (level >= D3D12_FEATURE_LEVEL_12_2) {
        c->max_shader_model = 7;
    }
}

/* A D3D12 device exists only at 11.0 or above.  The 9.x and 10.x values are in
 * the enum for D3D11-era compatibility reporting; a D3D12 device cannot be
 * created at them. */
static bool level_known(uint32_t level)
{
    switch (level) {
    case D3D12_FEATURE_LEVEL_11_0:
    case D3D12_FEATURE_LEVEL_11_1:
    case D3D12_FEATURE_LEVEL_12_0:
    case D3D12_FEATURE_LEVEL_12_1:
    case D3D12_FEATURE_LEVEL_12_2:
        return true;
    default:
        return false;
    }
}

wubu_vkd3d_device_t *wubu_vkd3d_device_create(uint32_t feature_level)
{
    if (!level_known(feature_level)) return NULL;

    wubu_vkd3d_device_t *d = calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->level = feature_level;
    caps_for_level(feature_level, &d->caps);
    return d;
}

void wubu_vkd3d_device_destroy(wubu_vkd3d_device_t *dev)
{
    free(dev);
}

const wubu_vkd3d_device_caps_t *wubu_vkd3d_device_caps(const wubu_vkd3d_device_t *dev)
{
    return dev ? &dev->caps : NULL;
}

uint32_t wubu_vkd3d_device_feature_level(const wubu_vkd3d_device_t *dev)
{
    return dev ? dev->level : 0;
}

bool wubu_vkd3d_device_supports(const wubu_vkd3d_device_t *dev, uint32_t feature_level)
{
    if (!dev) return false;
    return wubu_vkd3d_feature_level_le(feature_level, dev->level);
}

bool wubu_vkd3d_device_supports_command(const wubu_vkd3d_device_t *dev, uint32_t cmd_type)
{
    if (!dev) return false;
    /* All four D3D12 command types are legal on every D3D12 device -- the
     * queues differ, not the legality. */
    return cmd_type < VKD3D_CMD_COUNT;
}

const char *wubu_vkd3d_device_validate_pso(const wubu_vkd3d_device_t *dev,
                                           const wubu_vkd3d_pso_desc_t *d)
{
    if (!dev || !d) return "null device or descriptor";

    const wubu_vkd3d_device_caps_t *c = &dev->caps;

    /* A Vulkan sample count must be a power of two (1, 2, 4, 8, 16, ...).
     * D3D12 rejects anything else at PSO creation. */
    if (d->sample_count == 0 || (d->sample_count & (d->sample_count - 1)) != 0)
        return "sample count must be a power of two";

    if (d->rtv_count > c->max_render_targets)
        return "too many render targets for this feature level";

    if (d->dsv_format != 0 && !d->depth_enable)
        return "a depth-stencil format requires depth testing to be enabled";

    if ((d->hs_hash != 0 || d->ds_hash != 0) && !c->tessellation)
        return "tessellation shaders need a device with tessellation";

    if (d->gs_hash != 0 && !c->geometry_shaders)
        return "geometry shaders need a device with geometry shaders";

    if (d->cs_hash != 0 && !c->compute_shaders)
        return "compute shaders need a device with compute support";

    /* A zero stride means the input assembler would read every vertex from the
     * same offset -- almost always a layout bug rather than an intent. */
    for (uint32_t i = 0; i < d->input_element_count; i++) {
        if (d->input_strides[i] == 0)
            return "an input element has a zero stride";
    }

    return NULL;      /* legal */
}
