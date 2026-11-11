/*
 * wubu_vkd3d_fence_test.c -- the timeline-semaphore fence.
 *
 * The load-bearing rule: timeline values are STRICTLY increasing, because a
 * non-increasing signal would collapse two submissions into one and let the CPU
 * reuse memory the GPU is still reading.
 */
#include "wubu_vkd3d_fence.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_vkd3d_fence ===\n");
    wubu_vkd3d_fence_t f;
    wubu_vkd3d_fence_init(&f);

    CHECK(wubu_vkd3d_fence_value(&f) == 0, "a new fence starts at 0");
    CHECK(wubu_vkd3d_fence_completed(&f) == 0, "nothing has completed yet");
    CHECK(!wubu_vkd3d_fence_signalled(&f, 1), "value 1 is not signalled yet");

    /* -- strictly increasing -- */
    CHECK(wubu_vkd3d_fence_signal(&f, 1) == 0, "the first signal is accepted");
    CHECK(wubu_vkd3d_fence_signal(&f, 1) == -1, "an EQUAL signal is rejected");
    CHECK(wubu_vkd3d_fence_signal(&f, 0) == -1, "a LOWER signal is rejected");
    CHECK(wubu_vkd3d_fence_value(&f) == 1, "a rejected signal does not move the value");
    CHECK(wubu_vkd3d_fence_signal(&f, 2) == 0, "a strictly greater signal is accepted");
    CHECK(wubu_vkd3d_fence_value(&f) == 2, "the value advanced");

    /* -- waiting -- */
    CHECK(wubu_vkd3d_fence_wait(&f, 1) == 1, "a submitted value has not completed yet");
    CHECK(wubu_vkd3d_fence_wait(&f, 2) == 1, "the newest value is also outstanding");
    CHECK(wubu_vkd3d_fence_wait(&f, 3) == -1,
          "waiting on a value that was never signalled is an error, not a block");

    /* the GPU completes the submissions */
    f.completed = 2;
    CHECK(wubu_vkd3d_fence_wait(&f, 1) == 0, "a completed value returns 'already done'");
    CHECK(wubu_vkd3d_fence_wait(&f, 2) == 0, "the newest value is done too");
    CHECK(wubu_vkd3d_fence_signalled(&f, 1), "signalled() reflects completion");
    CHECK(!wubu_vkd3d_fence_signalled(&f, 3), "an unsignalled value is not signalled");

    /* -- the Vulkan mapping -- */
    {
        wubu_vkd3d_fence_vk_t vk;
        CHECK(wubu_vkd3d_fence_vk(&f, &vk), "the fence maps to a Vulkan timeline semaphore");
        CHECK(vk.valid && vk.type == VK_SEMAPHORE_TYPE_TIMELINE_KHR,
              "the type is VK_SEMAPHORE_TYPE_TIMELINE_KHR (Vulkan has no binary match)");
        CHECK(vk.initial_value == 0, "the initial value is 0");
        CHECK(vk.wait_stage_mask != 0, "a wait stage is named");
        CHECK(!wubu_vkd3d_fence_vk(NULL, &vk), "a null fence does not map");
        CHECK(!wubu_vkd3d_fence_vk(&f, NULL), "a null out pointer is rejected");
    }

    /* -- a second fence is independent -- */
    {
        wubu_vkd3d_fence_t g;
        wubu_vkd3d_fence_init(&g);
        wubu_vkd3d_fence_signal(&g, 1);
        CHECK(wubu_vkd3d_fence_value(&g) == 1 && wubu_vkd3d_fence_value(&f) == 2,
              "fences are independent counters");
    }

    wubu_vkd3d_fence_init(NULL);        /* must be a safe no-op */
    CHECK(wubu_vkd3d_fence_signal(NULL, 1) == -1, "a null fence rejects a signal");

    if (failures == 0) printf("ALL VKD3D_FENCE TESTS PASSED\n");
    else printf("%d VKD3D_FENCE FAILURES\n", failures);
    return failures ? 1 : 0;
}
