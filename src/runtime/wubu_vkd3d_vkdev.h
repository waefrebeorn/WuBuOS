/*
 * wubu_vkd3d_vkdev.h -- VKD3D slice 9: the logical VkDevice.
 *
 * Slice 8 picks the physical device; this one creates the logical device on
 * it.  Vulkan's device creation is where the most avoidable failures live,
 * because every field is a request and the driver only rejects combinations it
 * cannot satisfy:
 *
 *   - a queue request must name a family index that EXISTS
 *   - a family's queueCount must not exceed what the device reported
 *   - a family must not be requested TWICE (two DeviceQueueCreateInfos for one
 *     family is invalid; the right way is one info with queueCount = N)
 *   - every requested queue must carry a priority
 *
 * D3D12 hides all of this: the app asks for a device and the runtime picks the
 * queues.  A translation layer must make the request explicitly, and it must
 * VALIDATE it before calling vkCreateDevice -- a driver error there is a
 * generic VK_ERROR_INITIALIZATION_FAILED with no indication of which field was
 * wrong, and the app cannot fall back.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_VKDEV_H
#define WUBU_VKD3D_VKDEV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "wubu_vkd3d_physdev.h"

#define VKD3D_MAX_QUEUE_REQUESTS 8

typedef struct {
    uint32_t family_index;
    uint32_t queue_count;
    float    priority;
} wubu_vkd3d_queue_request_t;

typedef struct {
    uint32_t                      queue_request_count;
    wubu_vkd3d_queue_request_t    queues[VKD3D_MAX_QUEUE_REQUESTS];
    uint32_t                      extension_count;
    const char                   *extensions[VKD3D_MAX_EXTENSIONS];
    VkPhysicalDeviceFeatures      features;
} wubu_vkd3d_vkdev_desc_t;

/* Validate a desc against a physical device.  NULL when legal, else a static
 * reason naming the first problem. */
const char *wubu_vkd3d_vkdev_validate(const wubu_vkd3d_physdev_t *p,
                                     const wubu_vkd3d_vkdev_desc_t *d);

/* The Vulkan translation: one VkDeviceQueueCreateInfo per request.  Returns the
 * number written, or -1 on an invalid desc. */
int wubu_vkd3d_vkdev_queue_create_infos(const wubu_vkd3d_physdev_t *p,
                                        const wubu_vkd3d_vkdev_desc_t *d,
                                        VkDeviceQueueCreateInfo *out,
                                        uint32_t max);

/* Total queues requested. */
uint32_t wubu_vkd3d_vkdev_total_queues(const wubu_vkd3d_vkdev_desc_t *d);

/* -- live path ------------------------------------------------------ */

/* Create the logical device.  Returns false on an invalid desc or a driver
 * failure (the reason is reported by the validator). */
bool wubu_vkd3d_vkdev_create(const wubu_vkd3d_physdev_t *p,
                             const wubu_vkd3d_vkdev_desc_t *d,
                             VkDevice *out);

#endif /* WUBU_VKD3D_VKDEV_H */
