/*
 * vsl_gpu_vulkan.c  --  VSL Vulkan GPU Driver Implementation
 * 
 * Implements VSL_DRV_GPU_VULKAN driver for VSL.
 * Creates Vulkan instance/device on host, exposes handles to compositor.
 * Supports dmabuf export/import for Wayland buffer sharing.
 * Supports sync fd export/import for explicit sync.
 */

#define _GNU_SOURCE
#include "vsl/vsl_internal.h"
#include "vsl/vsl_gpu_vulkan.h"
#include "vsl/vsl_driver.h"
#include "vsl/vsl_memory.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

/* ================================================================
 * Internal Driver State
 * ================================================================ */

typedef struct {
    VkInstance        instance;
    VkPhysicalDevice  physical_device;
    VkDevice          device;
    VkQueue           graphics_queue;
    VkQueue           present_queue;
    uint32_t          graphics_queue_family;
    uint32_t          present_queue_family;
    
    /* Extension support flags */
    bool has_khr_surface;
    bool has_khr_wayland_surface;
    bool has_khr_swapchain;
    bool has_ext_image_drm_format_modifier;
    bool has_khr_external_memory_fd;
    bool has_khr_external_semaphore_fd;
    bool has_khr_timeline_semaphore;
    bool has_khr_synchronization2;
    bool has_khr_dynamic_rendering;
} VSL_Vulkan_State;

/* ================================================================
 * Vulkan Function Pointers (loaded dynamically)
 * Using vkfp_ prefix to avoid conflicts with Vulkan headers
 * ================================================================ */

static void *g_vulkan_lib = NULL;

#define VK_LOAD(fn) ((PFN_vk##fn)dlsym(g_vulkan_lib, "vk" #fn))

static PFN_vkCreateInstance vkfp_vkCreateInstance = NULL;
static PFN_vkDestroyInstance vkfp_vkDestroyInstance = NULL;
static PFN_vkEnumeratePhysicalDevices vkfp_vkEnumeratePhysicalDevices = NULL;
static PFN_vkGetPhysicalDeviceProperties2 vkfp_vkGetPhysicalDeviceProperties2 = NULL;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties2 vkfp_vkGetPhysicalDeviceQueueFamilyProperties2 = NULL;
static PFN_vkGetPhysicalDeviceFeatures2 vkfp_vkGetPhysicalDeviceFeatures2 = NULL;
static PFN_vkEnumerateDeviceExtensionProperties vkfp_vkEnumerateDeviceExtensionProperties = NULL;
static PFN_vkEnumerateInstanceExtensionProperties vkfp_vkEnumerateInstanceExtensionProperties = NULL;
static PFN_vkCreateDevice vkfp_vkCreateDevice = NULL;
static PFN_vkDestroyDevice vkfp_vkDestroyDevice = NULL;
static PFN_vkGetDeviceQueue vkfp_vkGetDeviceQueue = NULL;
static PFN_vkCreateWaylandSurfaceKHR vkfp_vkCreateWaylandSurfaceKHR = NULL;
static PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR vkfp_vkGetPhysicalDeviceWaylandPresentationSupportKHR = NULL;
static PFN_vkGetMemoryFdKHR vkfp_vkGetMemoryFdKHR = NULL;
static PFN_vkGetSemaphoreFdKHR vkfp_vkGetSemaphoreFdKHR = NULL;
static PFN_vkImportSemaphoreFdKHR vkfp_vkImportSemaphoreFdKHR = NULL;
static PFN_vkGetFenceFdKHR vkfp_vkGetFenceFdKHR = NULL;
static PFN_vkImportFenceFdKHR vkfp_vkImportFenceFdKHR = NULL;
static PFN_vkAllocateMemory vkfp_vkAllocateMemory = NULL;
static PFN_vkCreateSemaphore vkfp_vkCreateSemaphore = NULL;
static PFN_vkDestroySemaphore vkfp_vkDestroySemaphore = NULL;
static PFN_vkCreateFence vkfp_vkCreateFence = NULL;
static PFN_vkDestroyFence vkfp_vkDestroyFence = NULL;

/* Global state */
static VSL_Vulkan_State g_vk_state = {0};

/* ================================================================
 * Helper: Load Vulkan Library
 * ================================================================ */

static bool vsl_vulkan_load_library(void) {
    if (g_vulkan_lib) return true;
    
    const char *lib_names[] = {
        "libvulkan.so.1",
        "libvulkan.so",
    };
    
    for (int i = 0; i < sizeof(lib_names)/sizeof(lib_names[0]); i++) {
        g_vulkan_lib = dlopen(lib_names[i], RTLD_NOW | RTLD_LOCAL);
        if (g_vulkan_lib) break;
    }
    
    if (!g_vulkan_lib) {
        fprintf(stderr, "[VSL Vulkan] Failed to load Vulkan library: %s\n", dlerror());
        return false;
    }
    
    vkfp_vkCreateInstance = VK_LOAD(CreateInstance);
    vkfp_vkDestroyInstance = VK_LOAD(DestroyInstance);
    vkfp_vkEnumeratePhysicalDevices = VK_LOAD(EnumeratePhysicalDevices);
    vkfp_vkGetPhysicalDeviceProperties2 = VK_LOAD(GetPhysicalDeviceProperties2);
    vkfp_vkGetPhysicalDeviceQueueFamilyProperties2 = VK_LOAD(GetPhysicalDeviceQueueFamilyProperties2);
    vkfp_vkGetPhysicalDeviceFeatures2 = VK_LOAD(GetPhysicalDeviceFeatures2);
    vkfp_vkEnumerateDeviceExtensionProperties = VK_LOAD(EnumerateDeviceExtensionProperties);
    vkfp_vkCreateDevice = VK_LOAD(CreateDevice);
    vkfp_vkDestroyDevice = VK_LOAD(DestroyDevice);
    vkfp_vkGetDeviceQueue = VK_LOAD(GetDeviceQueue);
    vkfp_vkCreateWaylandSurfaceKHR = VK_LOAD(CreateWaylandSurfaceKHR);
    vkfp_vkGetPhysicalDeviceWaylandPresentationSupportKHR = VK_LOAD(GetPhysicalDeviceWaylandPresentationSupportKHR);
    vkfp_vkGetMemoryFdKHR = VK_LOAD(GetMemoryFdKHR);
    vkfp_vkGetSemaphoreFdKHR = VK_LOAD(GetSemaphoreFdKHR);
    vkfp_vkImportSemaphoreFdKHR = VK_LOAD(ImportSemaphoreFdKHR);
    vkfp_vkGetFenceFdKHR = VK_LOAD(GetFenceFdKHR);
    vkfp_vkImportFenceFdKHR = VK_LOAD(ImportFenceFdKHR);
    vkfp_vkAllocateMemory = VK_LOAD(AllocateMemory);
    vkfp_vkCreateSemaphore = VK_LOAD(CreateSemaphore);
    vkfp_vkDestroySemaphore = VK_LOAD(DestroySemaphore);
    vkfp_vkCreateFence = VK_LOAD(CreateFence);
    vkfp_vkDestroyFence = VK_LOAD(DestroyFence);
    vkfp_vkEnumerateInstanceExtensionProperties = VK_LOAD(EnumerateInstanceExtensionProperties);
    vkfp_vkEnumerateDeviceExtensionProperties = VK_LOAD(EnumerateDeviceExtensionProperties);
    
    /* Verify all function pointers loaded */
    bool all_loaded = vkfp_vkCreateInstance && vkfp_vkDestroyInstance && 
                     vkfp_vkEnumeratePhysicalDevices && vkfp_vkGetPhysicalDeviceProperties2 &&
                     vkfp_vkGetPhysicalDeviceQueueFamilyProperties2 && vkfp_vkGetPhysicalDeviceFeatures2 &&
                     vkfp_vkEnumerateDeviceExtensionProperties && vkfp_vkCreateDevice &&
                     vkfp_vkDestroyDevice && vkfp_vkGetDeviceQueue && vkfp_vkCreateWaylandSurfaceKHR &&
                     vkfp_vkGetPhysicalDeviceWaylandPresentationSupportKHR && vkfp_vkGetMemoryFdKHR &&
                     vkfp_vkGetSemaphoreFdKHR && vkfp_vkImportSemaphoreFdKHR &&
                     vkfp_vkGetFenceFdKHR && vkfp_vkImportFenceFdKHR && vkfp_vkAllocateMemory &&
                     vkfp_vkCreateSemaphore && vkfp_vkDestroySemaphore && vkfp_vkCreateFence &&
                     vkfp_vkDestroyFence && vkfp_vkEnumerateInstanceExtensionProperties &&
                     vkfp_vkEnumerateDeviceExtensionProperties;
    
    if (!all_loaded) {
        fprintf(stderr, "[VSL Vulkan] WARNING: Some Vulkan functions failed to load!\n");
        if (!vkfp_vkCreateInstance) fprintf(stderr, "  Missing: vkCreateInstance\n");
        if (!vkfp_vkDestroyInstance) fprintf(stderr, "  Missing: vkDestroyInstance\n");
        if (!vkfp_vkEnumeratePhysicalDevices) fprintf(stderr, "  Missing: vkEnumeratePhysicalDevices\n");
        if (!vkfp_vkGetPhysicalDeviceProperties2) fprintf(stderr, "  Missing: vkGetPhysicalDeviceProperties2\n");
        if (!vkfp_vkGetPhysicalDeviceQueueFamilyProperties2) fprintf(stderr, "  Missing: vkGetPhysicalDeviceQueueFamilyProperties2\n");
        if (!vkfp_vkGetPhysicalDeviceFeatures2) fprintf(stderr, "  Missing: vkGetPhysicalDeviceFeatures2\n");
        if (!vkfp_vkEnumerateDeviceExtensionProperties) fprintf(stderr, "  Missing: vkEnumerateDeviceExtensionProperties\n");
        if (!vkfp_vkCreateDevice) fprintf(stderr, "  Missing: vkCreateDevice\n");
        if (!vkfp_vkDestroyDevice) fprintf(stderr, "  Missing: vkDestroyDevice\n");
        if (!vkfp_vkGetDeviceQueue) fprintf(stderr, "  Missing: vkGetDeviceQueue\n");
        if (!vkfp_vkCreateWaylandSurfaceKHR) fprintf(stderr, "  Missing: vkCreateWaylandSurfaceKHR\n");
        if (!vkfp_vkGetPhysicalDeviceWaylandPresentationSupportKHR) fprintf(stderr, "  Missing: vkGetPhysicalDeviceWaylandPresentationSupportKHR\n");
        if (!vkfp_vkGetMemoryFdKHR) fprintf(stderr, "  Missing: vkGetMemoryFdKHR\n");
        if (!vkfp_vkGetSemaphoreFdKHR) fprintf(stderr, "  Missing: vkGetSemaphoreFdKHR\n");
        if (!vkfp_vkImportSemaphoreFdKHR) fprintf(stderr, "  Missing: vkImportSemaphoreFdKHR\n");
        if (!vkfp_vkGetFenceFdKHR) fprintf(stderr, "  Missing: vkGetFenceFdKHR\n");
        if (!vkfp_vkImportFenceFdKHR) fprintf(stderr, "  Missing: vkImportFenceFdKHR\n");
        if (!vkfp_vkAllocateMemory) fprintf(stderr, "  Missing: vkAllocateMemory\n");
        if (!vkfp_vkCreateSemaphore) fprintf(stderr, "  Missing: vkCreateSemaphore\n");
        if (!vkfp_vkDestroySemaphore) fprintf(stderr, "  Missing: vkDestroySemaphore\n");
        if (!vkfp_vkCreateFence) fprintf(stderr, "  Missing: vkCreateFence\n");
        if (!vkfp_vkDestroyFence) fprintf(stderr, "  Missing: vkDestroyFence\n");
        if (!vkfp_vkEnumerateInstanceExtensionProperties) fprintf(stderr, "  Missing: vkEnumerateInstanceExtensionProperties\n");
        if (!vkfp_vkEnumerateDeviceExtensionProperties) fprintf(stderr, "  Missing: vkEnumerateDeviceExtensionProperties\n");
    }
    
    return all_loaded;
}

/* ================================================================
 * Extension Checking
 * ================================================================ */

static bool vsl_vulkan_check_device_extensions(VkPhysicalDevice phys_dev, VSL_Vulkan_State *state) {
    uint32_t ext_count = 0;
    vkfp_vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &ext_count, NULL);
    if (ext_count == 0) return false;
    
    VkExtensionProperties *exts = calloc(ext_count, sizeof(VkExtensionProperties));
    vkfp_vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &ext_count, exts);
    
    for (uint32_t i = 0; i < ext_count; i++) {
        const char *name = exts[i].extensionName;
        if (strcmp(name, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) state->has_khr_swapchain = true;
        else if (strcmp(name, VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME) == 0) state->has_ext_image_drm_format_modifier = true;
        else if (strcmp(name, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) == 0) state->has_khr_external_memory_fd = true;
        else if (strcmp(name, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME) == 0) state->has_khr_external_semaphore_fd = true;
        else if (strcmp(name, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0) state->has_khr_timeline_semaphore = true;
        else if (strcmp(name, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) == 0) state->has_khr_synchronization2 = true;
        else if (strcmp(name, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) state->has_khr_dynamic_rendering = true;
    }
    
    free(exts);
    return true;
}

static bool vsl_vulkan_check_instance_extensions(VSL_Vulkan_State *state) {
    uint32_t ext_count = 0;
    vkfp_vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);
    if (ext_count == 0) return false;
    
    VkExtensionProperties *exts = calloc(ext_count, sizeof(VkExtensionProperties));
    vkfp_vkEnumerateInstanceExtensionProperties(NULL, &ext_count, exts);
    
    for (uint32_t i = 0; i < ext_count; i++) {
        const char *name = exts[i].extensionName;
        if (strcmp(name, VK_KHR_SURFACE_EXTENSION_NAME) == 0) state->has_khr_surface = true;
        else if (strcmp(name, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0) state->has_khr_wayland_surface = true;
    }
    
    free(exts);
    return state->has_khr_surface && state->has_khr_wayland_surface;
}

/* ================================================================
 * Driver Initialization
 * ================================================================ */

static bool vsl_vulkan_init_instance(void) {
    if (!vsl_vulkan_load_library()) return false;
    
    const char *instance_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
    };
    
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "WuBuOS Compositor",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "WuBuOS VSL",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
    
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = instance_exts,
    };
    
    VkResult res = vkfp_vkCreateInstance(&create_info, NULL, &g_vk_state.instance);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "[VSL Vulkan] Failed to create instance: %d\n", res);
        return false;
    }
    
    if (!vsl_vulkan_check_instance_extensions(&g_vk_state)) {
        fprintf(stderr, "[VSL Vulkan] Required instance extensions missing\n");
        vkfp_vkDestroyInstance(g_vk_state.instance, NULL);
        return false;
    }
    
    return true;
}

static bool vsl_vulkan_init_device(void) {
    /* Enumerate physical devices */
    uint32_t dev_count = 0;
    VkResult res = vkfp_vkEnumeratePhysicalDevices(g_vk_state.instance, &dev_count, NULL);
    if (res != VK_SUCCESS || dev_count == 0) {
        fprintf(stderr, "[VSL Vulkan] No physical devices found\n");
        return false;
    }
    
    VkPhysicalDevice *devs = calloc(dev_count, sizeof(VkPhysicalDevice));
    vkfp_vkEnumeratePhysicalDevices(g_vk_state.instance, &dev_count, devs);
    
    /* Select first suitable device with graphics queue */
    for (uint32_t i = 0; i < dev_count; i++) {
        VkPhysicalDeviceProperties2 props2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        };
        vkfp_vkGetPhysicalDeviceProperties2(devs[i], &props2);
        
        /* Check queue families */
        uint32_t qf_count = 0;
        vkfp_vkGetPhysicalDeviceQueueFamilyProperties2(devs[i], &qf_count, NULL);
        VkQueueFamilyProperties2 *qf_props = calloc(qf_count, sizeof(VkQueueFamilyProperties2));
        for (uint32_t j = 0; j < qf_count; j++) {
            qf_props[j].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        }
        vkfp_vkGetPhysicalDeviceQueueFamilyProperties2(devs[i], &qf_count, qf_props);
        
        int graphics_qf = -1, present_qf = -1;
                for (uint32_t j = 0; j < qf_count; j++) {
                    VkQueueFlags flags = qf_props[j].queueFamilyProperties.queueFlags;
                    if ((flags & VK_QUEUE_GRAPHICS_BIT) && graphics_qf < 0) {
                        graphics_qf = j;
                    }
                    /* For Wayland presentation support - we'll check at surface creation time */
                    if (g_vk_state.has_khr_wayland_surface && present_qf < 0) {
                        present_qf = j;
                    }
                }
                free(qf_props);
        
                if (graphics_qf >= 0) {
                    g_vk_state.physical_device = devs[i];
                    g_vk_state.graphics_queue_family = graphics_qf;
                    g_vk_state.present_queue_family = present_qf >= 0 ? present_qf : graphics_qf;
                    break;
                }
            }
            free(devs);
    
            if (!g_vk_state.physical_device) {
                fprintf(stderr, "[VSL Vulkan] No suitable physical device\n");
                return false;
            }
    
            /* Check device extensions */
            if (!vsl_vulkan_check_device_extensions(g_vk_state.physical_device, &g_vk_state)) {
                return false;
            }
    
            /* Create logical device */
            const char *device_exts[8];
            int ext_count = 0;
            device_exts[ext_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            if (g_vk_state.has_khr_external_memory_fd) device_exts[ext_count++] = VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME;
            if (g_vk_state.has_khr_external_semaphore_fd) device_exts[ext_count++] = VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME;
            if (g_vk_state.has_khr_timeline_semaphore) device_exts[ext_count++] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
            if (g_vk_state.has_khr_synchronization2) device_exts[ext_count++] = VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME;
            if (g_vk_state.has_khr_dynamic_rendering) device_exts[ext_count++] = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
            if (g_vk_state.has_ext_image_drm_format_modifier) device_exts[ext_count++] = VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME;
    
            float queue_priority = 1.0f;
            VkDeviceQueueCreateInfo queue_infos[2] = {0};
            int queue_info_count = 0;
    
            queue_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = g_vk_state.graphics_queue_family,
                .queueCount = 1,
                .pQueuePriorities = &queue_priority,
            };
    
            if (g_vk_state.present_queue_family != g_vk_state.graphics_queue_family) {
                queue_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = g_vk_state.present_queue_family,
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority,
                };
            }
    
            /* Use Vulkan 1.3/1.2/1.1 feature structures via pNext chain */
            VkPhysicalDeviceVulkan13Features v13_features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                .synchronization2 = g_vk_state.has_khr_synchronization2 ? VK_TRUE : VK_FALSE,
                .dynamicRendering = g_vk_state.has_khr_dynamic_rendering ? VK_TRUE : VK_FALSE,
                .pNext = NULL,
            };
            VkPhysicalDeviceVulkan12Features v12_features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .timelineSemaphore = g_vk_state.has_khr_timeline_semaphore ? VK_TRUE : VK_FALSE,
                .pNext = &v13_features,
            };
            VkPhysicalDeviceVulkan11Features v11_features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                .pNext = &v12_features,
            };
            VkPhysicalDeviceFeatures2 enabled_features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &v11_features,
            };
            VkDeviceCreateInfo dev_info = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = &enabled_features,
                .queueCreateInfoCount = queue_info_count,
                .pQueueCreateInfos = queue_infos,
                .enabledExtensionCount = ext_count,
                .ppEnabledExtensionNames = device_exts,
            };
    
            res = vkfp_vkCreateDevice(g_vk_state.physical_device, &dev_info, NULL, &g_vk_state.device);
            if (res != VK_SUCCESS) {
                fprintf(stderr, "[VSL Vulkan] Failed to create device: %d\n", res);
                return false;
            }
    
            /* Get queues */
            vkfp_vkGetDeviceQueue(g_vk_state.device, g_vk_state.graphics_queue_family, 0, &g_vk_state.graphics_queue);
    vkGetDeviceQueue(g_vk_state.device, g_vk_state.present_queue_family, 0, &g_vk_state.present_queue);
    
    fprintf(stderr, "[VSL Vulkan] Driver initialized: instance=%p, device=%p, gfx_q=%u, pres_q=%u\n",
            (void*)g_vk_state.instance, (void*)g_vk_state.device, 
            g_vk_state.graphics_queue_family, g_vk_state.present_queue_family);
    
    return true;
}

/* ================================================================
 * Public API
 * ================================================================ */

int vsl_vulkan_driver_init(void) {
    memset(&g_vk_state, 0, sizeof(g_vk_state));
    
    if (!vsl_vulkan_init_instance()) {
        return -1;
    }
    
    if (!vsl_vulkan_init_device()) {
        vkDestroyInstance(g_vk_state.instance, NULL);
        memset(&g_vk_state, 0, sizeof(g_vk_state));
        return -1;
    }
    
    /* Register with VSL driver system */
    int drv_id = vsl_register_driver(VSL_DRV_GPU_VULKAN, 0, 0, 0, 0);
    if (drv_id >= 0) {
        VSL_DRV *drv = &g_vsl.drivers[drv_id];
        drv->active = true;
        drv->priv = &g_vk_state;
    }
    
    return 0;
}

VkInstance vsl_vulkan_get_instance(void) {
    return g_vk_state.instance;
}

VkDevice vsl_vulkan_get_device(void) {
    return g_vk_state.device;
}

VkPhysicalDevice vsl_vulkan_get_physical_device(void) {
    return g_vk_state.physical_device;
}

VkQueue vsl_vulkan_get_graphics_queue(uint32_t *queue_family_out) {
    if (queue_family_out) *queue_family_out = g_vk_state.graphics_queue_family;
    return g_vk_state.graphics_queue;
}

VkQueue vsl_vulkan_get_present_queue(uint32_t *queue_family_out) {
    if (queue_family_out) *queue_family_out = g_vk_state.present_queue_family;
    return g_vk_state.present_queue;
}

VkResult vsl_vulkan_create_wayland_surface(
    struct wl_display *display,
    struct wl_surface *surface,
    VkSurfaceKHR *surface_out
) {
    if (!g_vk_state.instance || !vkfp_vkCreateWaylandSurfaceKHR) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    
    VkWaylandSurfaceCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = display,
        .surface = surface,
    };
    
    return vkfp_vkCreateWaylandSurfaceKHR(g_vk_state.instance, &info, NULL, surface_out);
}

bool vsl_vulkan_is_active(void) {
    return g_vk_state.instance != VK_NULL_HANDLE && g_vk_state.device != VK_NULL_HANDLE;
}

int vsl_vulkan_export_memory_fd(VkDeviceMemory memory, int *fd_out) {
    if (!g_vk_state.device || !vkfp_vkGetMemoryFdKHR || !g_vk_state.has_khr_external_memory_fd) {
        return -1;
    }
    
    VkMemoryGetFdInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    
    VkResult res = vkfp_vkGetMemoryFdKHR(g_vk_state.device, &info, fd_out);
    return res == VK_SUCCESS ? 0 : -1;
}

VkResult vsl_vulkan_import_memory_fd(int fd, VkDeviceMemory *memory_out, uint64_t *size_out) {
    if (!g_vk_state.device || !g_vk_state.has_khr_external_memory_fd) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    
    VkImportMemoryFdInfoKHR import_info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
        .fd = fd,
    };
    
    VkMemoryDedicatedAllocateInfoKHR dedicated_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
        .pNext = &import_info,
        .image = VK_NULL_HANDLE,
       
    };
    
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &dedicated_info,
        .allocationSize = *size_out,
        .memoryTypeIndex = 0,  /* TODO: find correct type */
    };
    
    VkResult res = vkfp_vkAllocateMemory(g_vk_state.device, &alloc_info, NULL, memory_out);
    return res;
}

int vsl_vulkan_export_semaphore_fd(VkSemaphore semaphore, int *fd_out) {
    if (!g_vk_state.device || !vkfp_vkGetSemaphoreFdKHR || !g_vk_state.has_khr_external_semaphore_fd) {
        return -1;
    }
    
    VkSemaphoreGetFdInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
        .semaphore = semaphore,
        .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
    };
    
    VkResult res = vkfp_vkGetSemaphoreFdKHR(g_vk_state.device, &info, fd_out);
    return res == VK_SUCCESS ? 0 : -1;
}

VkResult vsl_vulkan_import_semaphore_fd(int fd, VkSemaphore *semaphore_out) {
    if (!g_vk_state.device || !vkfp_vkImportSemaphoreFdKHR || !g_vk_state.has_khr_external_semaphore_fd) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    
    /* Create semaphore first */
    VkSemaphoreCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    
    VkResult res = vkfp_vkCreateSemaphore(g_vk_state.device, &create_info, NULL, semaphore_out);
    if (res != VK_SUCCESS) return res;
    
    VkImportSemaphoreFdInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
        .semaphore = *semaphore_out,
        .flags = 0,
        .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
        .fd = fd,
    };
    
    res = vkfp_vkImportSemaphoreFdKHR(g_vk_state.device, &info);
    if (res != VK_SUCCESS) {
        vkfp_vkDestroySemaphore(g_vk_state.device, *semaphore_out, NULL);
    }
    return res;
}

int vsl_vulkan_export_fence_fd(VkFence fence, int *fd_out) {
    if (!g_vk_state.device || !vkfp_vkGetFenceFdKHR) {
        return -1;
    }
    
    VkFenceGetFdInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR,
        .fence = fence,
        .handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT,
    };
    
    VkResult res = vkfp_vkGetFenceFdKHR(g_vk_state.device, &info, fd_out);
    return res == VK_SUCCESS ? 0 : -1;
}

VkResult vsl_vulkan_import_fence_fd(int fd, VkFence *fence_out) {
    if (!g_vk_state.device || !vkfp_vkImportFenceFdKHR) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    
    VkFenceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    
    VkResult res = vkfp_vkCreateFence(g_vk_state.device, &create_info, NULL, fence_out);
    if (res != VK_SUCCESS) return res;
    
    VkImportFenceFdInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR,
        .fence = *fence_out,
        .flags = 0,
        .handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT,
        .fd = fd,
    };
    
    res = vkfp_vkImportFenceFdKHR(g_vk_state.device, &info);
    if (res != VK_SUCCESS) {
        vkfp_vkDestroyFence(g_vk_state.device, *fence_out, NULL);
    }
    return res;
}

VkResult vsl_vulkan_get_device_properties(VkPhysicalDeviceProperties2 *props_out) {
    if (!g_vk_state.physical_device) return VK_ERROR_INITIALIZATION_FAILED;
    
    props_out->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkfp_vkGetPhysicalDeviceProperties2(g_vk_state.physical_device, props_out);
    return VK_SUCCESS;
}

VkResult vsl_vulkan_get_queue_family_properties(uint32_t *count_out, VkQueueFamilyProperties2 **props_out) {
    if (!g_vk_state.physical_device) return VK_ERROR_INITIALIZATION_FAILED;
    
    uint32_t count = 0;
    vkfp_vkGetPhysicalDeviceQueueFamilyProperties2(g_vk_state.physical_device, &count, NULL);
    if (count == 0) return VK_ERROR_INITIALIZATION_FAILED;
    
    *props_out = calloc(count, sizeof(VkQueueFamilyProperties2));
    for (uint32_t i = 0; i < count; i++) {
        (*props_out)[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    }
    
    vkfp_vkGetPhysicalDeviceQueueFamilyProperties2(g_vk_state.physical_device, &count, *props_out);
    *count_out = count;
    return VK_SUCCESS;
}