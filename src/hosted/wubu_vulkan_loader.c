/*
 * wubu_vulkan_loader.c -- WuBuOS Vulkan: dynamic libvulkan loader, instance,
 * physical-device selection, and logical-device creation/destroy.
 * Extracted from the monolithic wubu_vulkan.c. Self-contained; depends only on
 * the public wubu_vulkan.h API. C11, no god headers.
 */

#include "wubu_vulkan.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>

/* -- Vulkan Loader ------------------------------------------------- */

static void *g_vulkan_lib = NULL;
static PFN_vkGetInstanceProcAddr g_vkGetInstanceProcAddr = NULL;
static PFN_vkGetDeviceProcAddr g_vkGetDeviceProcAddr = NULL;

#define VK_LOAD(inst, name) \
    (PFN_##name)vkGetInstanceProcAddr(inst, #name)

#define VK_LOAD_DEV(dev, name) \
    (PFN_##name)vkGetDeviceProcAddr(dev, #name)

static int load_vulkan_library(void) {
    if (g_vulkan_lib) return 0;
    g_vulkan_lib = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!g_vulkan_lib) g_vulkan_lib = dlopen("libvulkan.so", RTLD_LAZY | RTLD_LOCAL);
    if (!g_vulkan_lib) return -1;
    
    g_vkGetInstanceProcAddr = dlsym(g_vulkan_lib, "vkGetInstanceProcAddr");
    if (!g_vkGetInstanceProcAddr) {
        dlclose(g_vulkan_lib);
        g_vulkan_lib = NULL;
        return -1;
    }
    
    g_vkGetDeviceProcAddr = dlsym(g_vulkan_lib, "vkGetDeviceProcAddr");
    if (!g_vkGetDeviceProcAddr) {
        dlclose(g_vulkan_lib);
        g_vulkan_lib = NULL;
        return -1;
    }
    return 0;
}

/* -- Instance Implementation -------------------------------------- */

int wubu_vk_instance_create(WubuVkInstance *inst,
                            const char *app_name,
                            uint32_t app_version,
                            const char *engine_name,
                            uint32_t engine_version,
                            const char **layers, size_t layer_count,
                            const char **extensions, size_t ext_count) {
    if (load_vulkan_library() != 0) return -1;
    
    PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)g_vkGetInstanceProcAddr(NULL, "vkCreateInstance");
    if (!vkCreateInstance) return -1;

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = app_name ? app_name : "WuBuOS",
        .applicationVersion = app_version,
        .pEngineName = engine_name ? engine_name : "WuBuOS Engine",
        .engineVersion = engine_version,
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = (uint32_t)layer_count,
        .ppEnabledLayerNames = layers,
        .enabledExtensionCount = (uint32_t)ext_count,
        .ppEnabledExtensionNames = extensions,
    };

    VkResult r = vkCreateInstance(&create_info, NULL, &inst->instance);
    if (r != VK_SUCCESS) return -1;

    /* Load instance functions */
    inst->api_version = VK_API_VERSION_1_3;
    inst->enabled_layers = layers;
    inst->layer_count = layer_count;
    inst->enabled_extensions = extensions;
    inst->extension_count = ext_count;

    return 0;
}

void wubu_vk_instance_destroy(WubuVkInstance *inst) {
    if (!inst || !inst->instance) return;
    
    PFN_vkDestroyInstance vkDestroyInstance = (PFN_vkDestroyInstance)g_vkGetInstanceProcAddr(inst->instance, "vkDestroyInstance");
    if (vkDestroyInstance) vkDestroyInstance(inst->instance, NULL);
    inst->instance = VK_NULL_HANDLE;
}

/* -- Debug Callback ----------------------------------------------- */

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *data,
    void *user_data) {
    (void)user_data;
    const char *sev = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" :
                      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN" :
                      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "INFO" : "DEBUG";
    fprintf(stderr, "[Vulkan %s] %s\n", sev, data->pMessage);
    return VK_FALSE;
}

VkBool32 wubu_vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                VkDebugUtilsMessageTypeFlagsEXT type,
                                const VkDebugUtilsMessengerCallbackDataEXT *data,
                                void *user_data) {
    return debug_callback(severity, type, data, user_data);
}

/* -- Physical Device ---------------------------------------------- */

int wubu_vk_physical_device_pick(WubuVkPhysicalDevice *phys, WubuVkInstance *inst) {
    uint32_t count = 0;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = 
        (PFN_vkEnumeratePhysicalDevices)vkGetInstanceProcAddr(inst->instance, "vkEnumeratePhysicalDevices");
    
    VkResult r = vkEnumeratePhysicalDevices(inst->instance, &count, NULL);
    if (r != VK_SUCCESS || count == 0) return -1;

    VkPhysicalDevice *devices = calloc(count, sizeof(VkPhysicalDevice));
    r = vkEnumeratePhysicalDevices(inst->instance, &count, devices);
    if (r != VK_SUCCESS) { free(devices); return -1; }

    /* Pick first discrete GPU, fallback to first */
    int picked = -1;
    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceProperties props;
        PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = 
            (PFN_vkGetPhysicalDeviceProperties)vkGetInstanceProcAddr(inst->instance, "vkGetPhysicalDeviceProperties");
        vkGetPhysicalDeviceProperties(devices[i], &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            picked = i;
            break;
        }
        if (picked == -1) picked = 0;
    }

    if (picked == -1) { free(devices); return -1; }

    phys->physical_device = devices[picked];
    phys->instance = inst->instance;  /* Store for later vkGetInstanceProcAddr calls */
    free(devices);

    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = 
        (PFN_vkGetPhysicalDeviceProperties)vkGetInstanceProcAddr(inst->instance, "vkGetPhysicalDeviceProperties");
    
    PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures = 
        (PFN_vkGetPhysicalDeviceFeatures)vkGetInstanceProcAddr(inst->instance, "vkGetPhysicalDeviceFeatures");
    
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = 
        (PFN_vkGetPhysicalDeviceMemoryProperties)vkGetInstanceProcAddr(inst->instance, "vkGetPhysicalDeviceMemoryProperties");

    vkGetPhysicalDeviceProperties(phys->physical_device, &phys->properties);
    vkGetPhysicalDeviceFeatures(phys->physical_device, &phys->features);
    vkGetPhysicalDeviceMemoryProperties(phys->physical_device, &phys->mem_properties);

    /* Find queue families */
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetQueueFamilyProps = 
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties)vkGetInstanceProcAddr(inst->instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    
    uint32_t queue_count = 0;
    vkGetQueueFamilyProps(phys->physical_device, &queue_count, NULL);
    VkQueueFamilyProperties *queue_props = calloc(queue_count, sizeof(VkQueueFamilyProperties));
    vkGetQueueFamilyProps(phys->physical_device, &queue_count, queue_props);

    phys->graphics_queue_family = UINT32_MAX;
    phys->compute_queue_family = UINT32_MAX;
    phys->transfer_queue_family = UINT32_MAX;

    for (uint32_t i = 0; i < queue_count; i++) {
        if (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && phys->graphics_queue_family == UINT32_MAX)
            phys->graphics_queue_family = i;
        if (queue_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT && phys->compute_queue_family == UINT32_MAX)
            phys->compute_queue_family = i;
        if ((queue_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && 
            !(queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && 
            phys->transfer_queue_family == UINT32_MAX)
            phys->transfer_queue_family = i;
    }

    free(queue_props);
    return 0;
}

int wubu_vk_physical_device_init_surface(WubuVkPhysicalDevice *phys,
                                         VkInstance instance,
                                         VkSurfaceKHR surface) {
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetSurfaceCaps = 
        (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetSurfaceFormats = 
        (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPresentModes = 
        (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");

    VkResult r = vkGetSurfaceCaps(phys->physical_device, surface, &phys->surface_caps);
    if (r != VK_SUCCESS) return -1;

    uint32_t format_count = 0;
    r = vkGetSurfaceFormats(phys->physical_device, surface, &format_count, NULL);
    if (r != VK_SUCCESS || format_count == 0) return -1;
    phys->surface_formats = calloc(format_count, sizeof(VkSurfaceFormatKHR));
    phys->surface_format_count = format_count;
    r = vkGetSurfaceFormats(phys->physical_device, surface, &format_count, phys->surface_formats);
    if (r != VK_SUCCESS) { free(phys->surface_formats); return -1; }

    uint32_t mode_count = 0;
    r = vkGetPresentModes(phys->physical_device, surface, &mode_count, NULL);
    if (r != VK_SUCCESS || mode_count == 0) return -1;
    phys->present_modes = calloc(mode_count, sizeof(VkPresentModeKHR));
    phys->present_mode_count = mode_count;
    r = vkGetPresentModes(phys->physical_device, surface, &mode_count, phys->present_modes);
    if (r != VK_SUCCESS) { free(phys->surface_formats); free(phys->present_modes); return -1; }

    return 0;
}

/* C-compatible helper: add unique queue family */
static int add_family(uint32_t *families, uint32_t *count, uint32_t fam) {
    for (uint32_t i = 0; i < *count; i++)
        if (families[i] == fam) return 0;
    families[(*count)++] = fam;
    return 1;
}

/* -- Logical Device ----------------------------------------------- */

int wubu_vk_device_create(WubuVkDevice *dev, WubuVkPhysicalDevice *phys,
                          VkSurfaceKHR surface,
                          const char **extensions, size_t ext_count) {
    PFN_vkCreateDevice vkCreateDevice = 
        (PFN_vkCreateDevice)vkGetInstanceProcAddr(phys->instance, "vkCreateDevice");
    
    float queue_priority = 1.0f;
    uint32_t unique_families[4];
    uint32_t family_count = 0;

    add_family(unique_families, &family_count, phys->graphics_queue_family);
    if (phys->compute_queue_family != phys->graphics_queue_family)
        add_family(unique_families, &family_count, phys->compute_queue_family);
    if (phys->transfer_queue_family != UINT32_MAX && 
        phys->transfer_queue_family != phys->graphics_queue_family &&
        phys->transfer_queue_family != phys->compute_queue_family)
        add_family(unique_families, &family_count, phys->transfer_queue_family);

    VkDeviceQueueCreateInfo *queue_infos = calloc(family_count, sizeof(VkDeviceQueueCreateInfo));
    for (uint32_t i = 0; i < family_count; i++) {
        queue_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_infos[i].queueFamilyIndex = unique_families[i];
        queue_infos[i].queueCount = 1;
        queue_infos[i].pQueuePriorities = (float[]){1.0f};
    }

    VkPhysicalDeviceFeatures enabled_features = {0};
    enabled_features.samplerAnisotropy = VK_TRUE;
    enabled_features.fillModeNonSolid = VK_TRUE;
    enabled_features.wideLines = VK_TRUE;

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = family_count,
        .pQueueCreateInfos = queue_infos,
        .enabledExtensionCount = (uint32_t)ext_count,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = &enabled_features,
    };
    
    VkResult r = vkCreateDevice(phys->physical_device, &create_info, NULL, &dev->device);
    free(queue_infos);
    if (r != VK_SUCCESS) return -1;

    dev->phys_dev = phys;

    PFN_vkGetDeviceQueue vkGetDeviceQueue = 
        (PFN_vkGetDeviceQueue)vkGetDeviceProcAddr(dev->device, "vkGetDeviceQueue");

    vkGetDeviceQueue(dev->device, phys->graphics_queue_family, 0, &dev->graphics_queue);
    if (phys->compute_queue_family != phys->graphics_queue_family)
        vkGetDeviceQueue(dev->device, phys->compute_queue_family, 0, &dev->compute_queue);
    else
        dev->compute_queue = dev->graphics_queue;
    
    if (phys->transfer_queue_family != UINT32_MAX && 
        phys->transfer_queue_family != phys->graphics_queue_family &&
        phys->transfer_queue_family != phys->compute_queue_family)
        vkGetDeviceQueue(dev->device, phys->transfer_queue_family, 0, &dev->transfer_queue);
    else
        dev->transfer_queue = dev->graphics_queue;

    dev->graphics_queue_family = phys->graphics_queue_family;
    dev->compute_queue_family = phys->compute_queue_family;
    dev->transfer_queue_family = phys->transfer_queue_family;

    return 0;
}

void wubu_vk_device_destroy(WubuVkDevice *dev) {
    if (dev->device) {
        PFN_vkDestroyDevice vkDestroyDevice = 
            (PFN_vkDestroyDevice)vkGetDeviceProcAddr(dev->device, "vkDestroyDevice");
        vkDestroyDevice(dev->device, NULL);
        dev->device = VK_NULL_HANDLE;
    }
}
