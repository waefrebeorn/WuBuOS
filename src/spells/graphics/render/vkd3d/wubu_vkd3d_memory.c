/*
 * wubu_vkd3d_memory.c -- VKD3D slice 12: D3D12 heaps -> Vulkan memory types.
 *
 * See wubu_vkd3d_memory.h.
 */
#include "wubu_vkd3d_memory.h"

#include <string.h>

bool wubu_vkd3d_heap_properties(wubu_vkd3d_heap_type_t heap,
                                VkMemoryPropertyFlags *out)
{
    if (!out) return false;
    *out = 0;
    switch (heap) {
    case VKD3D_HEAP_DEFAULT:
        /* Device-local only: this is what makes a render target fast. */
        *out = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        return true;
    case VKD3D_HEAP_UPLOAD:
        /* CPU writes, GPU reads: the CPU must see it, and coherency is what
         * makes a plain memcpy visible to the GPU without an invalidate. */
        *out = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        return true;
    case VKD3D_HEAP_READBACK:
        /* GPU writes, CPU reads: host-visible, and CACHED is explicitly
         * allowed -- requiring coherent here would exclude the fastest type
         * most devices offer for readback. */
        *out = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        return true;
    default:
        return false;
    }
}

bool wubu_vkd3d_heap_needs_host_visible(wubu_vkd3d_heap_type_t heap)
{
    return heap == VKD3D_HEAP_UPLOAD || heap == VKD3D_HEAP_READBACK;
}

int wubu_vkd3d_memory_type_index(const wubu_vkd3d_memory_types_t *t,
                                 const wubu_vkd3d_alloc_request_t *req,
                                 const char **why)
{
    if (why) *why = NULL;
    if (!t || !req) {
        if (why) *why = "no memory types or no request";
        return -1;
    }

    VkMemoryPropertyFlags want;
    if (!wubu_vkd3d_heap_properties(req->heap_type, &want)) {
        if (why) *why = "an unknown D3D12 heap type";
        return -1;
    }
    /* The app's extra requirements are CONJUNCTIVE with the heap's. */
    want |= req->required;

    if (req->type_bits == 0) {
        if (why) *why = "the resource reports no compatible memory types";
        return -1;
    }

    /* READBACK prefers a HOST_CACHED type: for GPU-writes/CPU-reads, cached
     * reads are markedly faster than coherent-but-uncached.  This is a
     * PREFERENCE, not a requirement -- the second pass accepts either. */
    VkMemoryPropertyFlags bonus = 0;
    if (req->heap_type == VKD3D_HEAP_READBACK)
        bonus = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    for (int pass = 0; pass < 2; pass++) {
        VkMemoryPropertyFlags need = (pass == 0) ? (want | bonus) : want;
        for (uint32_t i = 0; i < t->count && i < VKD3D_MAX_MEMORY_TYPES; i++) {
            if (!((req->type_bits >> i) & 1u)) continue;
            const VkMemoryType *mt = &t->types[i];
            if ((mt->propertyFlags & need) == need) return (int)i;
        }
    }

    /* Report WHICH requirement failed -- 'no memory' is not actionable. */
    if (why) {
        for (uint32_t i = 0; i < t->count && i < VKD3D_MAX_MEMORY_TYPES; i++) {
            if (!((req->type_bits >> i) & 1u)) continue;
            const VkMemoryType *mt = &t->types[i];
            if (!(mt->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                && (want & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                *why = "no host-visible memory type is compatible with this resource";
                return -1;
            }
            if (!(mt->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                && (want & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                *why = "no device-local memory type is compatible with this resource";
                return -1;
            }
        }
        *why = "no memory type satisfies the heap's properties";
    }
    return -1;
}
