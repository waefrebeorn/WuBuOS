/*
 * wubu_vkd3d_pso_test.c -- the PSO cache and its key.
 *
 * The strongest assertion here is the MUTATION SWEEP: every field of the desc
 * is changed in turn and the key must change.  That is what makes a near-miss a
 * MISS instead of a silently wrong pipeline.
 */
#include "wubu_vkd3d_pso.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void base_desc(wubu_vkd3d_pso_desc_t *d)
{
    memset(d, 0, sizeof(*d));
    d->root_signature_hash = 0x1111;
    d->vs_hash = 0x2222;
    d->ps_hash = 0x3333;
    d->gs_hash = 0x4444;
    d->hs_hash = 0x5555;
    d->ds_hash = 0x6666;
    d->cs_hash = 0x7777;

    d->blend_enable = true;
    d->blend_src_color = 5;      /* SRC_ALPHA */
    d->blend_dst_color = 6;      /* INV_SRC_ALPHA */
    d->blend_op_color = 1;       /* ADD */
    d->blend_src_alpha = 5;
    d->blend_dst_alpha = 6;
    d->blend_op_alpha = 1;
    d->render_target_write_mask = 0xF;

    d->fill_mode = 2;            /* SOLID */
    d->cull_mode = 2;            /* BACK */
    d->front_counter_clockwise = true;
    d->depth_bias = 0;
    d->depth_bias_clamp = 0.0f;
    d->slope_scaled_depth_bias = 0.0f;
    d->depth_clip_enable = true;

    d->depth_enable = true;
    d->depth_write_enable = true;
    d->depth_func = 4;           /* LESS_EQUAL */
    d->stencil_enable = false;
    d->stencil_read_mask = 0xFF;
    d->stencil_write_mask = 0xFF;

    d->input_element_count = 2;
    d->input_strides[0] = 12;
    d->input_strides[1] = 12;
    d->input_formats[0] = 41;    /* R32G32B32_FLOAT */
    d->input_formats[1] = 41;

    d->primitive_topology = 4;   /* TRIANGLE */
    d->rtv_count = 1;
    d->rtv_formats[0] = 28;      /* R8G8B8A8_UNORM */
    d->dsv_format = 0;
    d->sample_count = 1;
    d->sample_mask = 0xFFFFFFFF;
}

/* Flip ONE field and report whether the key changed. */
static int field_changes_key(int field)
{
    wubu_vkd3d_pso_desc_t a, b;
    base_desc(&a);
    base_desc(&b);
    switch (field) {
    case 0:  b.root_signature_hash = 0x9999; break;
    case 1:  b.vs_hash = 0x9999; break;
    case 2:  b.ps_hash = 0x9999; break;
    case 3:  b.gs_hash = 0x9999; break;
    case 4:  b.hs_hash = 0x9999; break;
    case 5:  b.ds_hash = 0x9999; break;
    case 6:  b.cs_hash = 0x9999; break;
    case 7:  b.blend_enable = !b.blend_enable; break;
    case 8:  b.blend_src_color = 9; break;
    case 9:  b.blend_dst_color = 10; break;
    case 10: b.blend_op_color = 2; break;
    case 11: b.blend_src_alpha = 7; break;
    case 12: b.blend_dst_alpha = 8; break;
    case 13: b.blend_op_alpha = 3; break;
    case 14: b.render_target_write_mask = 0x7; break;
    case 15: b.fill_mode = 1; break;
    case 16: b.cull_mode = 3; break;
    case 17: b.front_counter_clockwise = !b.front_counter_clockwise; break;
    case 18: b.depth_bias = 1; break;
    case 19: b.depth_bias_clamp = 0.5f; break;
    case 20: b.slope_scaled_depth_bias = 0.5f; break;
    case 21: b.depth_clip_enable = !b.depth_clip_enable; break;
    case 22: b.depth_enable = !b.depth_enable; break;
    case 23: b.depth_write_enable = !b.depth_write_enable; break;
    case 24: b.depth_func = 2; break;
    case 25: b.stencil_enable = !b.stencil_enable; break;
    case 26: b.stencil_read_mask = 0x0F; break;
    case 27: b.stencil_write_mask = 0x0F; break;
    case 28: b.input_element_count = 3; break;
    case 29: b.input_strides[0] = 16; break;
    case 30: b.input_formats[0] = 1; break;
    case 31: b.primitive_topology = 3; break;
    case 32: b.rtv_count = 2; break;
    case 33: b.rtv_formats[0] = 29; break;
    case 34: b.dsv_format = 2; break;
    case 35: b.sample_count = 4; break;
    case 36: b.sample_mask = 0x7FFFFFFF; break;
    default: return -1;
    }
    return wubu_vkd3d_pso_key(&a) != wubu_vkd3d_pso_key(&b);
}

int main(void)
{
    printf("=== test_vkd3d_pso ===\n");
    wubu_vkd3d_pso_desc_t d, e;
    base_desc(&d);
    base_desc(&e);

    /* -- determinism -- */
    CHECK(wubu_vkd3d_pso_key(&d) == wubu_vkd3d_pso_key(&e),
          "the same desc always gives the same key");

    char hex[16];
    CHECK(wubu_vkd3d_pso_key_hex(&d, hex, sizeof(hex)) == hex, "the key hex is written");
    CHECK(strlen(hex) == 8, "the key hex is 8 characters");

    /* -- the mutation sweep: the key covers EVERY field -- */
    int swept = 0, missed = 0;
    for (int f = 0; f < 37; f++) {
        int r = field_changes_key(f);
        if (r == 1) swept++;
        else if (r == 0) { missed++; printf("    (field %d did NOT change the key)\n", f); }
    }
    CHECK(missed == 0, "every field of the desc is covered by the key");
    CHECK(swept == 37, "all 37 swept fields are covered");

    /* -- the cache -- */
    {
        wubu_vkd3d_pso_cache_t *c = wubu_vkd3d_pso_cache_create(4);
        CHECK(c != NULL, "a cache is created");
        CHECK(wubu_vkd3d_pso_cache_size(c) == 0, "the cache starts empty");

        int created = 0;
        uint32_t p1 = wubu_vkd3d_pso_get(c, &d, &created);
        CHECK(p1 != 0 && created == 1, "the first get is a miss that creates");
        CHECK(wubu_vkd3d_pso_cache_size(c) == 1, "the cache holds one pipeline");
        CHECK(wubu_vkd3d_pso_cache_misses(c) == 1, "one miss counted");

        uint32_t p2 = wubu_vkd3d_pso_get(c, &e, &created);
        CHECK(created == 0, "an identical desc is a HIT");
        CHECK(p2 == p1, "a hit returns the SAME pipeline");
        CHECK(wubu_vkd3d_pso_cache_size(c) == 1, "a hit does not grow the cache");
        CHECK(wubu_vkd3d_pso_cache_hits(c) == 1, "one hit counted");

        /* a near-miss is a MISS, never an approximation */
        d.blend_dst_color = 9;
        int created2 = 0;
        uint32_t p3 = wubu_vkd3d_pso_get(c, &d, &created2);
        CHECK(created2 == 1, "a one-field difference is a miss, not a hit");
        CHECK(p3 != p1, "the near-miss produces a DIFFERENT pipeline");
        CHECK(wubu_vkd3d_pso_cache_size(c) == 2, "the near-miss grows the cache");

        /* -- release -- */
        CHECK(wubu_vkd3d_pso_release(c, p3) == 0, "releasing a live pipeline");
        CHECK(wubu_vkd3d_pso_cache_size(c) == 1, "the release shrinks the cache");
        CHECK(wubu_vkd3d_pso_release(c, p3) == -1, "releasing twice is an app bug");
        CHECK(wubu_vkd3d_pso_release(c, 99999) == -1, "an unknown handle is rejected");
        CHECK(wubu_vkd3d_pso_release(c, 0) == -1, "handle 0 is invalid");

        /* a released pipeline's key can be reused */
        int created3 = 0;
        uint32_t p4 = wubu_vkd3d_pso_get(c, &d, &created3);
        CHECK(created3 == 1 && p4 != p1, "the freed slot is reusable");

        /* -- capacity is enforced, never evicted silently -- */
        wubu_vkd3d_pso_desc_t t;
        base_desc(&t);
        t.ps_hash = 0xAAAA; uint32_t pa = wubu_vkd3d_pso_get(c, &t, NULL);
        t.ps_hash = 0xBBBB; uint32_t pb = wubu_vkd3d_pso_get(c, &t, NULL);
        CHECK(pa != 0 && pb != 0, "the cache fills up to capacity");
        CHECK(wubu_vkd3d_pso_cache_size(c) == 4, "the cache is at capacity");
        t.ps_hash = 0xCCCC;
        CHECK(wubu_vkd3d_pso_get(c, &t, NULL) == 0,
              "a full cache rejects a new PSO instead of evicting silently");
        CHECK(wubu_vkd3d_pso_cache_size(c) == 4, "a rejection does not grow the cache");

        /* the earlier pipelines are still intact after the rejections */
        int created4 = 0;
        CHECK(wubu_vkd3d_pso_get(c, &e, &created4) == p1 && created4 == 0,
              "an existing entry is still served after rejections");

        wubu_vkd3d_pso_cache_destroy(c);
    }

    /* -- the Vulkan mapping -- */
    {
        wubu_vkd3d_pso_vk_t vk;
        base_desc(&d);   /* the cache section above mutated d */
        CHECK(wubu_vkd3d_pso_vk(&d, &vk), "a valid desc maps");
        CHECK(vk.valid && vk.polygon_mode == VK_POLYGON_MODE_FILL,
              "fill mode SOLID -> VK_POLYGON_MODE_FILL");
        CHECK(vk.cull_mode == VK_CULL_MODE_BACK_BIT, "cull BACK maps");
        CHECK(vk.front_face == VK_FRONT_FACE_COUNTER_CLOCKWISE,
              "a counter-clockwise front face maps");
        CHECK(vk.depth_func == VK_COMPARE_OP_LESS_OR_EQUAL,
              "LESS_EQUAL maps to VkCompareOp");

        /* the blend-factor shift: D3D12 5 (SRC_ALPHA) is Vulkan 4 */
        CHECK(vk.src_color == VK_BLEND_FACTOR_SRC_ALPHA,
              "SRC_ALPHA maps (D3D12 is 1-based, Vulkan is 0-based)");
        CHECK(vk.dst_color == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
              "INV_SRC_ALPHA maps");
        CHECK(vk.op_color == VK_BLEND_OP_ADD, "blend ADD maps");

        CHECK(vk.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST && vk.topology_valid,
              "a TRIANGLE topology maps to the Vulkan list form");

        /* -- the MIN/MAX swap between the two APIs -- */
        wubu_vkd3d_pso_desc_t m;
        base_desc(&m);
        m.blend_op_color = 4;   /* D3D12 MIN */
        CHECK(wubu_vkd3d_pso_vk(&m, &vk) && vk.op_color == VK_BLEND_OP_MIN,
              "D3D12 blend MIN -> VK_BLEND_OP_MIN");
        m.blend_op_color = 5;   /* D3D12 MAX */
        CHECK(wubu_vkd3d_pso_vk(&m, &vk) && vk.op_color == VK_BLEND_OP_MAX,
              "D3D12 blend MAX -> VK_BLEND_OP_MAX");

        /* -- invalid enums are rejected, never defaulted -- */
        m.blend_op_color = 99;
        CHECK(!wubu_vkd3d_pso_vk(&m, &vk), "an unknown blend op is rejected");
        base_desc(&m); m.fill_mode = 99;
        CHECK(!wubu_vkd3d_pso_vk(&m, &vk), "an unknown fill mode is rejected");
        base_desc(&m); m.cull_mode = 99;
        CHECK(!wubu_vkd3d_pso_vk(&m, &vk), "an unknown cull mode is rejected");
        base_desc(&m); m.depth_func = 99;
        CHECK(!wubu_vkd3d_pso_vk(&m, &vk), "an unknown comparison func is rejected");
        base_desc(&m); m.blend_src_color = 12;   /* D3D12's BLEND_FACTOR */
        CHECK(!wubu_vkd3d_pso_vk(&m, &vk),
              "D3D12's BLEND_FACTOR is invalid without a separate API");
        base_desc(&m); m.blend_src_color = 13;   /* D3D12's INV_BLEND_FACTOR */
        CHECK(!wubu_vkd3d_pso_vk(&m, &vk),
              "D3D12's INV_BLEND_FACTOR is invalid without a separate API");

        /* the dual-source-blend family starts at 14, not 13 -- an off-by-one
         * here silently binds the wrong blend factor. */
        base_desc(&m); m.blend_src_color = 14;
        CHECK(wubu_vkd3d_pso_vk(&m, &vk) && vk.src_color == VK_BLEND_FACTOR_SRC1_COLOR,
              "D3D12 14 (SRC1_COLOR) maps to SRC1_COLOR");
        base_desc(&m); m.blend_src_color = 15;
        CHECK(wubu_vkd3d_pso_vk(&m, &vk) &&
              vk.src_color == VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
              "D3D12 15 (INV_SRC1_COLOR) maps correctly");
        base_desc(&m); m.blend_src_color = 16;
        CHECK(wubu_vkd3d_pso_vk(&m, &vk) && vk.src_color == VK_BLEND_FACTOR_SRC1_ALPHA,
              "D3D12 16 (SRC1_ALPHA) maps correctly");
        base_desc(&m); m.blend_src_color = 17;
        CHECK(wubu_vkd3d_pso_vk(&m, &vk) &&
              vk.src_color == VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
              "D3D12 17 (INV_SRC1_ALPHA) maps correctly");
        base_desc(&m); m.primitive_topology = 99;
        CHECK(!wubu_vkd3d_pso_vk(&m, &vk), "an unknown topology is rejected");

        /* wireframe */
        base_desc(&m); m.fill_mode = 1;
        CHECK(wubu_vkd3d_pso_vk(&m, &vk) && vk.polygon_mode == VK_POLYGON_MODE_LINE,
              "fill mode WIREFRAME -> VK_POLYGON_MODE_LINE");
    }

    if (failures == 0) printf("ALL VKD3D_PSO TESTS PASSED\n");
    else printf("%d VKD3D_PSO FAILURES\n", failures);
    return failures ? 1 : 0;
}
