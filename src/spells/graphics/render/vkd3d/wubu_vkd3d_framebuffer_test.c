/*
 * wubu_vkd3d_framebuffer_test.c -- image views and the framebuffer.
 *
 * The load-bearing rule: the aspect must come from the FORMAT, not from the
 * caller's claim.  Binding a colour view as depth-stencil does not error -- it
 * silently corrupts the depth test.
 */
#include "wubu_vkd3d_framebuffer.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void color_att(wubu_vkd3d_framebuffer_attachment_t *a)
{
    memset(a, 0, sizeof(*a));
    a->view = (VkImageView)(uintptr_t)0x1234;   /* non-NULL stand-in */
    a->format = VK_FORMAT_R8G8B8A8_UNORM;
    a->width = 1920;
    a->height = 1080;
    a->samples = 1;
    a->is_depth_stencil = false;
}

static void depth_att(wubu_vkd3d_framebuffer_attachment_t *a)
{
    color_att(a);
    a->format = VK_FORMAT_D32_SFLOAT;
    a->is_depth_stencil = true;
}

static void base_desc(wubu_vkd3d_framebuffer_desc_t *d)
{
    memset(d, 0, sizeof(*d));
    d->attachment_count = 2;
    color_att(&d->attachments[0]);
    depth_att(&d->attachments[1]);
    d->width = 1920;
    d->height = 1080;
    d->layers = 1;
}

int main(void)
{
    printf("=== test_vkd3d_framebuffer ===\n");
    wubu_vkd3d_framebuffer_desc_t d;
    VkImageAspectFlags f;

    /* -- aspect flags: the format decides -- */
    CHECK(wubu_vkd3d_aspect_flags(VK_FORMAT_D16_UNORM, &f) &&
          f == VK_IMAGE_ASPECT_DEPTH_BIT, "D16_UNORM is depth-only");
    CHECK(wubu_vkd3d_aspect_flags(VK_FORMAT_D32_SFLOAT, &f) &&
          f == VK_IMAGE_ASPECT_DEPTH_BIT, "D32_SFLOAT is depth-only");
    CHECK(wubu_vkd3d_aspect_flags(VK_FORMAT_S8_UINT, &f) &&
          f == VK_IMAGE_ASPECT_STENCIL_BIT, "S8_UINT is stencil-only");
    CHECK(wubu_vkd3d_aspect_flags(VK_FORMAT_D24_UNORM_S8_UINT, &f) &&
          f == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT),
          "D24S8 is BOTH aspects");
    CHECK(wubu_vkd3d_aspect_flags(VK_FORMAT_D32_SFLOAT_S8_UINT, &f) &&
          f == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT),
          "D32S8 is BOTH aspects");
    CHECK(wubu_vkd3d_aspect_flags(VK_FORMAT_R8G8B8A8_UNORM, &f) &&
          f == VK_IMAGE_ASPECT_COLOR_BIT, "a colour format is COLOR");
    CHECK(wubu_vkd3d_aspect_flags(VK_FORMAT_B8G8R8A8_SRGB, &f) &&
          f == VK_IMAGE_ASPECT_COLOR_BIT, "a BGRA sRGB format is COLOR too");
    CHECK(!wubu_vkd3d_aspect_flags(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, &f),
          "a multi-planar format has no single aspect");
    CHECK(!wubu_vkd3d_aspect_flags(VK_FORMAT_UNDEFINED, &f),
          "an unknown format is NOT guessed to be colour");
    CHECK(!wubu_vkd3d_aspect_flags(VK_FORMAT_R8G8B8A8_UNORM, NULL),
          "a null out pointer is rejected");

    /* -- validation -- */
    base_desc(&d);
    CHECK(wubu_vkd3d_framebuffer_validate(&d) == NULL, "a normal framebuffer is legal");

    base_desc(&d); d.attachments[0].view = VK_NULL_HANDLE;
    CHECK(strcmp(wubu_vkd3d_framebuffer_validate(&d),
                 "an attachment has no image view") == 0,
          "a missing image view is reported");
    base_desc(&d); d.attachments[0].format = VK_FORMAT_UNDEFINED;
    CHECK(wubu_vkd3d_framebuffer_validate(&d) != NULL,
          "a missing format is reported");
    base_desc(&d); d.attachment_count = 0;
    CHECK(wubu_vkd3d_framebuffer_validate(&d) != NULL,
          "a framebuffer with no attachments is reported");
    base_desc(&d); d.width = 0;
    CHECK(strcmp(wubu_vkd3d_framebuffer_validate(&d),
                 "a zero-sized framebuffer") == 0,
          "a zero-sized framebuffer is reported");
    base_desc(&d); d.layers = 0;
    CHECK(wubu_vkd3d_framebuffer_validate(&d) != NULL,
          "zero layers is reported");

    /* -- size mismatch: D3D12 never checks this -- */
    base_desc(&d); d.attachments[1].width = 1280;
    CHECK(strcmp(wubu_vkd3d_framebuffer_validate(&d),
                 "an attachment's size does not match the framebuffer") == 0,
          "a size mismatch is reported (D3D12 would not check it)");
    base_desc(&d); d.attachments[0].height = 720;
    CHECK(wubu_vkd3d_framebuffer_validate(&d) != NULL,
          "a height mismatch is reported too");

    /* -- sample count mismatch -- */
    base_desc(&d); d.attachments[1].samples = 4;
    CHECK(strcmp(wubu_vkd3d_framebuffer_validate(&d),
                 "an attachment's sample count does not match the framebuffer") == 0,
          "a sample count mismatch is reported");
    base_desc(&d); d.attachments[0].samples = 4; d.attachments[1].samples = 4;
    CHECK(wubu_vkd3d_framebuffer_validate(&d) == NULL,
          "a consistent 4x MSAA framebuffer is legal");

    /* -- a colour view bound as depth-stencil -- */
    base_desc(&d); d.attachments[1].format = VK_FORMAT_R8G8B8A8_UNORM;
    CHECK(strcmp(wubu_vkd3d_framebuffer_validate(&d),
                 "a depth-stencil attachment is not a depth-stencil format") == 0,
          "a colour format claimed as depth-stencil is reported");
    base_desc(&d); d.attachments[1].is_depth_stencil = false;
    CHECK(wubu_vkd3d_framebuffer_validate(&d) == NULL,
          "a depth format NOT claimed as depth-stencil is fine");
    CHECK(wubu_vkd3d_framebuffer_validate(NULL) != NULL, "a null desc is rejected");

    /* -- the create info -- */
    {
        VkFramebufferCreateInfo ci;
        base_desc(&d);
        CHECK(wubu_vkd3d_framebuffer_create_info(&d, &ci), "it translates");
        CHECK(ci.attachmentCount == 2 && ci.pAttachments,
              "the attachment count and view array are carried through");
        CHECK(ci.pAttachments[0] == d.attachments[0].view &&
              ci.pAttachments[1] == d.attachments[1].view,
              "the views are in order");
        CHECK(ci.width == 1920 && ci.height == 1080 && ci.layers == 1,
              "the extent and layer count are carried through");

        base_desc(&d); d.attachments[0].view = VK_NULL_HANDLE;
        CHECK(!wubu_vkd3d_framebuffer_create_info(&d, &ci),
              "an invalid desc does not translate");
        CHECK(!wubu_vkd3d_framebuffer_create_info(NULL, &ci),
              "a null desc does not translate");
        CHECK(!wubu_vkd3d_framebuffer_create_info(&d, NULL),
              "a null out pointer is rejected");
    }

    if (failures == 0) printf("ALL VKD3D_FRAMEBUFFER TESTS PASSED\n");
    else printf("%d VKD3D_FRAMEBUFFER FAILURES\n", failures);
    return failures ? 1 : 0;
}
