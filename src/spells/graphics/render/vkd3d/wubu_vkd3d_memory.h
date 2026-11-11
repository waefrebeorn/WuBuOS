/*
 * wubu_vkd3d_memory.h -- VKD3D slice 12: D3D12 heaps -> Vulkan memory types.
 *
 * D3D12 has three heap types and picks the memory for you; Vulkan makes the
 * app choose a memory TYPE out of a device-reported array.  The mapping is the
 * whole slice:
 *
 *   D3D12 DEFAULT   (render targets, textures, default buffers)
 *                   -> VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
 *   D3D12 UPLOAD    (CPU writes, GPU reads -- vertex staging, constant updates)
 *                   -> HOST_VISIBLE | HOST_COHERENT
 *   D3D12 READBACK  (GPU writes, CPU reads -- screenshot readback, query data)
 *                   -> HOST_VISIBLE | HOST_CACHED
 *
 * The rules that are easy to get wrong:
 *
 *   * UPLOAD must be HOST_VISIBLE or the CPU cannot write it, and
 *     HOST_COHERENT or the writes are not guaranteed visible to the GPU
 *     without an explicit invalidate.
 *   * READBACK must NOT require HOST_COHERENT: cached reads are faster, and a
 *     coherent-but-uncached type would silently cost performance.
 *   * The chosen type must satisfy BOTH the app's required properties AND the
 *     device's memoryTypeBits for that resource.  A type that has the right
 *     properties but is not in the mask cannot back the resource at all.
 *   * A missing memory type is an ERROR, not a fallback: picking a slower type
 *     than the heap requires turns a staging upload into a stall.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_MEMORY_H
#define WUBU_VKD3D_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#define VKD3D_MAX_MEMORY_TYPES 32

typedef enum {
    VKD3D_HEAP_DEFAULT = 1,
    VKD3D_HEAP_UPLOAD = 2,
    VKD3D_HEAP_READBACK = 3
} wubu_vkd3d_heap_type_t;

typedef struct {
    VkDeviceSize           size;
    wubu_vkd3d_heap_type_t heap_type;
    VkMemoryPropertyFlags  required;      /* what the app demands, on top */
    uint32_t               type_bits;      /* VkMemoryRequirements::memoryTypeBits */
} wubu_vkd3d_alloc_request_t;

typedef struct {
    uint32_t     count;
    VkMemoryType types[VKD3D_MAX_MEMORY_TYPES];
} wubu_vkd3d_memory_types_t;

/* The memory properties a D3D12 heap type implies. */
bool wubu_vkd3d_heap_properties(wubu_vkd3d_heap_type_t heap,
                                VkMemoryPropertyFlags *out);

/* Choose a memory type index.  Returns the index, or -1 and a static reason.
 * The type must satisfy the heap's properties, the app's extra requirements,
 * AND the resource's memoryTypeBits. */
int wubu_vkd3d_memory_type_index(const wubu_vkd3d_memory_types_t *t,
                                 const wubu_vkd3d_alloc_request_t *req,
                                 const char **why);

/* Does a heap type need HOST_VISIBLE? */
bool wubu_vkd3d_heap_needs_host_visible(wubu_vkd3d_heap_type_t heap);

#endif /* WUBU_VKD3D_MEMORY_H */
