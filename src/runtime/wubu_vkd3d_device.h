/*
 * wubu_vkd3d_device.h -- VKD3D slice 5: the feature-level device.
 *
 * The four slices before this one describe objects; a device decides which of
 * them are legal.  D3D12's answer is the FEATURE LEVEL: a device advertises a
 * maximum level, and every level below it must work.  A feature level is not a
 * performance number -- it is a CONTRACT: an app that targets 12.0 may use
 * anything 12.0 allows, and a device claiming 12.0 that fails one of those
 * guarantees is broken.
 *
 * So this device:
 *   - is created AT a feature level (it never downgrades silently), and
 *   - VALIDATES a PSO desc against the caps that level implies, returning the
 *     first reason instead of a boolean, because "no" without a reason sends
 *     the app author hunting through unrelated code.
 *
 * Opaque struct, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_DEVICE_H
#define WUBU_VKD3D_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wubu_vkd3d_pso.h"

typedef struct wubu_vkd3d_device wubu_vkd3d_device_t;

typedef struct {
    uint32_t feature_level;        /* the max D3D12_FEATURE_LEVEL this device supports */
    uint32_t max_shader_model;     /* 5, 6 or 7 */
    uint32_t resource_binding_tier;/* 1..3 */
    uint32_t tiled_resources_tier; /* 0..3 (0 = not supported) */
    uint32_t max_render_targets;   /* simultaneous colour targets */
    uint32_t max_texture_dimension;
    uint32_t max_gpu_visible_descriptors;

    bool     compute_shaders;
    bool     geometry_shaders;
    bool     tessellation;
    bool     conservative_raster;
    bool     rasterizer_ordered_views;
    bool     depth_bounds;
} wubu_vkd3d_device_caps_t;

/* Create a device at a feature level.  Returns NULL for an unknown level --
 * a device never claims a level it was not asked for. */
wubu_vkd3d_device_t *wubu_vkd3d_device_create(uint32_t feature_level);
void wubu_vkd3d_device_destroy(wubu_vkd3d_device_t *dev);

const wubu_vkd3d_device_caps_t *wubu_vkd3d_device_caps(const wubu_vkd3d_device_t *dev);
uint32_t wubu_vkd3d_device_feature_level(const wubu_vkd3d_device_t *dev);

/* Validate a PSO desc against this device.  Returns 0 when legal, or the first
 * violated rule (a static string suitable for an error log). */
const char *wubu_vkd3d_device_validate_pso(const wubu_vkd3d_device_t *dev,
                                           const wubu_vkd3d_pso_desc_t *d);

/* Is a given feature level at or below what this device supports? */
bool wubu_vkd3d_device_supports(const wubu_vkd3d_device_t *dev, uint32_t feature_level);

/* Validate a command type against the device (all four are legal on every
 * D3D12 device -- the queues differ, not the legality). */
bool wubu_vkd3d_device_supports_command(const wubu_vkd3d_device_t *dev, uint32_t cmd_type);

#endif /* WUBU_VKD3D_DEVICE_H */
