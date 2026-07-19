/*
 * VSL Vulkan Implementation - WuBuOS Vulkan Abstraction
 * 
 * Backend selection:
 *   1. WSL/VM: VirtIO-GPU Venus (via libvulkan_virtio.so or Venus ICD)
 *   2. Bare metal: VSL GPU driver (custom)
 *   3. Hosted Linux: Standard ICD loader (nouveau, radv, anv)
 */

#include "wubu_vsl_vulkan.h"
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

// ============================================================================
// Internal State
// ============================================================================

static struct {
    bool initialized;
    WubuVkBackend active_backend;
    VkInstance instance;
    WubuVkDebugCallback debug_callback;
    void *debug_user_data;
    VkDebugUtilsMessengerEXT debug_messenger;
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
} vsl_vk_state = {0};

// ============================================================================
// Backend Detection
// ============================================================================

static WubuVkBackend detect_backend(const WubuVkInstanceCreateInfo *create_info) {
    if (create_info->preferred_backend != WUBU_VK_BACKEND_UNKNOWN) {
        return create_info->preferred_backend;
    }
    
    // Check for WSL (VirtIO-GPU available)
    struct stat st;
    if (stat("/dev/virtio-ports/virtio-gpu", &st) == 0 ||
        stat("/dev/virtio-ports/virtio-gpu-0", &st) == 0 ||
        access("/dev/virtgpu", F_OK) == 0) {
        return WUBU_VK_BACKEND_VENUS;
    }
    
    // Check for VSL GPU device (bare metal)
    if (create_info->vsl_gpu_device && access(create_info->vsl_gpu_device, F_OK) == 0) {
        return WUBU_VK_BACKEND_VSL_GPU;
    }
    if (access("/dev/vsl/gpu0", F_OK) == 0) {
        return WUBU_VK_BACKEND_VSL_GPU;
    }
    
    // Check for standard Linux ICDs (hosted)
    DIR *dir = opendir("/usr/share/vulkan/icd.d");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir))) {
            if (strstr(ent->d_name, ".json")) {
                closedir(dir);
                return WUBU_VK_BACKEND_LINUX_ICD;
            }
        }
        closedir(dir);
    }
    
    return WUBU_VK_BACKEND_UNKNOWN;
}

static const char *backend_name(WubuVkBackend b) {
    switch (b) {
        case WUBU_VK_BACKEND_VENUS: return "VirtIO-GPU Venus";
        case WUBU_VK_BACKEND_VSL_GPU: return "VSL GPU Driver";
        case WUBU_VK_BACKEND_LINUX_ICD: return "Linux ICD Loader";
        default: return "Unknown";
    }
}

// ============================================================================
// ICD Loading (for Venus and Linux ICD)
// ============================================================================

static VkResult load_venus_icd(VkInstance *instance, const VkInstanceCreateInfo *create_info,
                                const VkAllocationCallbacks *allocator) {
    // Try to load Venus ICD directly
    void *handle = dlopen("libvulkan_virtio.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        // Try common Venus ICD paths
        const char *paths[] = {
            "/usr/lib/x86_64-linux-gnu/libvulkan_virtio.so",
            "/usr/lib/aarch64-linux-gnu/libvulkan_virtio.so",
            "/usr/local/lib/libvulkan_virtio.so",
        };
        for (int i = 0; i < 3; i++) {
            handle = dlopen(paths[i], RTLD_NOW | RTLD_LOCAL);
            if (handle) break;
        }
    }
    if (!handle) {
        fprintf(stderr, "[VSL Vulkan] Venus ICD not found: %s\n", dlerror());
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    // Function pointer type for dlsym
    typedef VkResult (VKAPI_PTR *PFN_vkCreateInstance_t)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);

    PFN_vkCreateInstance_t vkCreateInstance = (PFN_vkCreateInstance_t)dlsym(handle, "vkCreateInstance");
    if (!vkCreateInstance) {
        dlclose(handle);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    VkResult result = vkCreateInstance(create_info, allocator, instance);
    if (result != VK_SUCCESS) {
        dlclose(handle);
    }
    // Note: We don't dlclose on success - ICD must stay loaded
    return result;
}

static VkResult load_linux_icd(VkInstance *instance, const VkInstanceCreateInfo *create_info,
                                const VkAllocationCallbacks *allocator) {
    // Use standard Vulkan loader (libvulkan.so)
    void *handle = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        handle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!handle) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    // Function pointer type for dlsym
    typedef VkResult (VKAPI_PTR *PFN_vkCreateInstance_t)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);

    PFN_vkCreateInstance_t vkCreateInstance = (PFN_vkCreateInstance_t)dlsym(handle, "vkCreateInstance");
    if (!vkCreateInstance) {
        dlclose(handle);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    VkResult result = vkCreateInstance(create_info, allocator, instance);
    return result;
}

// ============================================================================
// Validation Layers
// ============================================================================

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *data,
    void *user_data) {
    
    if (vsl_vk_state.debug_callback) {
        vsl_vk_state.debug_callback(severity, type, data, vsl_vk_state.debug_user_data);
    }
    return VK_FALSE;
}

static void setup_debug_messenger(VkInstance instance) {
    if (!vsl_vk_state.debug_callback) return;
    
    vsl_vk_state.vkCreateDebugUtilsMessengerEXT = 
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    vsl_vk_state.vkDestroyDebugUtilsMessengerEXT = 
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    
    if (!vsl_vk_state.vkCreateDebugUtilsMessengerEXT) return;
    
    VkDebugUtilsMessengerCreateInfoEXT create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_messenger_callback,
        .pUserData = NULL,
    };
    
    vsl_vk_state.vkCreateDebugUtilsMessengerEXT(instance, &create_info, NULL, &vsl_vk_state.debug_messenger);
}

// ============================================================================
// Public API
// ============================================================================

VkResult wubu_vsl_vk_init(const WubuVkInstanceCreateInfo *create_info) {
    if (vsl_vk_state.initialized) {
        return VK_SUCCESS;
    }
    
    WubuVkInstanceCreateInfo local_create_info = {0};
    if (create_info) {
        local_create_info = *create_info;
    }
    
    vsl_vk_state.active_backend = detect_backend(&local_create_info);
    fprintf(stderr, "[VSL Vulkan] Selected backend: %s\n", backend_name(vsl_vk_state.active_backend));
    
    vsl_vk_state.initialized = true;
    return VK_SUCCESS;
}

void wubu_vsl_vk_cleanup(void) {
    if (vsl_vk_state.instance) {
        if (vsl_vk_state.debug_messenger && vsl_vk_state.vkDestroyDebugUtilsMessengerEXT) {
            vsl_vk_state.vkDestroyDebugUtilsMessengerEXT(vsl_vk_state.instance, vsl_vk_state.debug_messenger, NULL);
        }
        vkDestroyInstance(vsl_vk_state.instance, NULL);
        vsl_vk_state.instance = VK_NULL_HANDLE;
    }
    memset(&vsl_vk_state, 0, sizeof(vsl_vk_state));
}

WubuVkBackend wubu_vsl_vk_get_backend(void) {
    return vsl_vk_state.active_backend;
}

const char *wubu_vsl_vk_backend_name(WubuVkBackend backend) {
    return backend_name(backend);
}

VkResult wubu_vsl_vk_create_instance(
    const WubuVkInstanceCreateInfo *create_info,
    const VkAllocationCallbacks *allocator,
    VkInstance *instance) {
    
    if (!vsl_vk_state.initialized) {
        VkResult r = wubu_vsl_vk_init(create_info);
        if (r != VK_SUCCESS) return r;
    }
    
    // Build VkInstanceCreateInfo with backend-specific extensions
    WubuVkInstanceCreateInfo local_create_info = {0};
    if (create_info) {
        local_create_info = *create_info;
    }
    local_create_info.preferred_backend = vsl_vk_state.active_backend;
    
    // Get required extensions for backend
    uint32_t ext_count = 0;
    const char **required_exts = wubu_vsl_vk_get_required_instance_extensions(
        vsl_vk_state.active_backend, &ext_count);
    
    // Merge with user extensions
    uint32_t total_ext_count = local_create_info.vk.enabledExtensionCount + ext_count;
    const char **all_exts = malloc(total_ext_count * sizeof(char*));
    if (local_create_info.vk.ppEnabledExtensionNames && local_create_info.vk.enabledExtensionCount > 0) {
        memcpy(all_exts, local_create_info.vk.ppEnabledExtensionNames, 
               local_create_info.vk.enabledExtensionCount * sizeof(char*));
    }
    memcpy(all_exts + local_create_info.vk.enabledExtensionCount, required_exts, 
           ext_count * sizeof(char*));
    
    VkInstanceCreateInfo vk_create_info = local_create_info.vk;
    vk_create_info.enabledExtensionCount = total_ext_count;
    vk_create_info.ppEnabledExtensionNames = all_exts;
    
    // Validation layers
    const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
    if (local_create_info.enable_validation) {
        vk_create_info.enabledLayerCount = 1;
        vk_create_info.ppEnabledLayerNames = validation_layers;
    }
    
    // Create instance based on backend
    VkResult result;
    switch (vsl_vk_state.active_backend) {
        case WUBU_VK_BACKEND_VENUS:
            result = load_venus_icd(instance, &vk_create_info, allocator);
            break;
        case WUBU_VK_BACKEND_LINUX_ICD:
            result = load_linux_icd(instance, &vk_create_info, allocator);
            break;
        case WUBU_VK_BACKEND_VSL_GPU:
            // Bare metal: use custom VSL GPU ICD (not implemented yet)
            fprintf(stderr, "[VSL Vulkan] VSL GPU backend not yet implemented\n");
            result = VK_ERROR_INITIALIZATION_FAILED;
            break;
        default:
            result = VK_ERROR_INITIALIZATION_FAILED;
    }
    
    free(all_exts);
    
    if (result == VK_SUCCESS) {
        vsl_vk_state.instance = *instance;
        if (local_create_info.enable_validation) {
            setup_debug_messenger(*instance);
        }
    }
    
    return result;
}

VkResult wubu_vsl_vk_enumerate_physical_devices(
    VkInstance instance,
    uint32_t *count,
    WubuVkPhysicalDeviceProps *props) {
    
    VkResult result = vkEnumeratePhysicalDevices(instance, count, NULL);
    if (result != VK_SUCCESS) return result;
    
    if (*count == 0) return VK_SUCCESS;
    
    VkPhysicalDevice *devices = malloc(*count * sizeof(VkPhysicalDevice));
    result = vkEnumeratePhysicalDevices(instance, count, devices);
    if (result != VK_SUCCESS) {
        free(devices);
        return result;
    }
    
    for (uint32_t i = 0; i < *count; i++) {
        VkPhysicalDeviceProperties2 props2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        vkGetPhysicalDeviceProperties2(devices[i], &props2);
        props[i].vk = props2;
        props[i].backend = vsl_vk_state.active_backend;
        
        // Backend-specific queries
        switch (vsl_vk_state.active_backend) {
            case WUBU_VK_BACKEND_VENUS:
                snprintf(props[i].device_path, sizeof(props[i].device_path), "/dev/virtio-ports/virtio-gpu");
                props[i].supports_dmabuf = true;
                props[i].supports_explicit_sync = true;
                props[i].supports_present_wait = true;
                props[i].max_frames_in_flight = 3;
                break;
            case WUBU_VK_BACKEND_LINUX_ICD:
                snprintf(props[i].device_path, sizeof(props[i].device_path), "Linux ICD: %s", props2.properties.deviceName);
                props[i].supports_dmabuf = true;
                props[i].supports_explicit_sync = true;
                props[i].supports_present_wait = true;
                props[i].max_frames_in_flight = 3;
                break;
            default:
                props[i].supports_dmabuf = false;
                props[i].supports_explicit_sync = false;
                props[i].supports_present_wait = false;
                props[i].max_frames_in_flight = 2;
        }
    }
    
    free(devices);
    return VK_SUCCESS;
}

VkResult wubu_vsl_vk_create_surface(
    VkInstance instance,
    const WubuVkSurfaceCreateInfo *create_info,
    const VkAllocationCallbacks *allocator,
    VkSurfaceKHR *surface) {
    
    switch (create_info->type) {
        case WUBU_VK_SURFACE_WAYLAND: {
            PFN_vkCreateWaylandSurfaceKHR fn = (PFN_vkCreateWaylandSurfaceKHR)
                vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
            if (!fn) return VK_ERROR_EXTENSION_NOT_PRESENT;
            
            VkWaylandSurfaceCreateInfoKHR wayland_info = {
                .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
                .display = create_info->wayland.display,
                .surface = create_info->wayland.surface,
            };
            return fn(instance, &wayland_info, allocator, surface);
        }
        case WUBU_VK_SURFACE_XCB: {
            // XCB surface not implemented - requires vulkan_xcb.h
            fprintf(stderr, "[VSL Vulkan] XCB surface not implemented\n");
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        case WUBU_VK_SURFACE_VIRTIO: {
            // VirtIO-GPU native surface (Venus)
            // Uses VK_KHR_display or custom Venus extension
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        case WUBU_VK_SURFACE_VBE:
        default:
            return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

VkResult wubu_vsl_vk_get_gpu_device_fd(VkPhysicalDevice phys_dev, int *fd) {
    if (vsl_vk_state.active_backend != WUBU_VK_BACKEND_VSL_GPU) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    // TODO: VSL GPU driver ioctl to get device FD
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult wubu_vsl_venus_get_context(VkInstance instance, void **venus_context) {
    if (vsl_vk_state.active_backend != WUBU_VK_BACKEND_VENUS) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    // TODO: Venus-specific context retrieval
    *venus_context = NULL;
    return VK_SUCCESS;
}

VkResult wubu_vsl_venus_set_frame_pacing(VkInstance instance, uint32_t max_frames) {
    if (vsl_vk_state.active_backend != WUBU_VK_BACKEND_VENUS) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    // TODO: Venus frame pacing configuration
    return VK_SUCCESS;
}

bool wubu_vsl_vk_check_extension(VkPhysicalDevice phys_dev, const char *ext_name) {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &count, NULL);
    VkExtensionProperties *props = malloc(count * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &count, props);
    
    bool found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(props[i].extensionName, ext_name) == 0) {
            found = true;
            break;
        }
    }
    free(props);
    return found;
}

const char **wubu_vsl_vk_get_required_instance_extensions(WubuVkBackend backend, uint32_t *count) {
    static const char *venus_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    static const char *linux_icd_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        // VK_KHR_XCB_SURFACE_EXTENSION_NAME,  // Requires vulkan_xcb.h
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    static const char *vsl_gpu_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_DISPLAY_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    
    switch (backend) {
        case WUBU_VK_BACKEND_VENUS:
            *count = sizeof(venus_exts)/sizeof(venus_exts[0]);
            return venus_exts;
        case WUBU_VK_BACKEND_LINUX_ICD:
            *count = sizeof(linux_icd_exts)/sizeof(linux_icd_exts[0]);
            return linux_icd_exts;
        case WUBU_VK_BACKEND_VSL_GPU:
            *count = sizeof(vsl_gpu_exts)/sizeof(vsl_gpu_exts[0]);
            return vsl_gpu_exts;
        default:
            *count = 0;
            return NULL;
    }
}

const char **wubu_vsl_vk_get_required_device_extensions(WubuVkBackend backend, uint32_t *count) {
    static const char *venus_device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
        VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME,
    };
    static const char *linux_device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
        VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
    };
    static const char *vsl_gpu_device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    };
    
    switch (backend) {
        case WUBU_VK_BACKEND_VENUS:
            *count = sizeof(venus_device_exts)/sizeof(venus_device_exts[0]);
            return venus_device_exts;
        case WUBU_VK_BACKEND_LINUX_ICD:
            *count = sizeof(linux_device_exts)/sizeof(linux_device_exts[0]);
            return linux_device_exts;
        case WUBU_VK_BACKEND_VSL_GPU:
            *count = sizeof(vsl_gpu_device_exts)/sizeof(vsl_gpu_device_exts[0]);
            return vsl_gpu_device_exts;
        default:
            *count = 0;
            return NULL;
    }
}

VkResult wubu_vsl_vk_set_debug_callback(WubuVkDebugCallback callback, void *user_data) {
    vsl_vk_state.debug_callback = callback;
    vsl_vk_state.debug_user_data = user_data;
    
    if (vsl_vk_state.instance && vsl_vk_state.vkCreateDebugUtilsMessengerEXT) {
        setup_debug_messenger(vsl_vk_state.instance);
    }
    return VK_SUCCESS;
}

void wubu_vsl_vk_dump_capabilities(VkInstance instance) {
    fprintf(stderr, "\n=== VSL Vulkan Capabilities ===\n");
    fprintf(stderr, "Backend: %s\n", backend_name(vsl_vk_state.active_backend));
    
    uint32_t count;
    VkResult res = wubu_vsl_vk_enumerate_physical_devices(instance, &count, NULL);
    if (res != VK_SUCCESS || count == 0) {
        fprintf(stderr, "No physical devices found\n");
        return;
    }
    
    WubuVkPhysicalDeviceProps *props = malloc(count * sizeof(WubuVkPhysicalDeviceProps));
    res = wubu_vsl_vk_enumerate_physical_devices(instance, &count, props);
    
    // We need the actual device handles to query extensions
    VkPhysicalDevice *devices = malloc(count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(instance, &count, devices);
    
    for (uint32_t i = 0; i < count; i++) {
        fprintf(stderr, "\nDevice %u: %s\n", i, props[i].vk.properties.deviceName);
        fprintf(stderr, "  API Version: %d.%d.%d\n",
            VK_VERSION_MAJOR(props[i].vk.properties.apiVersion),
            VK_VERSION_MINOR(props[i].vk.properties.apiVersion),
            VK_VERSION_PATCH(props[i].vk.properties.apiVersion));
        fprintf(stderr, "  Driver Version: %u\n", props[i].vk.properties.driverVersion);
        fprintf(stderr, "  Vendor ID: 0x%04x\n", props[i].vk.properties.vendorID);
        fprintf(stderr, "  Device ID: 0x%04x\n", props[i].vk.properties.deviceID);
        fprintf(stderr, "  Device Type: %s\n", 
            props[i].vk.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete GPU" :
            props[i].vk.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated GPU" :
            props[i].vk.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? "Virtual GPU" : "Other");
        fprintf(stderr, "  Backend: %s\n", backend_name(props[i].backend));
        fprintf(stderr, "  Device Path: %s\n", props[i].device_path);
        fprintf(stderr, "  DMABUF: %s\n", props[i].supports_dmabuf ? "Yes" : "No");
        fprintf(stderr, "  Explicit Sync: %s\n", props[i].supports_explicit_sync ? "Yes" : "No");
        fprintf(stderr, "  Present Wait: %s\n", props[i].supports_present_wait ? "Yes" : "No");
        fprintf(stderr, "  Max Frames in Flight: %u\n", props[i].max_frames_in_flight);
        
        // Device extensions
        uint32_t ext_count;
        vkEnumerateDeviceExtensionProperties(devices[i], NULL, &ext_count, NULL);
        VkExtensionProperties *ext_props = malloc(ext_count * sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(devices[i], NULL, &ext_count, ext_props);
        fprintf(stderr, "  Extensions (%u):\n", ext_count);
        for (uint32_t j = 0; j < ext_count && j < 10; j++) {
            fprintf(stderr, "    %s\n", ext_props[j].extensionName);
        }
        if (ext_count > 10) {
            fprintf(stderr, "    ... and %u more\n", ext_count - 10);
        }
        free(ext_props);
    }
    
    free(props);
    free(devices);
    fprintf(stderr, "================================\n\n");
}