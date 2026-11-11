/*
 * wubu_vkd3d_physdev_test.c -- physical device selection.
 *
 * The load-bearing assertion is that every required feature bit must be SET:
 * a device advertising a capability is not the same as one supporting it, and
 * silently accepting the former is how a missing-feature crash happens later.
 */
#include "wubu_vkd3d_physdev.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void good_dev(wubu_vkd3d_physdev_t *p)
{
    memset(p, 0, sizeof(*p));
    p->api_version = VK_MAKE_API_VERSION(0, 1, 3, 0);
    p->properties.apiVersion = p->api_version;
    p->queue_family_count = 1;
    p->queue_families[0].queueFlags = VK_QUEUE_GRAPHICS_BIT
                                    | VK_QUEUE_COMPUTE_BIT
                                    | VK_QUEUE_TRANSFER_BIT;
    /* every feature set */
    memset(&p->features, 0xFF, sizeof(p->features));
}

static void base_req(wubu_vkd3d_device_requirements_t *r)
{
    memset(r, 0, sizeof(*r));
    r->min_api_version = VK_MAKE_API_VERSION(0, 1, 2, 0);
    r->required_queue_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    r->extension_count = 1;
    r->extensions[0] = "VK_KHR_swapchain";
    r->required_features.robustBufferAccess = VK_TRUE;
    r->required_features.tessellationShader = VK_TRUE;
}

int main(void)
{
    printf("=== test_vkd3d_physdev ===\n");
    wubu_vkd3d_physdev_t p;
    wubu_vkd3d_device_requirements_t r;

    good_dev(&p); base_req(&r);
    CHECK(wubu_vkd3d_physdev_check(&p, &r) == NULL, "a capable device passes");

    /* -- api version -- */
    good_dev(&p); p.api_version = VK_MAKE_API_VERSION(0, 1, 1, 0);
    CHECK(strcmp(wubu_vkd3d_physdev_check(&p, &r),
                 "the device's Vulkan version is below what the app requires") == 0,
          "an old API version is reported by version, not by a bare 'no'");

    /* -- queue families -- */
    good_dev(&p); p.queue_families[0].queueFlags = VK_QUEUE_TRANSFER_BIT;
    CHECK(strcmp(wubu_vkd3d_physdev_check(&p, &r),
                 "a required queue family is missing") == 0,
          "a missing queue family is reported");

    /* -- features: every required bit must be SET -- */
    good_dev(&p);
    p.features.tessellationShader = VK_FALSE;      /* required, not set */
    CHECK(strcmp(wubu_vkd3d_physdev_check(&p, &r),
                 "a required device feature is not supported") == 0,
          "a required feature bit that is not set fails");
    good_dev(&p);
    p.features.robustBufferAccess = VK_FALSE;
    CHECK(wubu_vkd3d_physdev_check(&p, &r) != NULL,
          "one unset required bit is enough to fail");
    good_dev(&p);
    p.features.geometryShader = VK_FALSE;          /* NOT required */
    CHECK(wubu_vkd3d_physdev_check(&p, &r) == NULL,
          "an unset bit that was NOT required does not matter");

    /* -- a device with no features at all -- */
    good_dev(&p); memset(&p.features, 0, sizeof(p.features));
    CHECK(wubu_vkd3d_physdev_check(&p, &r) != NULL,
          "a featureless device fails a feature requirement");

    /* -- empty requirements -- */
    good_dev(&p);
    memset(&r, 0, sizeof(r));
    CHECK(wubu_vkd3d_physdev_check(&p, &r) == NULL, "no requirements is satisfied");

    /* -- present capability -- */
    good_dev(&p);
    CHECK(wubu_vkd3d_physdev_can_present(&p, 0), "a graphics family can present");
    good_dev(&p); p.queue_families[0].queueFlags = VK_QUEUE_TRANSFER_BIT;
    CHECK(!wubu_vkd3d_physdev_can_present(&p, 0),
          "a transfer-only family CANNOT present");
    good_dev(&p); p.queue_families[0].queueFlags = VK_QUEUE_COMPUTE_BIT;
    CHECK(wubu_vkd3d_physdev_can_present(&p, 0), "a compute family can present");
    CHECK(!wubu_vkd3d_physdev_can_present(&p, 99), "an out-of-range family cannot present");
    CHECK(!wubu_vkd3d_physdev_can_present(NULL, 0), "a null device cannot present");

    /* -- null handling -- */
    CHECK(wubu_vkd3d_physdev_check(NULL, &r) != NULL, "a null device is rejected");
    good_dev(&p);
    CHECK(wubu_vkd3d_physdev_check(&p, NULL) != NULL, "null requirements are rejected");
    base_req(&r); r.extensions[0] = NULL;
    CHECK(wubu_vkd3d_physdev_check(&p, &r) != NULL,
          "a required extension with no name is reported, not skipped");

    /* -- api version helper -- */
    CHECK(wubu_vkd3d_physdev_api_version(1, 3, 0) == VK_MAKE_API_VERSION(0, 1, 3, 0),
          "the version helper matches the macro");
    CHECK(VK_VERSION_MAJOR(wubu_vkd3d_physdev_api_version(1, 3, 255)) == 1 &&
          VK_VERSION_MINOR(wubu_vkd3d_physdev_api_version(1, 3, 255)) == 3,
          "major/minor survive the packing");

    if (failures == 0) printf("ALL VKD3D_PHYSDEV TESTS PASSED\n");
    else printf("%d VKD3D_PHYSDEV FAILURES\n", failures);
    return failures ? 1 : 0;
}
