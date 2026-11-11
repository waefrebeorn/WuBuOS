/*
 * wubu_vkd3d_renderpass_test.c -- the render pass.
 *
 * The load-bearing rule: D3D12's NO_ACCESS must become DONT_CARE on BOTH
 * sides.  Mapping it to LOAD reads stale contents; mapping it to STORE keeps
 * them -- and D3D12's contract is that the contents are UNDEFINED afterwards.
 */
#include "wubu_vkd3d_renderpass.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void color_rt(wubu_vkd3d_attachment_t *a)
{
    memset(a, 0, sizeof(*a));
    a->format = VK_FORMAT_R8G8B8A8_UNORM;
    a->samples = 1;
    a->begin = VKD3D_BEGIN_PRESERVE;
    a->end = VKD3D_END_PRESERVE;
    a->initial_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    a->final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

static void depth_rt(wubu_vkd3d_attachment_t *a)
{
    memset(a, 0, sizeof(*a));
    a->format = VK_FORMAT_D32_SFLOAT;
    a->samples = 1;
    a->begin = VKD3D_BEGIN_CLEAR;
    a->end = VKD3D_END_PRESERVE;
    a->initial_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    a->final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

static void base_desc(wubu_vkd3d_renderpass_desc_t *d)
{
    memset(d, 0, sizeof(*d));
    d->attachment_count = 2;
    color_rt(&d->attachments[0]);
    depth_rt(&d->attachments[1]);
    d->has_depth_stencil = true;
    d->depth_stencil_attachment = 1;
    d->bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
}

int main(void)
{
    printf("=== test_vkd3d_renderpass ===\n");
    wubu_vkd3d_renderpass_desc_t d;
    VkAttachmentDescription ad[8];
    VkAttachmentReference ar[8];

    base_desc(&d);
    CHECK(wubu_vkd3d_renderpass_validate(&d) == NULL, "a normal desc is legal");
    CHECK(wubu_vkd3d_renderpass_attachments(&d, ad, 8) == 2, "two attachments translate");

    /* -- the load/store mapping -- */
    base_desc(&d); d.attachments[0].begin = VKD3D_BEGIN_PRESERVE;
    CHECK(wubu_vkd3d_renderpass_attachments(&d, ad, 8) == 2 &&
          ad[0].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD, "PRESERVE -> LOAD");
    base_desc(&d); d.attachments[0].begin = VKD3D_BEGIN_CLEAR;
    CHECK(wubu_vkd3d_renderpass_attachments(&d, ad, 8) == 2 &&
          ad[0].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR, "CLEAR -> CLEAR");
    base_desc(&d); d.attachments[0].begin = VKD3D_BEGIN_NO_ACCESS;
    CHECK(wubu_vkd3d_renderpass_attachments(&d, ad, 8) == 2 &&
          ad[0].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          "NO_ACCESS -> DONT_CARE on the load side");
    base_desc(&d); d.attachments[0].end = VKD3D_END_DISCARD;
    CHECK(wubu_vkd3d_renderpass_attachments(&d, ad, 8) == 2 &&
          ad[0].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE,
          "DISCARD -> DONT_CARE on the store side");

    /* -- NO_ACCESS on BOTH sides -- */
    base_desc(&d);
    d.attachments[0].begin = VKD3D_BEGIN_NO_ACCESS;
    d.attachments[0].end = VKD3D_END_DISCARD;
    CHECK(wubu_vkd3d_renderpass_attachments(&d, ad, 8) == 2 &&
          ad[0].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE &&
          ad[0].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE,
          "NO_ACCESS + DISCARD means the contents are undefined afterwards");

    /* -- the stencil ops follow the depth ops -- */
    base_desc(&d);
    CHECK(wubu_vkd3d_renderpass_attachments(&d, ad, 8) == 2 &&
          ad[1].stencilLoadOp == ad[1].loadOp &&
          ad[1].stencilStoreOp == ad[1].storeOp,
          "the stencil load/store ops mirror the depth ones");

    /* -- validation -- */
    base_desc(&d); d.attachment_count = 0;
    CHECK(wubu_vkd3d_renderpass_validate(&d) != NULL,
          "a render pass with no attachments is reported");
    base_desc(&d); d.depth_stencil_attachment = 5;
    CHECK(strcmp(wubu_vkd3d_renderpass_validate(&d),
                 "the depth-stencil attachment index is out of range") == 0,
          "an out-of-range depth-stencil index is reported by name");
    base_desc(&d); d.attachments[0].samples = 3;
    CHECK(strcmp(wubu_vkd3d_renderpass_validate(&d),
                 "a sample count must be a power of two (1,2,4,8,16)") == 0,
          "a non-power-of-two sample count is reported");
    base_desc(&d); d.attachments[0].initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    CHECK(strcmp(wubu_vkd3d_renderpass_validate(&d),
                 "a PRESERVE (load) attachment needs a real initial layout") == 0,
          "PRESERVE with an UNDEFINED layout is a contradiction");
    base_desc(&d);
    d.attachments[0].begin = VKD3D_BEGIN_NO_ACCESS;
    d.attachments[0].initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    CHECK(wubu_vkd3d_renderpass_validate(&d) != NULL,
          "even NO_ACCESS needs a valid initial layout");
    base_desc(&d); d.attachments[0].samples = 16;
    CHECK(wubu_vkd3d_renderpass_validate(&d) == NULL, "16x MSAA is legal");
    base_desc(&d); d.attachments[0].samples = 1;
    CHECK(wubu_vkd3d_renderpass_validate(&d) == NULL, "1x is legal");
    CHECK(wubu_vkd3d_renderpass_validate(NULL) != NULL, "a null desc is rejected");

    /* -- attachment references -- */
    base_desc(&d);
    CHECK(wubu_vkd3d_renderpass_color_refs(&d, ar, 8) == 1,
          "one color reference (the depth attachment is excluded)");
    CHECK(ar[0].attachment == 0 && ar[0].layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          "the color reference points at the colour attachment");
    {
        VkAttachmentReference dr;
        CHECK(wubu_vkd3d_renderpass_depth_ref(&d, &dr) == 1,
              "the depth reference points at the depth attachment");
        CHECK(dr.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
              "the depth reference uses the depth-stencil layout");
    }
    base_desc(&d); d.has_depth_stencil = false;
    CHECK(wubu_vkd3d_renderpass_color_refs(&d, ar, 8) == 2,
          "without depth, both attachments are colour");
    {
        VkAttachmentReference dr;
        CHECK(wubu_vkd3d_renderpass_depth_ref(&d, &dr) == -1,
              "no depth attachment means no depth reference");
    }
    /* depth FIRST: the colour refs must skip it and keep their order */
    base_desc(&d);
    {
        wubu_vkd3d_attachment_t tmp;
        depth_rt(&tmp);  d.attachments[0] = tmp;
        color_rt(&tmp);  d.attachments[1] = tmp;
    }
    d.depth_stencil_attachment = 0;
    CHECK(wubu_vkd3d_renderpass_color_refs(&d, ar, 8) == 1 && ar[0].attachment == 1,
          "a depth-first attachment list still yields the colour reference");
    /* an invalid desc does not translate */
    base_desc(&d); d.attachment_count = 0;
    CHECK(wubu_vkd3d_renderpass_color_refs(&d, ar, 8) == -1,
          "an invalid desc does not translate");

    if (failures == 0) printf("ALL VKD3D_RENDERPASS TESTS PASSED\n");
    else printf("%d VKD3D_RENDERPASS FAILURES\n", failures);
    return failures ? 1 : 0;
}
