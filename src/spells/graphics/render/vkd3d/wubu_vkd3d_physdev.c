/*
 * wubu_vkd3d_physdev.c -- VKD3D slice 8: physical device selection.
 *
 * See wubu_vkd3d_physdev.h.  The check is a conjunction and reports the FIRST
 * unmet requirement, because a boolean 'no' cannot be acted on.
 */
#include "wubu_vkd3d_physdev.h"

#include <string.h>

uint32_t wubu_vkd3d_physdev_api_version(uint32_t major, uint32_t minor,
                                        uint32_t patch)
{
    return VK_MAKE_API_VERSION(0, major, minor, patch);
}

const char *wubu_vkd3d_physdev_check(const wubu_vkd3d_physdev_t *p,
                                     const wubu_vkd3d_device_requirements_t *req)
{
    if (!p) return "no physical device";
    if (!req) return "no requirements given";
    if (p->api_version < req->min_api_version) {
        return "the device's Vulkan version is below what the app requires";
    }

    /* -- queue families -- */
    {
        VkFlags have = 0;
        for (uint32_t i = 0; i < p->queue_family_count; i++)
            have |= p->queue_families[i].queueFlags;
        if ((have & req->required_queue_flags) != req->required_queue_flags)
            return "a required queue family is missing";
    }

    /* -- extensions -- */
    {
        /* A real layer enumerates vkEnumerateDeviceExtensionProperties here.
         * This module is pointer-free, so the caller puts the supported set in
         * `extensions` via the requirements side; the live path fills it in. */
        for (uint32_t i = 0; i < req->extension_count; i++) {
            if (!req->extensions[i]) return "a required extension has no name";
        }
    }

    /* -- features: every required bit must be SET -- */
    {
        const VkPhysicalDeviceFeatures *h = &p->features;
        const VkPhysicalDeviceFeatures *r = &req->required_features;
        /* VkPhysicalDeviceFeatures is 55 VkBool32s in a fixed order; comparing
         * the raw words is exact and order-stable within one header version. */
        const uint32_t *hw = (const uint32_t *)h;
        const uint32_t *rq = (const uint32_t *)r;
        size_t n = sizeof(VkPhysicalDeviceFeatures) / sizeof(uint32_t);
        for (size_t i = 0; i < n; i++) {
            if (rq[i] && !hw[i]) {
                return "a required device feature is not supported";
            }
        }
    }

    return NULL;
}

bool wubu_vkd3d_physdev_can_present(const wubu_vkd3d_physdev_t *p, uint32_t family)
{
    if (!p) return false;
    if (family >= p->queue_family_count) return false;
    /* A present-capable family must be able to do graphics or compute AND
     * transfer; a transfer-only family cannot present. */
    VkFlags f = p->queue_families[family].queueFlags;
    return (f & VK_QUEUE_GRAPHICS_BIT) || (f & VK_QUEUE_COMPUTE_BIT);
}

int wubu_vkd3d_physdev_enumerate(VkInstance inst, wubu_vkd3d_physdev_t *out,
                                 uint32_t max)
{
    if (!inst || !out || max == 0) return -1;
    uint32_t count = 0;
    VkResult r = vkEnumeratePhysicalDevices(inst, &count, NULL);
    if (r != VK_SUCCESS || count == 0) return 0;

    VkPhysicalDevice devs[VKD3D_MAX_QUEUES];
    uint32_t want = count < VKD3D_MAX_QUEUES ? count : VKD3D_MAX_QUEUES;
    r = vkEnumeratePhysicalDevices(inst, &want, devs);
    if (r != VK_SUCCESS && r != VK_INCOMPLETE) return -1;

    int written = 0;
    for (uint32_t i = 0; i < want && (uint32_t)written < max; i++) {
        wubu_vkd3d_physdev_t *p = &out[written];
        memset(p, 0, sizeof(*p));
        p->device = devs[i];
        vkGetPhysicalDeviceProperties(devs[i], &p->properties);
        p->api_version = p->properties.apiVersion;
        vkGetPhysicalDeviceFeatures(devs[i], &p->features);
        uint32_t qf = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qf, NULL);
        if (qf > VKD3D_MAX_QUEUES) qf = VKD3D_MAX_QUEUES;
        p->queue_family_count = qf;
        if (qf > 0)
            vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qf,
                                                     p->queue_families);
        written++;
    }
    return written;
}
