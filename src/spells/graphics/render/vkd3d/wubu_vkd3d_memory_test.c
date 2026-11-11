/*
 * wubu_vkd3d_memory_test.c -- D3D12 heaps -> Vulkan memory types.
 *
 * The load-bearing rule: the chosen type must satisfy the heap's properties,
 * the app's extra requirements, AND the resource's memoryTypeBits.  A type
 * with the right properties but outside the mask cannot back the resource.
 */
#include "wubu_vkd3d_memory.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

/* A typical desktop device: 0 = device-local, 1 = host-visible+coherent,
 * 2 = host-visible+cached (readback). */
static void typical_device(wubu_vkd3d_memory_types_t *t)
{
    memset(t, 0, sizeof(*t));
    t->count = 3;
    t->types[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    t->types[0].heapIndex = 0;
    t->types[1].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                              | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    t->types[1].heapIndex = 1;
    t->types[2].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                              | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    t->types[2].heapIndex = 1;
}

static void req(wubu_vkd3d_alloc_request_t *r, wubu_vkd3d_heap_type_t heap,
                uint32_t type_bits)
{
    memset(r, 0, sizeof(*r));
    r->size = 1024 * 1024;
    r->heap_type = heap;
    r->type_bits = type_bits;
}

int main(void)
{
    printf("=== test_vkd3d_memory ===\n");
    wubu_vkd3d_memory_types_t t;
    wubu_vkd3d_alloc_request_t r;
    VkMemoryPropertyFlags p;
    const char *why = NULL;

    /* -- the heap -> property mapping -- */
    CHECK(wubu_vkd3d_heap_properties(VKD3D_HEAP_DEFAULT, &p) &&
          p == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          "DEFAULT -> device-local");
    CHECK(wubu_vkd3d_heap_properties(VKD3D_HEAP_UPLOAD, &p) &&
          p == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
          "UPLOAD -> host-visible + coherent");
    CHECK(wubu_vkd3d_heap_properties(VKD3D_HEAP_READBACK, &p) &&
          p == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
          "READBACK -> host-visible, WITHOUT requiring coherent");
    CHECK(!wubu_vkd3d_heap_properties((wubu_vkd3d_heap_type_t)99, &p),
          "an unknown heap type is rejected");
    CHECK(!wubu_vkd3d_heap_properties(VKD3D_HEAP_DEFAULT, NULL),
          "a null out pointer is rejected");

    /* -- host visibility -- */
    CHECK(!wubu_vkd3d_heap_needs_host_visible(VKD3D_HEAP_DEFAULT),
          "DEFAULT does not need host visibility");
    CHECK(wubu_vkd3d_heap_needs_host_visible(VKD3D_HEAP_UPLOAD),
          "UPLOAD needs host visibility");
    CHECK(wubu_vkd3d_heap_needs_host_visible(VKD3D_HEAP_READBACK),
          "READBACK needs host visibility");

    /* -- selection -- */
    typical_device(&t);

    req(&r, VKD3D_HEAP_DEFAULT, 0x05);      /* types 0 and 2 */
    CHECK(wubu_vkd3d_memory_type_index(&t, &r, &why) == 0,
          "DEFAULT picks the device-local type");
    req(&r, VKD3D_HEAP_DEFAULT, 0x02);      /* only type 1 (host) */
    CHECK(wubu_vkd3d_memory_type_index(&t, &r, &why) == -1,
          "DEFAULT cannot fall back to host-visible memory");
    CHECK(why && strstr(why, "device-local"),
          "and the reason names what was missing");

    req(&r, VKD3D_HEAP_UPLOAD, 0x06);       /* types 1 and 2 */
    CHECK(wubu_vkd3d_memory_type_index(&t, &r, &why) == 1,
          "UPLOAD picks the coherent host type");
    req(&r, VKD3D_HEAP_UPLOAD, 0x04);       /* only type 2 (cached, no coherent) */
    CHECK(wubu_vkd3d_memory_type_index(&t, &r, &why) == -1,
          "UPLOAD refuses a non-coherent type");

    req(&r, VKD3D_HEAP_READBACK, 0x06);
    CHECK(wubu_vkd3d_memory_type_index(&t, &r, &why) == 2,
          "READBACK prefers the CACHED type (the fastest for CPU reads)");
    req(&r, VKD3D_HEAP_READBACK, 0x02);
    CHECK(wubu_vkd3d_memory_type_index(&t, &r, &why) == 1,
          "READBACK accepts a coherent host type when that is all there is");

    /* -- the memoryTypeBits mask is a HARD constraint -- */
    req(&r, VKD3D_HEAP_DEFAULT, 0x02);      /* device-local exists but is masked out */
    CHECK(wubu_vkd3d_memory_type_index(&t, &r, &why) == -1,
          "a type outside memoryTypeBits is not usable, however suitable");
    req(&r, VKD3D_HEAP_DEFAULT, 0x00);
    CHECK(wubu_vkd3d_memory_type_index(&t, &r, &why) == -1,
          "a resource with no compatible types is an error");

    /* -- extra app requirements are conjunctive -- */
    typical_device(&t);
    req(&r, VKD3D_HEAP_DEFAULT, 0x07);
    r.required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    CHECK(wubu_vkd3d_memory_type_index(&t, &r, &why) == -1,
          "an app requirement on top of the heap's is conjunctive");

    /* -- a device with no device-local memory at all -- */
    {
        wubu_vkd3d_memory_types_t flat;
        memset(&flat, 0, sizeof(flat));
        flat.count = 1;
        flat.types[0].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        req(&r, VKD3D_HEAP_DEFAULT, 0x01);
        CHECK(wubu_vkd3d_memory_type_index(&flat, &r, &why) == -1,
              "a device with no device-local memory cannot satisfy DEFAULT");
        req(&r, VKD3D_HEAP_UPLOAD, 0x01);
        CHECK(wubu_vkd3d_memory_type_index(&flat, &r, &why) == 0,
              "the same device is fine for UPLOAD");
    }

    /* -- null handling -- */
    CHECK(wubu_vkd3d_memory_type_index(NULL, &r, &why) == -1,
          "null types are rejected");
    typical_device(&t);
    CHECK(wubu_vkd3d_memory_type_index(&t, NULL, &why) == -1,
          "null requests are rejected");

    if (failures == 0) printf("ALL VKD3D_MEMORY TESTS PASSED\n");
    else printf("%d VKD3D_MEMORY FAILURES\n", failures);
    return failures ? 1 : 0;
}
