/*
 * wubu_vkd3d_vkdev_test.c -- the logical VkDevice.
 *
 * The load-bearing rule: a family must not be requested TWICE, because two
 * DeviceQueueCreateInfos for one family is invalid Vulkan and the driver
 * answers with a generic failure that names no field.
 */
#include "wubu_vkd3d_vkdev.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void one_family_dev(wubu_vkd3d_physdev_t *p, uint32_t queues)
{
    memset(p, 0, sizeof(*p));
    p->api_version = VK_MAKE_API_VERSION(0, 1, 3, 0);
    p->queue_family_count = 1;
    p->queue_families[0].queueFlags = VK_QUEUE_GRAPHICS_BIT
                                    | VK_QUEUE_COMPUTE_BIT;
    p->queue_families[0].queueCount = queues;
    memset(&p->features, 0xFF, sizeof(p->features));
}

static void base_desc(wubu_vkd3d_vkdev_desc_t *d)
{
    memset(d, 0, sizeof(*d));
    d->queue_request_count = 1;
    d->queues[0].family_index = 0;
    d->queues[0].queue_count = 1;
    d->queues[0].priority = 1.0f;
    d->extension_count = 1;
    d->extensions[0] = "VK_KHR_swapchain";
}

int main(void)
{
    printf("=== test_vkd3d_vkdev ===\n");
    wubu_vkd3d_physdev_t p;
    wubu_vkd3d_vkdev_desc_t d;

    one_family_dev(&p, 4); base_desc(&d);
    CHECK(wubu_vkd3d_vkdev_validate(&p, &d) == NULL, "a normal desc is legal");

    /* -- the family must exist -- */
    one_family_dev(&p, 4); base_desc(&d); d.queues[0].family_index = 3;
    CHECK(strcmp(wubu_vkd3d_vkdev_validate(&p, &d),
                 "a queue request names a family index that does not exist") == 0,
          "an out-of-range family index is reported by name");

    /* -- no more queues than the family has -- */
    one_family_dev(&p, 2); base_desc(&d); d.queues[0].queue_count = 4;
    CHECK(strcmp(wubu_vkd3d_vkdev_validate(&p, &d),
                 "a queue request asks for more queues than the family has") == 0,
          "over-requesting queues is reported");

    /* -- a family must not be requested twice -- */
    one_family_dev(&p, 8); base_desc(&d);
    d.queue_request_count = 2;
    d.queues[1].family_index = 0;
    d.queues[1].queue_count = 1;
    d.queues[1].priority = 0.5f;
    CHECK(strcmp(wubu_vkd3d_vkdev_validate(&p, &d),
                 "the same queue family is requested twice") == 0,
          "requesting the same family twice is reported");

    /* -- two DISTINCT families is fine -- */
    one_family_dev(&p, 8); base_desc(&d);
    p.queue_family_count = 2;
    p.queue_families[1].queueFlags = VK_QUEUE_TRANSFER_BIT;
    p.queue_families[1].queueCount = 4;
    d.queue_request_count = 2;
    d.queues[1].family_index = 1;
    d.queues[1].queue_count = 2;
    d.queues[1].priority = 0.5f;
    CHECK(wubu_vkd3d_vkdev_validate(&p, &d) == NULL,
          "two distinct families are legal");

    /* -- zero queues / zero requests -- */
    one_family_dev(&p, 4); base_desc(&d); d.queues[0].queue_count = 0;
    CHECK(strcmp(wubu_vkd3d_vkdev_validate(&p, &d),
                 "a queue request asks for zero queues") == 0,
          "a zero-queue request is reported");
    one_family_dev(&p, 4); base_desc(&d); d.queue_request_count = 0;
    CHECK(strcmp(wubu_vkd3d_vkdev_validate(&p, &d),
                 "a device needs at least one queue") == 0,
          "a device with no queues is reported");

    /* -- priority range -- */
    one_family_dev(&p, 4); base_desc(&d); d.queues[0].priority = 1.5f;
    CHECK(strcmp(wubu_vkd3d_vkdev_validate(&p, &d),
                 "a queue priority is outside 0.0..1.0") == 0,
          "a priority above 1.0 is reported");
    one_family_dev(&p, 4); base_desc(&d); d.queues[0].priority = -0.1f;
    CHECK(wubu_vkd3d_vkdev_validate(&p, &d) != NULL,
          "a negative priority is reported");
    one_family_dev(&p, 4); base_desc(&d); d.queues[0].priority = 0.0f;
    CHECK(wubu_vkd3d_vkdev_validate(&p, &d) == NULL, "0.0 is a legal priority");

    /* -- extensions -- */
    one_family_dev(&p, 4); base_desc(&d); d.extensions[0] = NULL;
    CHECK(strcmp(wubu_vkd3d_vkdev_validate(&p, &d),
                 "a requested extension has no name") == 0,
          "an extension with no name is reported, not silently skipped");

    /* -- null handling -- */
    one_family_dev(&p, 4);
    CHECK(wubu_vkd3d_vkdev_validate(NULL, &d) != NULL, "a null device is rejected");
    base_desc(&d);
    CHECK(wubu_vkd3d_vkdev_validate(&p, NULL) != NULL, "a null desc is rejected");

    /* -- the Vulkan translation -- */
    {
        one_family_dev(&p, 8); base_desc(&d);
        d.queue_request_count = 2;
        d.queues[0].queue_count = 2;
        d.queues[0].priority = 1.0f;
        d.queues[1].family_index = 1;
        d.queues[1].queue_count = 1;
        d.queues[1].priority = 0.5f;
        p.queue_family_count = 2;
        p.queue_families[1].queueFlags = VK_QUEUE_TRANSFER_BIT;
        p.queue_families[1].queueCount = 4;

        VkDeviceQueueCreateInfo q[4];
        int n = wubu_vkd3d_vkdev_queue_create_infos(&p, &d, q, 4);
        CHECK(n == 2, "one create info per request");
        CHECK(q[0].queueFamilyIndex == 0 && q[0].queueCount == 2,
              "the graphics family info is carried through");
        CHECK(q[1].queueFamilyIndex == 1 && q[1].queueCount == 1,
              "the transfer family info is carried through");
        CHECK(q[0].pQueuePriorities && q[1].pQueuePriorities,
              "each info names its priorities");
        CHECK(wubu_vkd3d_vkdev_total_queues(&d) == 3,
              "the total queue count is the sum of the requests");

        /* an invalid desc does not translate */
        d.queues[0].family_index = 9;
        CHECK(wubu_vkd3d_vkdev_queue_create_infos(&p, &d, q, 4) == -1,
              "an invalid desc does not translate");
    }

    /* -- a device that cannot be created -- */
    {
        one_family_dev(&p, 4);
        wubu_vkd3d_vkdev_desc_t bad;
        base_desc(&bad);
        bad.queues[0].family_index = 42;
        CHECK(!wubu_vkd3d_vkdev_create(&p, &bad, NULL),
              "a null out pointer is rejected");
        VkDevice dev = VK_NULL_HANDLE;
        CHECK(!wubu_vkd3d_vkdev_create(&p, &bad, &dev),
              "an invalid desc does not reach vkCreateDevice");
        CHECK(dev == VK_NULL_HANDLE, "no device handle is produced");
    }

    if (failures == 0) printf("ALL VKD3D_VKDEV TESTS PASSED\n");
    else printf("%d VKD3D_VKDEV FAILURES\n", failures);
    return failures ? 1 : 0;
}
