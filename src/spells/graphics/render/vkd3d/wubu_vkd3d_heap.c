/*
 * wubu_vkd3d_heap.c -- VKD3D slice 2: D3D12 descriptor heaps.
 *
 * See wubu_vkd3d_heap.h.  The heap is a byte blob with a descriptor size and a
 * type; the Vulkan mapping says what a descriptor set built from it needs.
 */
#include "wubu_vkd3d_heap.h"

#include <stdlib.h>
#include <string.h>

struct wubu_vkd3d_heap {
    uint32_t               id;
    wubu_vkd3d_heap_type_t type;
    uint32_t               capacity;    /* descriptors */
    uint32_t               desc_size;   /* bytes per descriptor */
    bool                   gpu_visible;
    uint8_t               *bytes;
};

static uint32_t g_next_heap_id = 1;

wubu_vkd3d_heap_t *wubu_vkd3d_heap_create(wubu_vkd3d_heap_type_t type,
                                          uint32_t capacity,
                                          uint32_t descriptor_size,
                                          bool gpu_visible)
{
    if (type >= VKD3D_HEAP_COUNT) return NULL;
    if (capacity == 0 || descriptor_size == 0) return NULL;

    wubu_vkd3d_heap_t *h = calloc(1, sizeof(*h));
    if (!h) return NULL;
    h->bytes = calloc(capacity, descriptor_size);
    if (!h->bytes) { free(h); return NULL; }

    h->id = g_next_heap_id++;
    h->type = type;
    h->capacity = capacity;
    h->desc_size = descriptor_size;
    h->gpu_visible = gpu_visible;
    return h;
}

void wubu_vkd3d_heap_destroy(wubu_vkd3d_heap_t *h)
{
    if (!h) return;
    free(h->bytes);
    free(h);
}

uint32_t               wubu_vkd3d_heap_capacity(const wubu_vkd3d_heap_t *h)
{
    return h ? h->capacity : 0;
}

uint32_t               wubu_vkd3d_heap_descriptor_size(const wubu_vkd3d_heap_t *h)
{
    return h ? h->desc_size : 0;
}

bool                   wubu_vkd3d_heap_gpu_visible(const wubu_vkd3d_heap_t *h)
{
    return h ? h->gpu_visible : false;
}

wubu_vkd3d_heap_type_t wubu_vkd3d_heap_type(const wubu_vkd3d_heap_t *h)
{
    return h ? h->type : VKD3D_HEAP_COUNT;
}

uint32_t               wubu_vkd3d_heap_id(const wubu_vkd3d_heap_t *h)
{
    return h ? h->id : 0;
}

uint32_t               wubu_vkd3d_heap_bytes(const wubu_vkd3d_heap_t *h)
{
    return h ? h->capacity * h->desc_size : 0;
}

int wubu_vkd3d_heap_write(wubu_vkd3d_heap_t *h, uint32_t index, const void *bytes)
{
    if (!h || !bytes) return -1;
    if (index >= h->capacity) return -1;
    memcpy(h->bytes + (size_t)index * h->desc_size, bytes, h->desc_size);
    return 0;
}

int wubu_vkd3d_heap_read(const wubu_vkd3d_heap_t *h, uint32_t index, void *bytes)
{
    if (!h || !bytes) return -1;
    if (index >= h->capacity) return -1;
    memcpy(bytes, h->bytes + (size_t)index * h->desc_size, h->desc_size);
    return 0;
}

wubu_vkd3d_handle_t wubu_vkd3d_heap_handle(const wubu_vkd3d_heap_t *h, uint32_t index)
{
    wubu_vkd3d_handle_t hd = { 0, 0 };
    if (!h || index >= h->capacity) return hd;
    hd.heap_id = h->id;
    hd.byte_offset = index * h->desc_size;
    return hd;
}

int wubu_vkd3d_heap_copy(wubu_vkd3d_heap_t *dst, uint32_t dst_index,
                         const wubu_vkd3d_heap_t *src, uint32_t src_index,
                         uint32_t count)
{
    if (!dst || !src || count == 0) return -1;
    /* D3D12 forbids copying descriptors between heaps of different types, and
     * the descriptor sizes must agree or the records would be reinterpreted. */
    if (dst->type != src->type) return -1;
    if (dst->desc_size != src->desc_size) return -1;

    /* Bounds-checked BEFORE the copy, so a rejected copy leaves the
     * destination untouched rather than half-overwritten. */
    if (dst_index > dst->capacity || count > dst->capacity - dst_index) return -1;
    if (src_index > src->capacity || count > src->capacity - src_index) return -1;

    uint8_t *d = dst->bytes + (size_t)dst_index * dst->desc_size;
    const uint8_t *s = src->bytes + (size_t)src_index * src->desc_size;
    memmove(d, s, (size_t)count * dst->desc_size);   /* overlapping is legal */
    return 0;
}

/* -- the Vulkan translation ----------------------------------------- */

bool wubu_vkd3d_heap_vk(wubu_vkd3d_heap_type_t type, bool gpu_visible,
                        wubu_vkd3d_heap_vk_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));

#ifdef VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
    const VkDescriptorPoolCreateFlags uab =
        VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
#else
    const VkDescriptorPoolCreateFlags uab =
        VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
#endif

    switch (type) {
    case VKD3D_HEAP_CBV_SRV_UAV:
        /* One heap, three kinds of descriptor.  A Vulkan set layout can hold
         * all of them; the pool must therefore be sized for the union. */
        out->descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        out->mixed = true;
        out->name = "CBV_SRV_UAV";
        break;
    case VKD3D_HEAP_SAMPLER:
        out->descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER;
        out->name = "SAMPLER";
        break;
    case VKD3D_HEAP_RTV:
        out->descriptor_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;   /* an attachment */
        out->name = "RTV";
        break;
    case VKD3D_HEAP_DSV:
        out->descriptor_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;   /* an attachment */
        out->name = "DSV";
        break;
    default:
        return false;
    }

    if (gpu_visible) {
        out->pool_flags = uab;
        out->binding_flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    }
    out->valid = true;
    return true;
}

void wubu_vkd3d_heap_pool_sizes(wubu_vkd3d_heap_type_t type, uint32_t capacity,
                                VkDescriptorPoolSize *out, uint32_t *n_out)
{
    if (n_out) *n_out = 0;
    if (!out || !n_out || capacity == 0) return;

    switch (type) {
    case VKD3D_HEAP_CBV_SRV_UAV:
        /* The union of everything a CBV_SRV_UAV heap can hold. */
        out[(*n_out)++] = (VkDescriptorPoolSize){ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, capacity };
        out[(*n_out)++] = (VkDescriptorPoolSize){ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  capacity };
        out[(*n_out)++] = (VkDescriptorPoolSize){ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, capacity };
        out[(*n_out)++] = (VkDescriptorPoolSize){ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  capacity };
        break;
    case VKD3D_HEAP_SAMPLER:
        out[(*n_out)++] = (VkDescriptorPoolSize){ VK_DESCRIPTOR_TYPE_SAMPLER, capacity };
        break;
    case VKD3D_HEAP_RTV:
    case VKD3D_HEAP_DSV:
        /* Render target and depth stencil views are framebuffer ATTACHMENTS in
         * Vulkan -- they consume no descriptors, so the pool needs no size. */
        break;
    default:
        break;
    }
}
