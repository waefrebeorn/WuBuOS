/*
 * wubu_vulkan.c  --  WuBuOS Vulkan Implementation
 *
 * Cell 410: Minimal Vulkan for Proton/DXVK.
 * Dynamic libvulkan loading for SteamOS compatibility.
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

/* -- Swapchain Implementation ------------------------------------- */

int wubu_vk_swapchain_create(WubuVkSwapchain *sc, WubuVkDevice *dev,
                             VkSurfaceKHR surface,
                             uint32_t width, uint32_t height,
                             VkPresentModeKHR present_mode) {
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetSurfaceCaps = 
        (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(dev->phys_dev->instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetSurfaceFormats = 
        (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(dev->phys_dev->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");

    VkSurfaceCapabilitiesKHR caps;
    VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        (VkPhysicalDevice)dev->phys_dev->physical_device, surface, &caps);
    if (r != VK_SUCCESS) return -1;

    VkExtent2D extent = { width, height };
    if (extent.width < caps.minImageExtent.width) extent.width = caps.minImageExtent.width;
    if (extent.width > caps.maxImageExtent.width) extent.width = caps.maxImageExtent.width;
    if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
    if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;

    VkSurfaceFormatKHR desired_format = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        (VkPhysicalDevice)dev->phys_dev->physical_device, surface, &format_count, NULL);
    
    VkSurfaceFormatKHR chosen_format = desired_format;
    VkSurfaceFormatKHR *formats = calloc(format_count, sizeof(VkSurfaceFormatKHR));
    if (format_count > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            (VkPhysicalDevice)dev->phys_dev->physical_device, surface, &format_count, formats);
        for (uint32_t i = 0; i < format_count; i++) {
            if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && 
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen_format = formats[i];
                break;
            }
        }
    }
    free(formats);

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        (VkPhysicalDevice)dev->phys_dev->physical_device, surface, &present_mode_count, NULL);
    
    VkPresentModeKHR *present_modes = calloc(present_mode_count, sizeof(VkPresentModeKHR));
    if (present_mode_count > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            (VkPhysicalDevice)dev->phys_dev->physical_device, surface, &present_mode_count, present_modes);
        
        VkPresentModeKHR chosen_mode = present_mode;
        if (present_mode == VK_PRESENT_MODE_MAX_ENUM_KHR) {
            chosen_mode = VK_PRESENT_MODE_FIFO_KHR;  /* Default to VSYNC */
            for (uint32_t i = 0; i < present_mode_count; i++) {
                if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                    chosen_mode = VK_PRESENT_MODE_MAILBOX_KHR;  /* Triple buffering */
                    break;
                }
            }
        }
        present_mode = chosen_mode;
    }
    free(present_modes);

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = chosen_format.format,
        .imageColorSpace = chosen_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = 
        (PFN_vkCreateSwapchainKHR)vkGetDeviceProcAddr(dev->device, "vkCreateSwapchainKHR");
    
    r = vkCreateSwapchainKHR(dev->device, &create_info, NULL, &sc->swapchain);
    if (r != VK_SUCCESS) return -1;

    sc->device = dev;
    sc->surface = surface;
    sc->extent = extent;
    sc->format = chosen_format.format;
    sc->color_space = chosen_format.colorSpace;

    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImages = 
        (PFN_vkGetSwapchainImagesKHR)vkGetDeviceProcAddr(dev->device, "vkGetSwapchainImagesKHR");
    
    r = vkGetSwapchainImages(dev->device, sc->swapchain, &sc->image_count, NULL);

    sc->images = calloc(sc->image_count, sizeof(VkImage));
    r = vkGetSwapchainImages(dev->device, sc->swapchain, &sc->image_count, sc->images);
    if (r != VK_SUCCESS) return -1;

    sc->image_views = calloc(sc->image_count, sizeof(VkImageView));
    for (uint32_t i = 0; i < sc->image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = sc->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = sc->format,
            .components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        PFN_vkCreateImageView vkCreateImageView = 
            (PFN_vkCreateImageView)vkGetDeviceProcAddr(dev->device, "vkCreateImageView");
        VkResult rv = vkCreateImageView(dev->device, &view_info, NULL, &sc->image_views[i]);
        if (rv != VK_SUCCESS) return -1;
    }

    PFN_vkCreateSemaphore vkCreateSemaphore = 
        (PFN_vkCreateSemaphore)vkGetDeviceProcAddr(dev->device, "vkCreateSemaphore");
    sc->image_acquired = calloc(sc->image_count, sizeof(VkSemaphore));
    sc->render_finished = calloc(sc->image_count, sizeof(VkSemaphore));
    for (uint32_t i = 0; i < sc->image_count; i++) {
        VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        vkCreateSemaphore(dev->device, &sem_info, NULL, &sc->image_acquired[i]);
        vkCreateSemaphore(dev->device, &sem_info, NULL, &sc->render_finished[i]);
    }

    sc->current_image = 0;
    return 0;
}

void wubu_vk_swapchain_destroy(WubuVkSwapchain *sc) {
    if (!sc->device || !sc->device->device) return;
    
    PFN_vkDestroySwapchainKHR vkDestroySwapchain = 
        (PFN_vkDestroySwapchainKHR)vkGetInstanceProcAddr(sc->device->device, "vkDestroySwapchainKHR");
    PFN_vkDestroyImageView vkDestroyImageView = 
        (PFN_vkDestroyImageView)vkGetInstanceProcAddr(sc->device->device, "vkDestroyImageView");
    PFN_vkDestroySemaphore vkDestroySemaphore = 
        (PFN_vkDestroySemaphore)vkGetInstanceProcAddr(sc->device->device, "vkDestroySemaphore");

    if (sc->framebuffers) free(sc->framebuffers);
    if (sc->image_views) {
        for (uint32_t i = 0; i < sc->image_count; i++)
            vkDestroyImageView(sc->device->device, sc->image_views[i], NULL);
        free(sc->image_views);
    }
    if (sc->image_acquired) {
        for (uint32_t i = 0; i < sc->image_count; i++)
            vkDestroySemaphore(sc->device->device, sc->image_acquired[i], NULL);
        free(sc->image_acquired);
    }
    if (sc->render_finished) {
        for (uint32_t i = 0; i < sc->image_count; i++)
            vkDestroySemaphore(sc->device->device, sc->render_finished[i], NULL);
        free(sc->render_finished);
    }
    if (sc->images) free(sc->images);
    if (sc->swapchain) vkDestroySwapchain(sc->device->device, sc->swapchain, NULL);
}

int wubu_vk_swapchain_acquire(WubuVkSwapchain *sc, uint64_t timeout_ns) {
    PFN_vkAcquireNextImageKHR vkAcquireNextImage = 
        (PFN_vkAcquireNextImageKHR)vkGetDeviceProcAddr(sc->device->device, "vkAcquireNextImageKHR");
    
    VkResult r = vkAcquireNextImage(sc->device->device, sc->swapchain, timeout_ns,
                                    sc->image_acquired[sc->current_image],
                                    VK_NULL_HANDLE, &sc->current_image);
    return (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR) ? 0 : -1;
}

int wubu_vk_swapchain_present(WubuVkSwapchain *sc) {
    PFN_vkQueuePresentKHR vkQueuePresent = 
        (PFN_vkQueuePresentKHR)vkGetDeviceProcAddr(sc->device->device, "vkQueuePresentKHR");
    
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sc->render_finished[sc->current_image],
        .swapchainCount = 1,
        .pSwapchains = &sc->swapchain,
        .pImageIndices = &sc->current_image,
    };
    
    VkResult r = vkQueuePresent(sc->device->present_queue, &present_info);
    return (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR) ? 0 : -1;
}

/* -- Command Pool ------------------------------------------------- */

int wubu_vk_cmd_pool_create(WubuVkCmdPool *pool, WubuVkDevice *dev,
                            uint32_t queue_family, uint32_t count) {
    pool->device = dev;
    PFN_vkCreateCommandPool vkCreateCommandPool = 
        (PFN_vkCreateCommandPool)vkGetDeviceProcAddr(dev->device, "vkCreateCommandPool");
    
    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family,
    };
    
    VkResult r = vkCreateCommandPool(dev->device, &create_info, NULL, &pool->pool);
    if (r != VK_SUCCESS) return -1;
    
    pool->buffer_count = count;
    pool->buffers = calloc(count, sizeof(VkCommandBuffer));
    pool->fences = calloc(count, sizeof(VkFence));

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = count,
    };
    
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = 
        (PFN_vkAllocateCommandBuffers)vkGetDeviceProcAddr(dev->device, "vkAllocateCommandBuffers");
    r = vkAllocateCommandBuffers(dev->device, &alloc_info, pool->buffers);
    if (r != VK_SUCCESS) return -1;
    
    PFN_vkCreateFence vkCreateFence = 
        (PFN_vkCreateFence)vkGetDeviceProcAddr(dev->device, "vkCreateFence");
    VkFenceCreateInfo fence_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    for (uint32_t i = 0; i < count; i++) {
        vkCreateFence(dev->device, &fence_info, NULL, &pool->fences[i]);
    }
    
    return 0;
}

void wubu_vk_cmd_pool_destroy(WubuVkCmdPool *pool) {
    if (!pool->device || !pool->device->device) return;
    
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers = 
        (PFN_vkFreeCommandBuffers)vkGetDeviceProcAddr(pool->device->device, "vkFreeCommandBuffers");
    PFN_vkDestroyFence vkDestroyFence = 
        (PFN_vkDestroyFence)vkGetDeviceProcAddr(pool->device->device, "vkDestroyFence");
    PFN_vkDestroyCommandPool vkDestroyCommandPool = 
        (PFN_vkDestroyCommandPool)vkGetDeviceProcAddr(pool->device->device, "vkDestroyCommandPool");

    if (pool->fences) {
        for (uint32_t i = 0; i < pool->buffer_count; i++)
            vkDestroyFence(pool->device->device, pool->fences[i], NULL);
        free(pool->fences);
    }
    if (pool->buffers) {
        vkFreeCommandBuffers(pool->device->device, pool->pool, pool->buffer_count, pool->buffers);
        free(pool->buffers);
    }
    if (pool->pool)
        vkDestroyCommandPool(pool->device->device, pool->pool, NULL);
}

VkCommandBuffer wubu_vk_cmd_begin(WubuVkCmdPool *pool, uint32_t index) {
    PFN_vkResetCommandBuffer vkResetCommandBuffer = 
        (PFN_vkResetCommandBuffer)vkGetInstanceProcAddr(pool->device->phys_dev->instance, "vkResetCommandBuffer");
    PFN_vkResetFences vkResetFences = 
        (PFN_vkResetFences)vkGetInstanceProcAddr(pool->device->phys_dev->instance, "vkResetFences");
    
    vkResetCommandBuffer(pool->buffers[index], 0);
    vkResetFences(pool->device->device, 1, &pool->fences[index]);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = 
        (PFN_vkBeginCommandBuffer)vkGetDeviceProcAddr(pool->device->device, "vkBeginCommandBuffer");
    vkBeginCommandBuffer(pool->buffers[index], &begin_info);
    
    return pool->buffers[index];
}

void wubu_vk_cmd_end(WubuVkCmdPool *pool, VkCommandBuffer cmd) {
    PFN_vkEndCommandBuffer vkEndCommandBuffer = 
        (PFN_vkEndCommandBuffer)vkGetInstanceProcAddr(pool->device->phys_dev->instance, "vkEndCommandBuffer");
    vkEndCommandBuffer(cmd);
}

void wubu_vk_cmd_submit(WubuVkDevice *dev, VkQueue queue,
                        VkCommandBuffer *cmds, uint32_t count,
                        VkSemaphore *wait_semaphores, uint32_t wait_count,
                        VkSemaphore *signal_semaphores, uint32_t signal_count,
                        VkFence fence) {
    PFN_vkQueueSubmit vkQueueSubmit = 
        (PFN_vkQueueSubmit)vkGetInstanceProcAddr(dev->phys_dev->instance, "vkQueueSubmit");
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = wait_count,
        .pWaitSemaphores = wait_semaphores,
        .commandBufferCount = count,
        .pCommandBuffers = cmds,
        .signalSemaphoreCount = signal_count,
        .pSignalSemaphores = signal_semaphores,
    };
    vkQueueSubmit(queue, 1, &submit_info, fence);
}

/* -- Utility ------------------------------------------------------ */

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

/* ===================================================================
 * COMPUTE PIPELINE IMPLEMENTATION
 * =================================================================== */

int wubu_vk_shader_module_create(WubuVkShaderModule *shader,
                                 WubuVkDevice *dev,
                                 const uint32_t *spirv, size_t spirv_size) {
    if (!shader || !dev || !spirv || spirv_size == 0) return -1;

    shader->device = dev->device;

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv_size,
        .pCode = spirv,
    };

    PFN_vkCreateShaderModule vkCreateShaderModule = 
        (PFN_vkCreateShaderModule)vkGetDeviceProcAddr(dev->device, "vkCreateShaderModule");
    if (!vkCreateShaderModule) return -1;

    VkResult r = vkCreateShaderModule(dev->device, &create_info, NULL, &shader->module);
    return (r == VK_SUCCESS) ? 0 : -1;
}

void wubu_vk_shader_module_destroy(WubuVkShaderModule *shader) {
    if (!shader || shader->device == VK_NULL_HANDLE || !shader->module) return;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = 
        (PFN_vkDestroyShaderModule)vkGetDeviceProcAddr(shader->device, "vkDestroyShaderModule");
    if (vkDestroyShaderModule) {
        vkDestroyShaderModule(shader->device, shader->module, NULL);
    }
    shader->module = VK_NULL_HANDLE;
}

int wubu_vk_compute_pipeline_create(WubuVkComputePipeline *pipe,
                                    WubuVkDevice *dev,
                                    WubuVkShaderModule *shader,
                                    const char *entry_point,
                                    VkDescriptorSetLayoutBinding *bindings,
                                    uint32_t binding_count,
                                    VkPushConstantRange *push_constants,
                                    uint32_t push_constant_count) {
    if (!pipe || !dev || !shader || !entry_point) return -1;

    pipe->device = dev;

    /* Create descriptor set layout */
    VkDescriptorSetLayoutCreateInfo dsl_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = binding_count,
        .pBindings = bindings,
    };

    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = 
        (PFN_vkCreateDescriptorSetLayout)vkGetDeviceProcAddr(dev->device, "vkCreateDescriptorSetLayout");
    if (!vkCreateDescriptorSetLayout) return -1;

    VkResult r = vkCreateDescriptorSetLayout(dev->device, &dsl_create_info, NULL, &pipe->desc_set_layout);
    if (r != VK_SUCCESS) return -1;

    /* Create pipeline layout */
    VkPipelineLayoutCreateInfo pl_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &pipe->desc_set_layout,
        .pushConstantRangeCount = push_constant_count,
        .pPushConstantRanges = push_constants,
    };

    PFN_vkCreatePipelineLayout vkCreatePipelineLayout = 
        (PFN_vkCreatePipelineLayout)vkGetDeviceProcAddr(dev->device, "vkCreatePipelineLayout");
    if (!vkCreatePipelineLayout) {
        vkDestroyDescriptorSetLayout(dev->device, pipe->desc_set_layout, NULL);
        return -1;
    }

    r = vkCreatePipelineLayout(dev->device, &pl_create_info, NULL, &pipe->layout);
    if (r != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(dev->device, pipe->desc_set_layout, NULL);
        return -1;
    }

    /* Create compute pipeline */
    VkPipelineShaderStageCreateInfo stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader->module,
        .pName = entry_point,
    };

    VkComputePipelineCreateInfo pipe_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage_info,
        .layout = pipe->layout,
    };

    PFN_vkCreateComputePipelines vkCreateComputePipelines = 
        (PFN_vkCreateComputePipelines)vkGetDeviceProcAddr(dev->device, "vkCreateComputePipelines");
    if (!vkCreateComputePipelines) {
        vkDestroyPipelineLayout(dev->device, pipe->layout, NULL);
        vkDestroyDescriptorSetLayout(dev->device, pipe->desc_set_layout, NULL);
        return -1;
    }

    r = vkCreateComputePipelines(dev->device, VK_NULL_HANDLE, 1, &pipe_create_info, NULL, &pipe->pipeline);
    if (r != VK_SUCCESS) {
        vkDestroyPipelineLayout(dev->device, pipe->layout, NULL);
        vkDestroyDescriptorSetLayout(dev->device, pipe->desc_set_layout, NULL);
        return -1;
    }

    return 0;
}

void wubu_vk_compute_pipeline_destroy(WubuVkComputePipeline *pipe) {
    if (!pipe || !pipe->device) return;
    PFN_vkDestroyPipeline vkDestroyPipeline = 
        (PFN_vkDestroyPipeline)vkGetDeviceProcAddr(pipe->device->device, "vkDestroyPipeline");
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = 
        (PFN_vkDestroyPipelineLayout)vkGetDeviceProcAddr(pipe->device->device, "vkDestroyPipelineLayout");
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = 
        (PFN_vkDestroyDescriptorSetLayout)vkGetDeviceProcAddr(pipe->device->device, "vkDestroyDescriptorSetLayout");
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = 
        (PFN_vkDestroyDescriptorPool)vkGetDeviceProcAddr(pipe->device->device, "vkDestroyDescriptorPool");

    if (pipe->pipeline && vkDestroyPipeline) {
        vkDestroyPipeline(pipe->device->device, pipe->pipeline, NULL);
    }
    if (pipe->layout && vkDestroyPipelineLayout) {
        vkDestroyPipelineLayout(pipe->device->device, pipe->layout, NULL);
    }
    if (pipe->desc_set_layout && vkDestroyDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(pipe->device->device, pipe->desc_set_layout, NULL);
    }
    if (pipe->desc_pool && vkDestroyDescriptorPool) {
        vkDestroyDescriptorPool(pipe->device->device, pipe->desc_pool, NULL);
    }
    memset(pipe, 0, sizeof(WubuVkComputePipeline));
}

int wubu_vk_compute_descriptor_sets_alloc(WubuVkComputePipeline *pipe,
                                          VkDescriptorSetLayoutBinding *bindings,
                                          uint32_t binding_count) {
    if (!pipe || !pipe->device) return -1;

    /* Create descriptor pool */
    VkDescriptorPoolSize pool_sizes[16];
    uint32_t pool_size_count = 0;

    for (uint32_t i = 0; i < binding_count && pool_size_count < 16; i++) {
        pool_sizes[pool_size_count].type = bindings[i].descriptorType;
        pool_sizes[pool_size_count].descriptorCount = bindings[i].descriptorCount;
        pool_size_count++;
    }

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = pool_size_count,
        .pPoolSizes = pool_sizes,
    };

    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = 
        (PFN_vkCreateDescriptorPool)vkGetDeviceProcAddr(pipe->device->device, "vkCreateDescriptorPool");
    if (!vkCreateDescriptorPool) return -1;

    VkResult r = vkCreateDescriptorPool(pipe->device->device, &pool_info, NULL, &pipe->desc_pool);
    if (r != VK_SUCCESS) return -1;

    /* Allocate descriptor set */
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pipe->desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &pipe->desc_set_layout,
    };

    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = 
        (PFN_vkAllocateDescriptorSets)vkGetDeviceProcAddr(pipe->device->device, "vkAllocateDescriptorSets");
    if (!vkAllocateDescriptorSets) {
        vkDestroyDescriptorPool(pipe->device->device, pipe->desc_pool, NULL);
        pipe->desc_pool = VK_NULL_HANDLE;
        return -1;
    }

    r = vkAllocateDescriptorSets(pipe->device->device, &alloc_info, &pipe->desc_set);
    if (r != VK_SUCCESS) {
        vkDestroyDescriptorPool(pipe->device->device, pipe->desc_pool, NULL);
        pipe->desc_pool = VK_NULL_HANDLE;
        return -1;
    }

    return 0;
}

void wubu_vk_compute_descriptor_sets_free(WubuVkComputePipeline *pipe) {
    if (!pipe || !pipe->device || !pipe->desc_pool) return;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets = 
        (PFN_vkFreeDescriptorSets)vkGetDeviceProcAddr(pipe->device->device, "vkFreeDescriptorSets");
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = 
        (PFN_vkDestroyDescriptorPool)vkGetDeviceProcAddr(pipe->device->device, "vkDestroyDescriptorPool");

    if (pipe->desc_set && vkFreeDescriptorSets) {
        vkFreeDescriptorSets(pipe->device->device, pipe->desc_pool, 1, &pipe->desc_set);
    }
    if (vkDestroyDescriptorPool) {
        vkDestroyDescriptorPool(pipe->device->device, pipe->desc_pool, NULL);
    }
    pipe->desc_set = VK_NULL_HANDLE;
    pipe->desc_pool = VK_NULL_HANDLE;
}

int wubu_vk_compute_descriptor_write_buffer(WubuVkComputePipeline *pipe,
                                            uint32_t binding,
                                            VkBuffer buffer,
                                            VkDeviceSize offset,
                                            VkDeviceSize range) {
    if (!pipe || !pipe->device || !pipe->desc_set || !buffer) return -1;

    VkDescriptorBufferInfo buffer_info = {
        .buffer = buffer,
        .offset = offset,
        .range = range,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = pipe->desc_set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer_info,
    };

    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = 
        (PFN_vkUpdateDescriptorSets)vkGetDeviceProcAddr(pipe->device->device, "vkUpdateDescriptorSets");
    if (!vkUpdateDescriptorSets) return -1;

    vkUpdateDescriptorSets(pipe->device->device, 1, &write, 0, NULL);
    return 0;
}

int wubu_vk_compute_descriptor_write_image(WubuVkComputePipeline *pipe,
                                           uint32_t binding,
                                           VkImageView image_view,
                                           VkSampler sampler,
                                           VkImageLayout layout) {
    if (!pipe || !pipe->device || !pipe->desc_set || !image_view) return -1;

    VkDescriptorImageInfo image_info = {
        .sampler = sampler,
        .imageView = image_view,
        .imageLayout = layout,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = pipe->desc_set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &image_info,
    };

    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = 
        (PFN_vkUpdateDescriptorSets)vkGetDeviceProcAddr(pipe->device->device, "vkUpdateDescriptorSets");
    if (!vkUpdateDescriptorSets) return -1;

    vkUpdateDescriptorSets(pipe->device->device, 1, &write, 0, NULL);
    return 0;
}

int wubu_vk_cmd_dispatch(WubuVkCmdPool *pool,
                         uint32_t index,
                         WubuVkComputePipeline *pipe,
                         uint32_t group_x, uint32_t group_y, uint32_t group_z,
                         uint32_t push_constant_size, const void *push_constants) {
    if (!pool || !pool->device || index >= pool->buffer_count || !pipe || !pipe->pipeline) return -1;

    VkCommandBuffer cmd = pool->buffers[index];

    PFN_vkCmdBindPipeline vkCmdBindPipeline = 
        (PFN_vkCmdBindPipeline)vkGetDeviceProcAddr(pool->device->device, "vkCmdBindPipeline");
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = 
        (PFN_vkCmdBindDescriptorSets)vkGetDeviceProcAddr(pool->device->device, "vkCmdBindDescriptorSets");
    PFN_vkCmdPushConstants vkCmdPushConstants = 
        (PFN_vkCmdPushConstants)vkGetDeviceProcAddr(pool->device->device, "vkCmdPushConstants");
    PFN_vkCmdDispatch vkCmdDispatch = 
        (PFN_vkCmdDispatch)vkGetDeviceProcAddr(pool->device->device, "vkCmdDispatch");

    if (!vkCmdBindPipeline || !vkCmdBindDescriptorSets || !vkCmdDispatch) return -1;

    /* Bind pipeline */
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->pipeline);

    /* Bind descriptor set */
    if (pipe->desc_set) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->layout,
                                0, 1, &pipe->desc_set, 0, NULL);
    }

    /* Push constants if provided */
    if (push_constants && push_constant_size > 0 && vkCmdPushConstants) {
        vkCmdPushConstants(cmd, pipe->layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, push_constant_size, push_constants);
    }

    /* Dispatch compute work */
    vkCmdDispatch(cmd, group_x, group_y, group_z);

    return 0;
}