/*
 * wubu_vkd3d_heap.h -- VKD3D slice 2: D3D12 descriptor heaps.
 *
 * D3D12 describes resources through DESCRIPTORS -- fixed-size records living in
 * a heap.  The app gets a HANDLE (a heap + a byte offset), copies descriptors in
 * and out with memcpy semantics, and tells the GPU to read a range of them by
 * setting a descriptor table.  Vulkan has no equivalent object: descriptors live
 * in descriptor SETS, allocated from a descriptor POOL, and a table is a range of
 * bindings inside a set layout.
 *
 * So the translation has two halves, and both live here:
 *
 *   1. the heap itself -- a byte blob with a descriptor size, a type, and a
 *      GPU-visible flag.  This half is pure C11 and fully testable.
 *   2. the Vulkan mapping -- which VkDescriptorType and which pool/binding flags
 *      a heap of a given type needs.  GPU-visible heaps need UPDATE_AFTER_BIND,
 *      because the app writes into them after the set has been bound.
 *
 * Opaque struct, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_HEAP_H
#define WUBU_VKD3D_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

typedef enum {
    VKD3D_HEAP_CBV_SRV_UAV = 0,  /* constant / shader resource / UAV views */
    VKD3D_HEAP_SAMPLER,          /* samplers */
    VKD3D_HEAP_RTV,              /* render target views */
    VKD3D_HEAP_DSV,              /* depth stencil views */
    VKD3D_HEAP_COUNT
} wubu_vkd3d_heap_type_t;

/* A D3D12 descriptor handle: which heap, and a byte offset into it.
 * Offsets are always a multiple of the heap's descriptor size. */
typedef struct {
    uint32_t heap_id;
    uint32_t byte_offset;
} wubu_vkd3d_handle_t;

typedef struct wubu_vkd3d_heap wubu_vkd3d_heap_t;

/* Create a heap.  capacity is a DESCRIPTOR COUNT (not bytes); descriptor_size is
 * the device-reported size of one descriptor, and is configurable per device --
 * hardcoding 32 (the D3D12 default) is the classic mistake.
 * Returns NULL when capacity or descriptor_size is zero, or the type is
 * unknown. */
wubu_vkd3d_heap_t *wubu_vkd3d_heap_create(wubu_vkd3d_heap_type_t type,
                                          uint32_t capacity,
                                          uint32_t descriptor_size,
                                          bool gpu_visible);
void wubu_vkd3d_heap_destroy(wubu_vkd3d_heap_t *h);

uint32_t               wubu_vkd3d_heap_capacity(const wubu_vkd3d_heap_t *h);
uint32_t               wubu_vkd3d_heap_descriptor_size(const wubu_vkd3d_heap_t *h);
bool                   wubu_vkd3d_heap_gpu_visible(const wubu_vkd3d_heap_t *h);
wubu_vkd3d_heap_type_t wubu_vkd3d_heap_type(const wubu_vkd3d_heap_t *h);
uint32_t               wubu_vkd3d_heap_id(const wubu_vkd3d_heap_t *h);
uint32_t               wubu_vkd3d_heap_bytes(const wubu_vkd3d_heap_t *h);

/* Write one descriptor into a slot from a CPU-side blob of descriptor_size
 * bytes.  Returns -1 on a bad index or a NULL blob. */
int wubu_vkd3d_heap_write(wubu_vkd3d_heap_t *h, uint32_t index, const void *bytes);
/* Read one descriptor back out. */
int wubu_vkd3d_heap_read(const wubu_vkd3d_heap_t *h, uint32_t index, void *bytes);
/* The handle for a slot. */
wubu_vkd3d_handle_t wubu_vkd3d_heap_handle(const wubu_vkd3d_heap_t *h, uint32_t index);

/* Copy `count` descriptors.  D3D12 forbids cross-type copies, so a mismatch is
 * an error.  The destination is left UNCHANGED when the copy is rejected.
 * Overlapping copies within one heap are legal (memmove semantics). */
int wubu_vkd3d_heap_copy(wubu_vkd3d_heap_t *dst, uint32_t dst_index,
                         const wubu_vkd3d_heap_t *src, uint32_t src_index,
                         uint32_t count);

/* -- the Vulkan translation ----------------------------------------- */

typedef struct {
    VkDescriptorType            descriptor_type;
    /* VK_DESCRIPTOR_TYPE_MAX_ENUM for RTV/DSV: those are framebuffer
     * ATTACHMENTS in Vulkan, not descriptors at all. */
    VkDescriptorPoolCreateFlags pool_flags;      /* GPU-visible adds UPDATE_AFTER_BIND */
    VkDescriptorBindingFlags    binding_flags;
    bool                        mixed;           /* CBV_SRV_UAV holds several types */
    bool                        valid;
    const char                 *name;
} wubu_vkd3d_heap_vk_t;

/* Map a heap type to the Vulkan descriptor type + the pool/binding flags a
 * descriptor set built from it must carry. */
bool wubu_vkd3d_heap_vk(wubu_vkd3d_heap_type_t type, bool gpu_visible,
                        wubu_vkd3d_heap_vk_t *out);

/* The pool sizes a heap of `capacity` descriptors needs. */
void wubu_vkd3d_heap_pool_sizes(wubu_vkd3d_heap_type_t type, uint32_t capacity,
                                VkDescriptorPoolSize *out, uint32_t *n_out);

#endif /* WUBU_VKD3D_HEAP_H */
