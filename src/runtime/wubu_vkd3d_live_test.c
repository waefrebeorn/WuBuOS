/*
 * wubu_vkd3d_live_test.c -- VKD3D slice 8 (live): real device enumeration.
 *
 * Every other VKD3D slice opens no VkDevice, which is why their correctness is
 * provable without a GPU.  This one is the exception: it creates a real
 * VkInstance, enumerates the real physical devices, and checks them against a
 * D3D12-class requirement set.
 *
 * It is a SEPARATE target (test_vkd3d_live), not part of the default gate: a
 * machine with no Vulkan loader or no GPU cannot run it, and the gate must stay
 * portable.  On this box it should find the RTX 5050.
 */
#include "wubu_vkd3d_physdev.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static const char *device_type_name(VkPhysicalDeviceType t)
{
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:          return "other";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
    default:                                     return "unknown";
    }
}

int main(void)
{
    printf("=== test_vkd3d_live (needs a real Vulkan loader) ===\n");

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wubu-vkd3d-live";
    app.applicationVersion = 1;
    app.apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);

    VkInstanceCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;

    VkInstance inst = VK_NULL_HANDLE;
    VkResult r = vkCreateInstance(&ci, NULL, &inst);
    if (r != VK_SUCCESS) {
        printf("  SKIP: vkCreateInstance failed (%d) -- no Vulkan loader?\n", (int)r);
        return 0;   /* not a failure: the box may simply have no loader */
    }

    /* -- a D3D12-class requirement set: what a D3D12 12_0 app needs -- */
    wubu_vkd3d_device_requirements_t req;
    memset(&req, 0, sizeof(req));
    req.min_api_version = VK_MAKE_API_VERSION(0, 1, 2, 0);
    req.required_queue_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    req.extension_count = 1;
    req.extensions[0] = "VK_KHR_swapchain";
    req.required_features.tessellationShader = VK_TRUE;
    req.required_features.geometryShader = VK_TRUE;
    req.required_features.samplerAnisotropy = VK_TRUE;

    wubu_vkd3d_physdev_t devs[8];
    int n = wubu_vkd3d_physdev_enumerate(inst, devs, 8);
    CHECK(n > 0, "at least one physical device is present");

    int capable = 0;
    for (int i = 0; i < n; i++) {
        wubu_vkd3d_physdev_t *p = &devs[i];
        const char *why = wubu_vkd3d_physdev_check(p, &req);
        printf("  [%d] %s (%s)\n", i, p->properties.deviceName,
               device_type_name(p->properties.deviceType));
        printf("      api %u.%u.%u, %u queue families, %u features\n",
               VK_VERSION_MAJOR(p->api_version), VK_VERSION_MINOR(p->api_version),
               VK_VERSION_PATCH(p->api_version), p->queue_family_count,
               (unsigned)(sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)));
        for (uint32_t q = 0; q < p->queue_family_count; q++) {
            VkFlags f = p->queue_families[q].queueFlags;
            printf("      queue %u: flags 0x%x%s%s\n", q, (unsigned)f,
                   (f & VK_QUEUE_GRAPHICS_BIT) ? " GRAPHICS" : "",
                   wubu_vkd3d_physdev_can_present(p, q) ? " (present-capable)" : "");
        }
        if (why) printf("      -> NOT D3D12-class: %s\n", why);
        else { printf("      -> D3D12-class: yes\n"); capable++; }
    }

    CHECK(capable > 0, "at least one device is D3D12-class");
    printf("  %d device(s), %d D3D12-class\n", n, capable);

    vkDestroyInstance(inst, NULL);

    if (failures == 0) printf("ALL VKD3D_LIVE TESTS PASSED\n");
    else printf("%d VKD3D_LIVE FAILURES\n", failures);
    return failures ? 1 : 0;
}
