/*
 * wubu_vkd3d_msaa_test.c -- sample descriptions (MSAA).
 *
 * The load-bearing rule: a pipeline's rasterizationSamples must EQUAL the
 * attachment's sampleCount -- there is no "at most", and some drivers only
 * report the mismatch at submit time.
 */
#include "wubu_vkd3d_msaa.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_vkd3d_msaa ===\n");
    wubu_vkd3d_sample_desc_t d;
    wubu_vkd3d_msaa_vk_t vk;

    /* -- validation -- */
    d.count = 1; d.quality = 0;
    CHECK(wubu_vkd3d_sample_desc_validate(&d) == NULL, "1x / quality 0 is legal");
    d.count = 4; d.quality = 0;
    CHECK(wubu_vkd3d_sample_desc_validate(&d) == NULL, "4x / quality 0 is legal");
    d.count = 4; d.quality = 2;
    CHECK(wubu_vkd3d_sample_desc_validate(&d) == NULL, "4x / quality 2 is legal");
    d.count = 3; d.quality = 0;
    CHECK(strcmp(wubu_vkd3d_sample_desc_validate(&d),
                 "a sample count must be 1, 2, 4, 8 or 16") == 0,
          "a non-power-of-two count is reported");
    d.count = 6; d.quality = 0;
    CHECK(wubu_vkd3d_sample_desc_validate(&d) != NULL, "6x is not a legal count");
    d.count = 0; d.quality = 0;
    CHECK(wubu_vkd3d_sample_desc_validate(&d) != NULL, "0x is rejected");
    d.count = 1; d.quality = 1;
    CHECK(strcmp(wubu_vkd3d_sample_desc_validate(&d),
                 "at 1x the quality level must be 0") == 0,
          "quality at 1x must be 0");
    CHECK(wubu_vkd3d_sample_desc_validate(NULL) != NULL, "a null desc is rejected");

    /* -- the mapping -- */
    d.count = 1; d.quality = 0;
    CHECK(wubu_vkd3d_msaa_vk(&d, &vk) && vk.valid, "1x maps");
    CHECK(vk.rasterization_samples == VK_SAMPLE_COUNT_1_BIT, "1x -> SAMPLE_COUNT_1");
    CHECK(!vk.sample_shading_enable, "quality 0 -> sample shading OFF");
    CHECK(!vk.needs_resolve, "1x needs no resolve");

    d.count = 2; d.quality = 0;
    CHECK(wubu_vkd3d_msaa_vk(&d, &vk) && vk.valid &&
          vk.rasterization_samples == VK_SAMPLE_COUNT_2_BIT, "2x -> SAMPLE_COUNT_2");
    CHECK(vk.needs_resolve, "2x NEEDS a resolve");

    d.count = 4; d.quality = 0;
    CHECK(wubu_vkd3d_msaa_vk(&d, &vk) && vk.valid &&
          vk.rasterization_samples == VK_SAMPLE_COUNT_4_BIT, "4x -> SAMPLE_COUNT_4");
    CHECK(vk.needs_resolve, "4x needs a resolve");

    d.count = 8; d.quality = 0;
    CHECK(wubu_vkd3d_msaa_vk(&d, &vk) && vk.valid &&
          vk.rasterization_samples == VK_SAMPLE_COUNT_8_BIT, "8x -> SAMPLE_COUNT_8");

    d.count = 16; d.quality = 0;
    CHECK(wubu_vkd3d_msaa_vk(&d, &vk) && vk.valid &&
          vk.rasterization_samples == VK_SAMPLE_COUNT_16_BIT, "16x -> SAMPLE_COUNT_16");

    /* -- quality -> sample shading -- */
    d.count = 4; d.quality = 1;
    CHECK(wubu_vkd3d_msaa_vk(&d, &vk) && vk.valid && vk.sample_shading_enable,
          "a non-zero quality enables sample shading");
    CHECK(vk.min_sample_shading == 1.0f, "minSampleShading is 1.0");
    d.count = 4; d.quality = 7;
    CHECK(wubu_vkd3d_msaa_vk(&d, &vk) && vk.valid && vk.sample_shading_enable,
          "any non-zero quality enables it (the level itself is vendor-specific)");

    /* -- Count > 1 does NOT imply sample shading -- */
    d.count = 4; d.quality = 0;
    CHECK(wubu_vkd3d_msaa_vk(&d, &vk) && vk.valid && !vk.sample_shading_enable,
          "4x with quality 0 does NOT enable sample shading (independent knobs)");

    /* -- the resolve requirement -- */
    CHECK(!wubu_vkd3d_needs_resolve(1), "1x needs no resolve");
    CHECK(wubu_vkd3d_needs_resolve(2), "2x needs a resolve");
    CHECK(wubu_vkd3d_needs_resolve(16), "16x needs a resolve");

    /* -- pipeline vs attachment: they must be EQUAL -- */
    CHECK(wubu_vkd3d_sample_counts_compatible(4, 4), "4x pipeline + 4x attachment is legal");
    CHECK(!wubu_vkd3d_sample_counts_compatible(4, 1),
          "a 4x pipeline CANNOT draw into a 1x attachment");
    CHECK(!wubu_vkd3d_sample_counts_compatible(1, 4),
          "nor a 1x pipeline into a 4x attachment (no 'at most')");
    CHECK(!wubu_vkd3d_sample_counts_compatible(2, 8), "2x vs 8x is a mismatch");

    /* -- max sample count from device flags -- */
    CHECK(wubu_vkd3d_max_color_samples(VK_SAMPLE_COUNT_1_BIT) == 1, "1 from flags");
    CHECK(wubu_vkd3d_max_color_samples(VK_SAMPLE_COUNT_2_BIT |
                                       VK_SAMPLE_COUNT_4_BIT) == 4, "4 from flags");
    CHECK(wubu_vkd3d_max_color_samples(VK_SAMPLE_COUNT_16_BIT |
                                       VK_SAMPLE_COUNT_4_BIT |
                                       VK_SAMPLE_COUNT_2_BIT |
                                       VK_SAMPLE_COUNT_1_BIT) == 16,
          "16 from flags (the highest wins)");
    CHECK(wubu_vkd3d_max_color_samples(0) == 0, "no flags -> 0");

    /* -- an invalid desc does not map -- */
    d.count = 3; d.quality = 0;
    CHECK(!wubu_vkd3d_msaa_vk(&d, &vk), "an invalid desc does not map");
    CHECK(!vk.valid && vk.reason != NULL, "and it carries a reason");

    if (failures == 0) printf("ALL VKD3D_MSAA TESTS PASSED\n");
    else printf("%d VKD3D_MSAA FAILURES\n", failures);
    return failures ? 1 : 0;
}
