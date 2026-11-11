/*
 * wubu_vkd3d_cmd_test.c -- the command-list state machine.
 *
 * Every rejected transition is a use-after-free prevented, so the test drives
 * the whole cycle and asserts each refusal.
 */
#include "wubu_vkd3d_cmd.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_vkd3d_cmd ===\n");

    /* -- creation -- */
    CHECK(wubu_vkd3d_cmd_alloc_create(VKD3D_CMD_COUNT) == NULL, "an unknown type is rejected");
    CHECK(wubu_vkd3d_cmd_list_create(NULL) == NULL, "a list without an allocator is rejected");

    wubu_vkd3d_cmd_alloc_t *a = wubu_vkd3d_cmd_alloc_create(VKD3D_CMD_DIRECT);
    CHECK(a != NULL, "an allocator is created");
    CHECK(wubu_vkd3d_cmd_alloc_type(a) == VKD3D_CMD_DIRECT, "the allocator remembers its type");
    CHECK(wubu_vkd3d_cmd_alloc_lists(a) == 0, "the allocator starts with no lists");
    CHECK(wubu_vkd3d_cmd_alloc_next_fence(a) == 1, "fence 0 means never executed");

    wubu_vkd3d_cmd_list_t *l = wubu_vkd3d_cmd_list_create(a);
    CHECK(l != NULL, "a list is created");
    CHECK(wubu_vkd3d_cmd_list_state(l) == VKD3D_LIST_CREATED, "a new list is recording");
    CHECK(wubu_vkd3d_cmd_list_type(l) == VKD3D_CMD_DIRECT, "the list inherits the allocator's type");
    CHECK(wubu_vkd3d_cmd_list_fence(l) == 0, "a new list has never executed");
    CHECK(wubu_vkd3d_cmd_alloc_lists(a) == 1, "the allocator knows its list");

    /* -- the close rule -- */
    CHECK(wubu_vkd3d_cmd_list_execute(l) == -1, "a recording list cannot be executed");
    CHECK(wubu_vkd3d_cmd_list_close(l) == 0, "a recording list closes");
    CHECK(wubu_vkd3d_cmd_list_state(l) == VKD3D_LIST_CLOSED, "the list is closed");
    CHECK(wubu_vkd3d_cmd_list_close(l) == -1, "an already-closed list cannot be closed again");

    /* -- execute -- */
    CHECK(wubu_vkd3d_cmd_list_execute(l) == 0, "a closed list can be executed");
    CHECK(wubu_vkd3d_cmd_list_state(l) == VKD3D_LIST_EXECUTING, "the list is in flight");
    CHECK(wubu_vkd3d_cmd_list_fence(l) == 1, "the list carries the fence it was submitted with");

    /* -- the safety rule: never reuse command memory while the GPU reads it -- */
    CHECK(wubu_vkd3d_cmd_list_reset(l) == -1, "an in-flight list cannot be reset");
    CHECK(wubu_vkd3d_cmd_alloc_reset(a) == -1, "an allocator cannot be reset while a list is in flight");
    CHECK(wubu_vkd3d_cmd_list_state(l) == VKD3D_LIST_EXECUTING, "the refusal changed nothing");

    CHECK(wubu_vkd3d_cmd_list_complete(l) == 0, "the fence fires");
    CHECK(wubu_vkd3d_cmd_list_state(l) == VKD3D_LIST_CLOSED, "the list is closed again");
    CHECK(wubu_vkd3d_cmd_list_complete(l) == -1, "completing a non-executing list is an error");
    CHECK(wubu_vkd3d_cmd_list_reset(l) == 0, "a completed list can be reset");
    CHECK(wubu_vkd3d_cmd_list_state(l) == VKD3D_LIST_CREATED, "the list is recording again");
    CHECK(wubu_vkd3d_cmd_list_fence(l) == 0, "resetting clears the fence");

    /* -- allocator reset: only when nothing is in flight -- */
    {
        wubu_vkd3d_cmd_list_t *l1 = wubu_vkd3d_cmd_list_create(a);
        wubu_vkd3d_cmd_list_t *l2 = wubu_vkd3d_cmd_list_create(a);
        CHECK(l1 && l2, "two more lists");
        CHECK(wubu_vkd3d_cmd_alloc_lists(a) == 3, "the allocator tracks three lists");

        wubu_vkd3d_cmd_list_close(l1);
        wubu_vkd3d_cmd_list_execute(l1);
        CHECK(wubu_vkd3d_cmd_alloc_reset(a) == -1, "the allocator reset is refused while l1 is in flight");

        wubu_vkd3d_cmd_list_close(l2);
        wubu_vkd3d_cmd_list_complete(l1);
        CHECK(wubu_vkd3d_cmd_alloc_reset(a) == 0, "the allocator resets once nothing is in flight");

        /* every list from that allocator is now INVALID */
        CHECK(wubu_vkd3d_cmd_list_state(l1) == VKD3D_LIST_INVALID, "l1 is invalidated");
        CHECK(wubu_vkd3d_cmd_list_state(l2) == VKD3D_LIST_INVALID, "l2 is invalidated");
        CHECK(wubu_vkd3d_cmd_list_state(l) == VKD3D_LIST_INVALID, "the first list is invalidated too");
        CHECK(wubu_vkd3d_cmd_list_reset(l1) == -1, "an invalidated list cannot be reset");
        CHECK(wubu_vkd3d_cmd_list_close(l1) == -1, "an invalidated list cannot be closed");
        CHECK(wubu_vkd3d_cmd_list_execute(l1) == -1, "an invalidated list cannot be executed");

        /* destroying invalidated lists is still safe */
        wubu_vkd3d_cmd_list_destroy(l1);
        wubu_vkd3d_cmd_list_destroy(l2);
        CHECK(wubu_vkd3d_cmd_alloc_lists(a) == 1, "the destroyed lists are forgotten");
    }

    /* -- fence values are monotonic across executions -- */
    {
        wubu_vkd3d_cmd_list_t *f = wubu_vkd3d_cmd_list_create(a);
        CHECK(f != NULL, "a fresh list after the allocator reset");
        wubu_vkd3d_cmd_list_close(f);
        wubu_vkd3d_cmd_list_execute(f);
        /* l1 already consumed fence 2, so this is fence 3: the counter is
         * per-allocator and monotonic, and an allocator reset does NOT rewind
         * it (fences are monotonic by definition). */
        CHECK(wubu_vkd3d_cmd_list_fence(f) == 3, "the fence counter is monotonic");
        wubu_vkd3d_cmd_list_complete(f);
        wubu_vkd3d_cmd_list_reset(f);
        wubu_vkd3d_cmd_list_close(f);
        wubu_vkd3d_cmd_list_execute(f);
        CHECK(wubu_vkd3d_cmd_list_fence(f) == 4, "the counter keeps climbing");
        wubu_vkd3d_cmd_list_complete(f);
        wubu_vkd3d_cmd_list_destroy(f);
    }

    /* -- the Vulkan mapping -- */
    {
        wubu_vkd3d_cmd_vk_t vk;
        CHECK(wubu_vkd3d_cmd_vk(VKD3D_CMD_DIRECT, &vk), "DIRECT maps");
        CHECK(vk.valid && vk.level == VK_COMMAND_BUFFER_LEVEL_PRIMARY,
              "DIRECT is a primary buffer");
        CHECK(vk.queue_flags == VK_QUEUE_GRAPHICS_BIT, "DIRECT runs on the graphics queue");

        CHECK(wubu_vkd3d_cmd_vk(VKD3D_CMD_BUNDLE, &vk), "BUNDLE maps");
        CHECK(vk.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY,
              "a D3D12 bundle is a SECONDARY Vulkan buffer");

        CHECK(wubu_vkd3d_cmd_vk(VKD3D_CMD_COMPUTE, &vk), "COMPUTE maps");
        CHECK(vk.level == VK_COMMAND_BUFFER_LEVEL_PRIMARY,
              "Vulkan has no compute buffer level -- it is primary");
        CHECK(vk.queue_flags == VK_QUEUE_COMPUTE_BIT, "COMPUTE runs on the compute queue");

        CHECK(wubu_vkd3d_cmd_vk(VKD3D_CMD_COPY, &vk), "COPY maps");
        CHECK(vk.queue_flags == VK_QUEUE_TRANSFER_BIT, "COPY runs on the transfer queue");

        CHECK(!wubu_vkd3d_cmd_vk(VKD3D_CMD_COUNT, &vk), "an unknown type does not map");
        CHECK(wubu_vkd3d_cmd_pool_flags() == VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
              "every D3D12 allocator needs RESET_COMMAND_BUFFER (D3D12 resets single lists)");
    }

    wubu_vkd3d_cmd_alloc_destroy(a);   /* also destroys the lists it owns */
    wubu_vkd3d_cmd_alloc_destroy(NULL);
    wubu_vkd3d_cmd_list_destroy(NULL);

    if (failures == 0) printf("ALL VKD3D_CMD TESTS PASSED\n");
    else printf("%d VKD3D_CMD FAILURES\n", failures);
    return failures ? 1 : 0;
}
