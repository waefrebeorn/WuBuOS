/*
 * wubu_vkd3d_swapchain_test.c -- the swapchain / present path.
 *
 * The load-bearing rules: a flip chain needs 2+ buffers, a frame-latency cap
 * cannot exceed the ring, and a SyncInterval above 1 has NO Vulkan equivalent
 * (so the translation must SAY it needs an app-level limiter rather than
 * pretending FIFO is "every Nth frame").
 */
#include "wubu_vkd3d_swapchain.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void base_desc(wubu_vkd3d_swapchain_desc_t *d)
{
    memset(d, 0, sizeof(*d));
    d->width = 1920;
    d->height = 1080;
    d->buffer_count = 4;
    d->effect = VKD3D_SWAP_EFFECT_FLIP_SEQUENTIAL;
    d->tearing_allowed = false;
    d->sync_interval = 1;
    d->max_frame_latency = 3;   /* 3 in flight need 4 images, so 4 buffers */
}

int main(void)
{
    printf("=== test_vkd3d_swapchain ===\n");
    wubu_vkd3d_swapchain_desc_t d;
    wubu_vkd3d_swapchain_vk_t vk;

    base_desc(&d);
    CHECK(wubu_vkd3d_swapchain_validate(&d) == NULL, "a normal desc is legal");
    CHECK(wubu_vkd3d_swapchain_vk(&d, &vk) && vk.valid, "it translates");
    CHECK(vk.present_mode == VK_PRESENT_MODE_FIFO_KHR,
          "FLIP_SEQUENTIAL + sync interval 1 -> FIFO (vsync)");
    CHECK(!vk.needs_app_throttle, "sync interval 1 needs no app limiter");
    CHECK(vk.create_info.minImageCount == 4,
          "a frame latency of 3 needs 4 images (N in flight + 1)");

    /* -- the three swap effects -- */
    base_desc(&d); d.sync_interval = 0; d.tearing_allowed = false;
    CHECK(wubu_vkd3d_swapchain_vk(&d, &vk) && vk.present_mode == VK_PRESENT_MODE_MAILBOX_KHR,
          "FLIP + interval 0, no tearing -> MAILBOX");
    base_desc(&d); d.sync_interval = 0; d.tearing_allowed = true;
    CHECK(wubu_vkd3d_swapchain_vk(&d, &vk) && vk.present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR,
          "interval 0 + tearing -> IMMEDIATE");
    base_desc(&d); d.effect = VKD3D_SWAP_EFFECT_DISCARD;
    CHECK(wubu_vkd3d_swapchain_vk(&d, &vk) && vk.present_mode == VK_PRESENT_MODE_FIFO_KHR,
          "the legacy bitblt swap effect with vsync -> FIFO");

    /* -- the sync interval above 1 has NO Vulkan equivalent -- */
    base_desc(&d); d.sync_interval = 2;
    CHECK(wubu_vkd3d_swapchain_vk(&d, &vk) && vk.present_mode == VK_PRESENT_MODE_FIFO_KHR,
          "sync interval 2 still maps to FIFO (Vulkan cannot express it)");
    CHECK(vk.needs_app_throttle,
          "and the translation SAYS an app-level limiter is required");
    base_desc(&d); d.sync_interval = 4;
    CHECK(wubu_vkd3d_swapchain_vk(&d, &vk) && vk.needs_app_throttle,
          "sync interval 4 needs the limiter too");

    /* -- validation -- */
    base_desc(&d); d.effect = VKD3D_SWAP_EFFECT_FLIP_SEQUENTIAL; d.buffer_count = 1;
    CHECK(wubu_vkd3d_swapchain_validate(&d) != NULL,
          "a flip chain needs at least two buffers");
    CHECK(!wubu_vkd3d_swapchain_vk(&d, &vk), "and it does not translate");
    base_desc(&d); d.effect = VKD3D_SWAP_EFFECT_DISCARD; d.buffer_count = 1;
    CHECK(wubu_vkd3d_swapchain_validate(&d) == NULL,
          "the legacy DISCARD effect works with a single buffer");
    /* Vulkan may return MORE images than requested, so a latency cap does not
     * make a desc invalid -- it RAISES the image count instead. */
    base_desc(&d); d.max_frame_latency = 8; d.buffer_count = 2;
    CHECK(wubu_vkd3d_swapchain_validate(&d) == NULL,
          "a large frame latency cap is not an error");
    CHECK(wubu_vkd3d_swapchain_min_image_count(&d) == 9,
          "it raises the image count to hold them (8 in flight + 1)");
    CHECK(wubu_vkd3d_swapchain_vk(&d, &vk) && vk.create_info.minImageCount == 9,
          "and the raised count reaches the Vulkan create info");
    base_desc(&d); d.sync_interval = 5;
    CHECK(wubu_vkd3d_swapchain_validate(&d) != NULL, "a sync interval above 4 is out of range");
    base_desc(&d); d.width = 0;
    CHECK(wubu_vkd3d_swapchain_validate(&d) != NULL, "a zero width is rejected");
    CHECK(wubu_vkd3d_swapchain_validate(NULL) != NULL, "a null desc is rejected");
    CHECK(!wubu_vkd3d_swapchain_vk(NULL, &vk), "a null desc does not translate");

    /* -- min image count -- */
    base_desc(&d); d.max_frame_latency = 1;
    CHECK(wubu_vkd3d_swapchain_min_image_count(&d) == 3,
          "never below 3 images, even for a latency of 1");
    base_desc(&d); d.max_frame_latency = 5;
    CHECK(wubu_vkd3d_swapchain_min_image_count(&d) == 6,
          "a latency of 5 needs 6 images");

    /* -- frame latency -- */
    {
        wubu_vkd3d_frame_latency_t f;
        wubu_vkd3d_frame_latency_init(&f, 2);
        CHECK(wubu_vkd3d_frame_latency_can_present(&f), "a fresh fence can present");
        wubu_vkd3d_frame_latency_submit(&f);
        CHECK(wubu_vkd3d_frame_latency_can_present(&f), "one in flight is fine");
        wubu_vkd3d_frame_latency_submit(&f);
        CHECK(!wubu_vkd3d_frame_latency_can_present(&f),
              "two in flight hits the cap -- the app must wait");
        CHECK(wubu_vkd3d_frame_latency_in_flight(&f) == 2, "two are tracked");
        wubu_vkd3d_frame_latency_complete(&f);
        CHECK(wubu_vkd3d_frame_latency_can_present(&f),
              "a completed frame frees a slot");
        CHECK(wubu_vkd3d_frame_latency_in_flight(&f) == 1, "one is still in flight");
        wubu_vkd3d_frame_latency_complete(&f);
        wubu_vkd3d_frame_latency_complete(&f);   /* must not underflow */
        CHECK(wubu_vkd3d_frame_latency_in_flight(&f) == 0, "the counter does not underflow");
        CHECK(wubu_vkd3d_frame_latency_can_present(&f), "empty again");
        wubu_vkd3d_frame_latency_submit(&f);
        CHECK(wubu_vkd3d_frame_latency_in_flight(&f) == 1, "submit after emptying works");
    }

    /* -- create info -- */
    base_desc(&d);
    CHECK(wubu_vkd3d_swapchain_vk(&d, &vk) && vk.valid, "translates");
    CHECK(vk.create_info.imageExtent.width == 1920 &&
          vk.create_info.imageExtent.height == 1080,
          "the extent is carried through");
    CHECK(vk.create_info.imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
          "the images are colour attachments");
    CHECK(vk.create_info.presentMode == vk.present_mode,
          "the create info agrees with the present mode");

    if (failures == 0) printf("ALL VKD3D_SWAPCHAIN TESTS PASSED\n");
    else printf("%d VKD3D_SWAPCHAIN FAILURES\n", failures);
    return failures ? 1 : 0;
}
