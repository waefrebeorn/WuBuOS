/*
 * wubu_vkd3d_vkdev.c -- VKD3D slice 9: the logical VkDevice.
 *
 * See wubu_vkd3d_vkdev.h.  Every check here exists because vkCreateDevice
 * answers a bad request with a generic failure and no field name.
 */
#include "wubu_vkd3d_vkdev.h"

#include <string.h>

uint32_t wubu_vkd3d_vkdev_total_queues(const wubu_vkd3d_vkdev_desc_t *d)
{
    if (!d) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < d->queue_request_count; i++)
        n += d->queues[i].queue_count;
    return n;
}

const char *wubu_vkd3d_vkdev_validate(const wubu_vkd3d_physdev_t *p,
                                     const wubu_vkd3d_vkdev_desc_t *d)
{
    if (!p) return "no physical device";
    if (!d) return "no device description";
    if (d->queue_request_count == 0)
        return "a device needs at least one queue";
    if (d->queue_request_count > VKD3D_MAX_QUEUE_REQUESTS)
        return "too many queue requests";

    for (uint32_t i = 0; i < d->extension_count; i++) {
        if (!d->extensions[i]) return "a requested extension has no name";
    }

    for (uint32_t i = 0; i < d->queue_request_count; i++) {
        const wubu_vkd3d_queue_request_t *q = &d->queues[i];

        if (q->family_index >= p->queue_family_count)
            return "a queue request names a family index that does not exist";
        if (q->queue_count == 0)
            return "a queue request asks for zero queues";

        /* Two DeviceQueueCreateInfos for one family is invalid; the right way
         * is a single info with queueCount = N.  Catch it here rather than
         * letting the driver hand back a generic failure. */
        for (uint32_t j = i + 1; j < d->queue_request_count; j++) {
            if (d->queues[j].family_index == q->family_index)
                return "the same queue family is requested twice";
        }

        uint32_t avail = p->queue_families[q->family_index].queueCount;
        if (q->queue_count > avail)
            return "a queue request asks for more queues than the family has";

        if (q->priority < 0.0f || q->priority > 1.0f)
            return "a queue priority is outside 0.0..1.0";
    }
    return NULL;
}

int wubu_vkd3d_vkdev_queue_create_infos(const wubu_vkd3d_physdev_t *p,
                                        const wubu_vkd3d_vkdev_desc_t *d,
                                        VkDeviceQueueCreateInfo *out,
                                        uint32_t max)
{
    if (wubu_vkd3d_vkdev_validate(p, d) != NULL) return -1;
    if (!out || max < d->queue_request_count) return -1;

    for (uint32_t i = 0; i < d->queue_request_count; i++) {
        const wubu_vkd3d_queue_request_t *q = &d->queues[i];
        VkDeviceQueueCreateInfo *o = &out[i];
        memset(o, 0, sizeof(*o));
        o->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        o->queueFamilyIndex = q->family_index;
        o->queueCount = q->queue_count;
        /* Every requested queue carries a priority; queueCount == 1 is the
         * overwhelmingly common case and Vulkan allows a single float there. */
        o->pQueuePriorities = &q->priority;
    }
    return (int)d->queue_request_count;
}

bool wubu_vkd3d_vkdev_create(const wubu_vkd3d_physdev_t *p,
                             const wubu_vkd3d_vkdev_desc_t *d,
                             VkDevice *out)
{
    if (!out) return false;
    *out = VK_NULL_HANDLE;
    if (wubu_vkd3d_vkdev_validate(p, d) != NULL) return false;

    VkDeviceQueueCreateInfo qinfos[VKD3D_MAX_QUEUE_REQUESTS];
    int nq = wubu_vkd3d_vkdev_queue_create_infos(p, d, qinfos,
                                                 VKD3D_MAX_QUEUE_REQUESTS);
    if (nq < 0) return false;

    VkDeviceCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = (uint32_t)nq;
    ci.pQueueCreateInfos = qinfos;
    ci.enabledExtensionCount = d->extension_count;
    ci.ppEnabledExtensionNames = d->extensions;
    ci.pEnabledFeatures = &d->features;

    return vkCreateDevice(p->device, &ci, NULL, out) == VK_SUCCESS;
}
