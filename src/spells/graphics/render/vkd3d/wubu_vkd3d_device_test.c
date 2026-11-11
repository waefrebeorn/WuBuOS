/*
 * wubu_vkd3d_device_test.c -- the feature-level device.
 *
 * A feature level is a CONTRACT, so the load-bearing assertion is that every
 * level INHERITS everything below it: 11.1 must be everything 11.0 guarantees
 * plus more.  A fall-through switch got that wrong on the first attempt and
 * 11.1 silently lost tessellation and geometry shaders.
 */
#include "wubu_vkd3d_device.h"
#include "wubu_vkd3d.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void legal_desc(wubu_vkd3d_pso_desc_t *d)
{
    memset(d, 0, sizeof(*d));
    d->depth_enable = true;
    d->depth_write_enable = true;
    d->depth_func = 4;            /* LESS_EQUAL */
    d->fill_mode = 2;             /* SOLID */
    d->cull_mode = 2;             /* BACK */
    d->blend_src_color = 5;
    d->blend_dst_color = 6;
    d->blend_op_color = 1;
    d->blend_src_alpha = 5;
    d->blend_dst_alpha = 6;
    d->blend_op_alpha = 1;
    d->sample_count = 1;
    d->sample_mask = 0xFFFFFFFF;
    d->rtv_count = 1;
    d->primitive_topology = 4;    /* TRIANGLE */
    d->input_element_count = 1;
    d->input_strides[0] = 12;
    d->input_formats[0] = 41;
}

int main(void)
{
    printf("=== test_vkd3d_device ===\n");
    wubu_vkd3d_pso_desc_t d;

    /* -- creation -- */
    CHECK(wubu_vkd3d_device_create(0x9999) == NULL, "an unknown feature level is rejected");
    CHECK(wubu_vkd3d_device_create(D3D12_FEATURE_LEVEL_12_2 + 1) == NULL,
          "a level above anything modelled is rejected");
    CHECK(wubu_vkd3d_device_create(D3D12_FEATURE_LEVEL_10_0) == NULL,
          "a D3D11-era level is not a D3D12 device level");
    CHECK(wubu_vkd3d_device_create(0) == NULL, "level 0 is rejected");

    wubu_vkd3d_device_t *d110 = wubu_vkd3d_device_create(D3D12_FEATURE_LEVEL_11_0);
    wubu_vkd3d_device_t *d111 = wubu_vkd3d_device_create(D3D12_FEATURE_LEVEL_11_1);
    wubu_vkd3d_device_t *d122 = wubu_vkd3d_device_create(D3D12_FEATURE_LEVEL_12_2);
    CHECK(d110 && d111 && d122, "devices are created at their requested level");
    CHECK(wubu_vkd3d_device_feature_level(d110) == D3D12_FEATURE_LEVEL_11_0,
          "the device keeps the level it was created at");

    /* -- INHERITANCE: every level keeps everything below it -- */
    {
        const wubu_vkd3d_device_caps_t *c110 = wubu_vkd3d_device_caps(d110);
        const wubu_vkd3d_device_caps_t *c111 = wubu_vkd3d_device_caps(d111);
        const wubu_vkd3d_device_caps_t *c122 = wubu_vkd3d_device_caps(d122);
        CHECK(c110 && c111 && c122, "caps are available");

        CHECK(c110->tessellation && c110->geometry_shaders && c110->compute_shaders,
              "11.0 has tessellation, geometry and compute");
        /* the regression: 11.1 must not lose 11.0's guarantees */
        CHECK(c111->tessellation && c111->geometry_shaders,
              "11.1 still has tessellation and geometry (it inherits 11.0)");
        CHECK(c111->tiled_resources_tier > c110->tiled_resources_tier,
              "11.1 improves tiled resources over 11.0");
        CHECK(c122->tiled_resources_tier >= c111->tiled_resources_tier,
              "12.2 does not lose tiled resources");
        CHECK(c122->conservative_raster, "12.2 has conservative rasterisation");
        CHECK(c122->rasterizer_ordered_views, "12.2 has rasterizer ordered views");
        CHECK(c122->max_shader_model >= c111->max_shader_model,
              "12.2 does not lose shader model");
        CHECK(c122->max_gpu_visible_descriptors > c111->max_gpu_visible_descriptors,
              "12.2 raises the GPU-visible descriptor limit");
    }

    /* -- supports -- */
    CHECK(wubu_vkd3d_device_supports(d111, D3D12_FEATURE_LEVEL_11_0),
          "an 11.1 device supports 11.0");
    CHECK(!wubu_vkd3d_device_supports(d111, D3D12_FEATURE_LEVEL_12_0),
          "an 11.1 device does NOT claim 12.0");

    /* -- validation: a legal desc -- */
    legal_desc(&d);
    CHECK(wubu_vkd3d_device_validate_pso(d122, &d) == NULL, "a legal PSO is accepted");

    /* -- validation: each violation returns its reason -- */
    {
        wubu_vkd3d_pso_desc_t b;
        legal_desc(&b); b.sample_count = 3;
        CHECK(strcmp(wubu_vkd3d_device_validate_pso(d122, &b),
                     "sample count must be a power of two") == 0,
              "a non-power-of-two sample count is reported");

        legal_desc(&b); b.rtv_count = 9;
        CHECK(strcmp(wubu_vkd3d_device_validate_pso(d122, &b),
                     "too many render targets for this feature level") == 0,
              "too many render targets is reported");

        legal_desc(&b); b.dsv_format = 2; b.depth_enable = false;
        CHECK(strcmp(wubu_vkd3d_device_validate_pso(d122, &b),
                     "a depth-stencil format requires depth testing to be enabled") == 0,
              "a depth format without depth testing is reported");

        /* The tessellation / geometry / compute checks in the validator are
         * DEFENSIVE: every D3D12 feature level has all three, so they cannot
         * be reached from a real device.  They are exercised by the caps
         * assertions above instead (a level that lost one would be caught
         * there). */

        legal_desc(&b); b.input_strides[0] = 0;
        CHECK(strcmp(wubu_vkd3d_device_validate_pso(d122, &b),
                     "an input element has a zero stride") == 0,
              "a zero input stride is reported");

        /* the first violated rule wins, not a random one */
        legal_desc(&b); b.sample_count = 3; b.rtv_count = 99;
        CHECK(strcmp(wubu_vkd3d_device_validate_pso(d122, &b),
                     "sample count must be a power of two") == 0,
              "the first violated rule is the one reported");

        CHECK(wubu_vkd3d_device_validate_pso(NULL, &b) != NULL, "a null device is rejected");
        CHECK(wubu_vkd3d_device_validate_pso(d122, NULL) != NULL, "a null desc is rejected");
    }

    /* -- command support -- */
    CHECK(wubu_vkd3d_device_supports_command(d122, 0) &&
          wubu_vkd3d_device_supports_command(d122, 3),
          "all four D3D12 command types are legal on a D3D12 device");
    CHECK(!wubu_vkd3d_device_supports_command(d122, 4),
          "an unknown command type is not legal");
    CHECK(!wubu_vkd3d_device_supports_command(NULL, 0), "a null device supports nothing");

    wubu_vkd3d_device_destroy(d110);
    wubu_vkd3d_device_destroy(d111);
    wubu_vkd3d_device_destroy(d122);
    wubu_vkd3d_device_destroy(NULL);

    if (failures == 0) printf("ALL VKD3D_DEVICE TESTS PASSED\n");
    else printf("%d VKD3D_DEVICE FAILURES\n", failures);
    return failures ? 1 : 0;
}
