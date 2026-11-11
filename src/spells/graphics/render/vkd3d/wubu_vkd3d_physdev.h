/*
 * wubu_vkd3d_physdev.h -- VKD3D slice 8: physical device selection.
 *
 * Every earlier slice opened no VkDevice, which is why their correctness is
 * provable without a GPU.  This slice is the bridge: the REQUIREMENT model is
 * pure logic over a descriptor and stays testable in plain C, while the live
 * enumerator is a thin wrapper over the Vulkan entry points.
 *
 * The rule that matters: D3D12 picks a device by FEATURE LEVEL; Vulkan picks by
 * explicit capability queries.  So "this device is good enough" is not a
 * comparison of numbers -- it is the conjunction of
 *   - API version >= what the app needs
 *   - every required queue family present
 *   - every required extension present
 *   - every required feature bit actually SET (a device advertising support is
 *     not enough; vkGetPhysicalDeviceFeatures reports what it REALLY has)
 * and the answer must be a REASON, not a boolean: an app that cannot create a
 * device needs to know which requirement failed, or it cannot fall back.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_PHYSDEV_H
#define WUBU_VKD3D_PHYSDEV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#define VKD3D_MAX_EXTENSIONS 16
#define VKD3D_MAX_QUEUES     32

typedef struct {
    VkPhysicalDevice            device;
    uint32_t                    api_version;
    VkPhysicalDeviceProperties  properties;
    VkPhysicalDeviceFeatures    features;
    uint32_t                    queue_family_count;
    VkQueueFamilyProperties     queue_families[VKD3D_MAX_QUEUES];
} wubu_vkd3d_physdev_t;

typedef struct {
    uint32_t                 min_api_version;
    VkFlags                  required_queue_flags;   /* VK_QUEUE_*_BIT */
    uint32_t                 extension_count;
    const char              *extensions[VKD3D_MAX_EXTENSIONS];
    VkPhysicalDeviceFeatures required_features;
} wubu_vkd3d_device_requirements_t;

/* Does this device satisfy the requirements?  Returns NULL when it does, else
 * a static reason naming the FIRST unmet requirement. */
const char *wubu_vkd3d_physdev_check(const wubu_vkd3d_physdev_t *p,
                                     const wubu_vkd3d_device_requirements_t *req);

/* The Vulkan translation of a queue-family requirement: which stage mask a
 * queue family can drive.  A family with only TRANSFER cannot present. */
bool wubu_vkd3d_physdev_can_present(const wubu_vkd3d_physdev_t *p, uint32_t family);

/* -- live path (needs a real instance) ------------------------------ */

int  wubu_vkd3d_physdev_enumerate(VkInstance inst, wubu_vkd3d_physdev_t *out,
                                  uint32_t max);
uint32_t wubu_vkd3d_physdev_api_version(uint32_t major, uint32_t minor,
                                        uint32_t patch);

#endif /* WUBU_VKD3D_PHYSDEV_H */
