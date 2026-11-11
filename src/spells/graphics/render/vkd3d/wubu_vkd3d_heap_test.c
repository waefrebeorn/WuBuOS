/*
 * wubu_vkd3d_heap_test.c -- the descriptor-heap slice, proven without a GPU.
 */
#include "wubu_vkd3d_heap.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_vkd3d_heap ===\n");

    /* -- creation -- */
    CHECK(wubu_vkd3d_heap_create(VKD3D_HEAP_CBV_SRV_UAV, 0, 32, false) == NULL,
          "a zero capacity is rejected");
    CHECK(wubu_vkd3d_heap_create(VKD3D_HEAP_CBV_SRV_UAV, 8, 0, false) == NULL,
          "a zero descriptor size is rejected");
    CHECK(wubu_vkd3d_heap_create(VKD3D_HEAP_COUNT, 8, 32, false) == NULL,
          "an unknown type is rejected");

    wubu_vkd3d_heap_t *h = wubu_vkd3d_heap_create(VKD3D_HEAP_CBV_SRV_UAV, 8, 32, false);
    CHECK(h != NULL, "a valid heap is created");
    CHECK(wubu_vkd3d_heap_capacity(h) == 8, "capacity is remembered");
    CHECK(wubu_vkd3d_heap_descriptor_size(h) == 32, "descriptor size is remembered");
    CHECK(wubu_vkd3d_heap_bytes(h) == 8 * 32, "byte size is capacity * descriptor size");
    CHECK(wubu_vkd3d_heap_gpu_visible(h) == false, "CPU-only heap is not GPU-visible");
    CHECK(wubu_vkd3d_heap_id(h) != 0, "the heap has an id");

    /* -- write / read round-trip -- */
    uint8_t in[32], out[32];
    memset(in, 0xAB, sizeof(in));
    CHECK(wubu_vkd3d_heap_write(h, 0, in) == 0, "write slot 0");
    memset(out, 0, sizeof(out));
    CHECK(wubu_vkd3d_heap_read(h, 0, out) == 0, "read slot 0");
    CHECK(memcmp(in, out, sizeof(in)) == 0, "the descriptor round-trips");
    CHECK(wubu_vkd3d_heap_write(h, 7, in) == 0, "the last slot is writable");
    CHECK(wubu_vkd3d_heap_write(h, 8, in) == -1, "one past the end is rejected");
    CHECK(wubu_vkd3d_heap_write(h, 0, NULL) == -1, "a NULL blob is rejected");
    CHECK(wubu_vkd3d_heap_read(h, 9, out) == -1, "reading past the end is rejected");

    /* -- handles are heap id + byte offset -- */
    wubu_vkd3d_handle_t hd = wubu_vkd3d_heap_handle(h, 3);
    CHECK(hd.heap_id == wubu_vkd3d_heap_id(h), "the handle carries the heap id");
    CHECK(hd.byte_offset == 3 * 32, "the handle offset is index * descriptor size");
    hd = wubu_vkd3d_heap_handle(h, 8);
    CHECK(hd.heap_id == 0 && hd.byte_offset == 0, "an out-of-range handle is zeroed");

    /* -- copy within one heap, overlapping (memmove semantics) -- */
    {
        uint8_t a[16], b[16];
        memset(a, 0x11, sizeof(a));
        memset(b, 0x22, sizeof(b));
        CHECK(wubu_vkd3d_heap_write(h, 0, a) == 0, "seed slot 0");
        CHECK(wubu_vkd3d_heap_write(h, 1, b) == 0, "seed slot 1");
        /* 2 descriptors from slot 0 to slot 1: the ranges overlap. */
        CHECK(wubu_vkd3d_heap_copy(h, 1, h, 0, 2) == 0, "an overlapping self-copy is legal");
        uint8_t back[32];
        CHECK(wubu_vkd3d_heap_read(h, 1, back) == 0, "read back after self-copy");
        CHECK(back[0] == 0x11, "memmove, not memcpy: the overlap is correct");
    }

    /* -- copy between heaps -- */
    {
        wubu_vkd3d_heap_t *src = wubu_vkd3d_heap_create(VKD3D_HEAP_CBV_SRV_UAV, 8, 32, false);
        wubu_vkd3d_heap_t *dst = wubu_vkd3d_heap_create(VKD3D_HEAP_CBV_SRV_UAV, 8, 32, false);
        CHECK(src && dst, "two heaps created");
        CHECK(wubu_vkd3d_heap_id(src) != wubu_vkd3d_heap_id(dst), "heap ids are distinct");

        uint8_t v[32]; memset(v, 0x5C, sizeof(v));
        CHECK(wubu_vkd3d_heap_write(src, 2, v) == 0, "seed the source heap");
        CHECK(wubu_vkd3d_heap_copy(dst, 5, src, 2, 1) == 0, "cross-heap copy succeeds");
        uint8_t back[32]; memset(back, 0, sizeof(back));
        CHECK(wubu_vkd3d_heap_read(dst, 5, back) == 0, "read the destination");
        CHECK(memcmp(back, v, sizeof(v)) == 0, "the descriptor moved between heaps");

        /* cross-type is forbidden */
        wubu_vkd3d_heap_t *rtv = wubu_vkd3d_heap_create(VKD3D_HEAP_RTV, 8, 32, false);
        CHECK(rtv != NULL, "an RTV heap is created");
        CHECK(wubu_vkd3d_heap_copy(rtv, 0, src, 0, 1) == -1,
              "a cross-type copy is rejected");
        CHECK(wubu_vkd3d_heap_copy(src, 0, rtv, 0, 1) == -1,
              "a cross-type copy is rejected in both directions");

        /* a mismatched descriptor size is forbidden (records would be
         * reinterpreted at the wrong stride) */
        wubu_vkd3d_heap_t *wide = wubu_vkd3d_heap_create(VKD3D_HEAP_CBV_SRV_UAV, 8, 64, false);
        CHECK(wide != NULL, "a heap with a different descriptor size");
        CHECK(wubu_vkd3d_heap_copy(wide, 0, src, 0, 1) == -1,
              "a mismatched descriptor size is rejected");

        /* an overflowing copy is rejected AND leaves the destination intact */
        uint8_t before[32]; memset(before, 0x77, sizeof(before));
        CHECK(wubu_vkd3d_heap_write(dst, 6, before) == 0, "seed dst slot 6");
        CHECK(wubu_vkd3d_heap_copy(dst, 6, src, 0, 4) == -1,
              "a copy past the destination end is rejected");
        uint8_t after[32]; memset(after, 0, sizeof(after));
        CHECK(wubu_vkd3d_heap_read(dst, 6, after) == 0, "read dst after rejection");
        CHECK(memcmp(before, after, sizeof(before)) == 0,
              "a rejected copy leaves the destination unchanged");

        wubu_vkd3d_heap_destroy(rtv);
        wubu_vkd3d_heap_destroy(wide);
        wubu_vkd3d_heap_destroy(src);
        wubu_vkd3d_heap_destroy(dst);
    }

    /* -- the Vulkan mapping -- */
    {
        wubu_vkd3d_heap_vk_t vk;
        CHECK(wubu_vkd3d_heap_vk(VKD3D_HEAP_CBV_SRV_UAV, false, &vk), "CBV_SRV_UAV maps");
        CHECK(vk.valid && vk.mixed, "CBV_SRV_UAV is a MIXED heap");
        CHECK(vk.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              "CBV_SRV_UAV names its representative type");
        CHECK(vk.pool_flags == 0, "a CPU-only heap needs no UPDATE_AFTER_BIND");

        CHECK(wubu_vkd3d_heap_vk(VKD3D_HEAP_SAMPLER, false, &vk), "SAMPLER maps");
        CHECK(!vk.mixed && vk.descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER,
              "SAMPLER maps to the sampler descriptor type");

        /* RTV/DSV are ATTACHMENTS in Vulkan, not descriptors at all */
        CHECK(wubu_vkd3d_heap_vk(VKD3D_HEAP_RTV, false, &vk), "RTV maps");
        CHECK(vk.descriptor_type == VK_DESCRIPTOR_TYPE_MAX_ENUM,
              "RTV is not a descriptor -- it is a framebuffer attachment");
        CHECK(wubu_vkd3d_heap_vk(VKD3D_HEAP_DSV, false, &vk), "DSV maps");
        CHECK(vk.descriptor_type == VK_DESCRIPTOR_TYPE_MAX_ENUM,
              "DSV is not a descriptor either");

        /* GPU-visible heaps must be updatable after bind */
        CHECK(wubu_vkd3d_heap_vk(VKD3D_HEAP_CBV_SRV_UAV, true, &vk),
              "a GPU-visible heap maps");
        CHECK(vk.pool_flags != 0, "a GPU-visible heap needs UPDATE_AFTER_BIND in the pool");
        CHECK(vk.binding_flags != 0, "a GPU-visible heap needs UPDATE_AFTER_BIND on the binding");

        CHECK(!wubu_vkd3d_heap_vk(VKD3D_HEAP_COUNT, false, &vk), "an unknown type does not map");
    }

    /* -- pool sizing -- */
    {
        VkDescriptorPoolSize sizes[8]; uint32_t n = 0;
        wubu_vkd3d_heap_pool_sizes(VKD3D_HEAP_CBV_SRV_UAV, 100, sizes, &n);
        CHECK(n == 4, "a CBV_SRV_UAV pool needs four descriptor types");
        CHECK(sizes[0].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
              sizes[0].descriptorCount == 100, "the pool is sized to the heap capacity");

        n = 0;
        wubu_vkd3d_heap_pool_sizes(VKD3D_HEAP_SAMPLER, 100, sizes, &n);
        CHECK(n == 1 && sizes[0].type == VK_DESCRIPTOR_TYPE_SAMPLER,
              "a sampler pool needs one descriptor type");

        n = 99;
        wubu_vkd3d_heap_pool_sizes(VKD3D_HEAP_RTV, 100, sizes, &n);
        CHECK(n == 0, "an RTV pool needs no descriptors");
        n = 99;
        wubu_vkd3d_heap_pool_sizes(VKD3D_HEAP_DSV, 100, sizes, &n);
        CHECK(n == 0, "a DSV pool needs no descriptors");
    }

    wubu_vkd3d_heap_destroy(h);
    wubu_vkd3d_heap_destroy(NULL);      /* must be a safe no-op */

    if (failures == 0) printf("ALL VKD3D_HEAP TESTS PASSED\n");
    else printf("%d VKD3D_HEAP FAILURES\n", failures);
    return failures ? 1 : 0;
}
