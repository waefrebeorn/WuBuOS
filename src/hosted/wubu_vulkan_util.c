/*
 * wubu_vulkan_util.c -- WuBuOS Vulkan shared utilities
 *
 * Standalone helpers used across the Vulkan backend: VkResult decoding and
 * memory-type lookup. Self-contained; depends only on the public wubu_vulkan.h
 * API (and <stdint.h> for UINT32_MAX). C11, no god headers, opaque-safe.
 */

#include "wubu_vulkan.h"
#include <stdint.h>

/* -- VkResult decode ---------------------------------------------- */

const char *wubu_vk_result_string(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        default: return "VK_UNKNOWN";
    }
}

/* -- Memory Type Finder ------------------------------------------- */

uint32_t wubu_vk_find_memory_type(WubuVkAllocator *alloc,
                                  uint32_t type_filter,
                                  VkMemoryPropertyFlags props) {
    for (uint32_t i = 0; i < alloc->mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1u << i)) &&
            (alloc->mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

/* -- Allocator ----------------------------------------------------- */
/*
 * The three allocator entry points wubu_vulkan.h declares.  They were declared
 * but never defined, which is why nothing could link against the compute
 * helper -- the missing half of the Vulkan API, now written.
 */

int wubu_vk_allocator_create(WubuVkAllocator *alloc, WubuVkDevice *dev) {
    if (!alloc || !dev || !dev->device || !dev->phys_dev) return -1;
    alloc->device = dev->device;
    vkGetPhysicalDeviceMemoryProperties(dev->phys_dev->physical_device,
                                        &alloc->mem_props);
    return 0;
}

VkDeviceMemory wubu_vk_alloc(WubuVkAllocator *alloc,
                             VkMemoryRequirements *req,
                             VkMemoryPropertyFlags props) {
    if (!alloc || !req || req->size == 0) return VK_NULL_HANDLE;

    VkMemoryAllocateInfo ai = {0};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req->size;
    ai.memoryTypeIndex = wubu_vk_find_memory_type(alloc, req->memoryTypeBits, props);
    if (ai.memoryTypeIndex == UINT32_MAX) return VK_NULL_HANDLE;

    VkDeviceMemory mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(alloc->device, &ai, NULL, &mem) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return mem;
}

void wubu_vk_free(WubuVkAllocator *alloc, VkDeviceMemory mem) {
    if (alloc && mem) vkFreeMemory(alloc->device, mem, NULL);
}
