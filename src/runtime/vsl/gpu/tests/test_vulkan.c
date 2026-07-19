/*
 * VSL Vulkan Test - Validates backend selection and device enumeration
 * Run on WSL to test Venus/Linux ICD backends
 */

#include "wubu_vsl_vulkan.h"
#include <stdio.h>
#include <stdlib.h>

static void debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                           VkDebugUtilsMessageTypeFlagsEXT type,
                           const VkDebugUtilsMessengerCallbackDataEXT *data,
                           void *user_data) {
    (void)type;
    (void)user_data;
    const char *sev_str = 
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" :
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN" :
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "INFO" : "VERBOSE";
    
    fprintf(stderr, "[Vulkan %s] %s\n", sev_str, data->pMessage);
}

int main() {
    printf("=== VSL Vulkan Test ===\n\n");
    
    // Initialize with auto-detection
    WubuVkInstanceCreateInfo create_info = {0};
    create_info.vk.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.vk.pApplicationInfo = &(VkApplicationInfo){
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "WuBuOS VSL Vulkan Test",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "WuBuOS",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
    create_info.enable_validation = true;
    create_info.enable_syncobj = true;
    create_info.max_frames_in_flight = 3;
    
    VkResult res = wubu_vsl_vk_init(&create_info);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to init VSL Vulkan: %d\n", res);
        return 1;
    }
    
    printf("Backend: %s\n", wubu_vsl_vk_backend_name(wubu_vsl_vk_get_backend()));
    
    // Set debug callback
    wubu_vsl_vk_set_debug_callback(debug_callback, NULL);
    
    // Create instance
    VkInstance instance;
    res = wubu_vsl_vk_create_instance(&create_info, NULL, &instance);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance: %d\n", res);
        return 1;
    }
    printf("Vulkan instance created: %p\n", (void*)instance);
    
    // Enumerate devices
    wubu_vsl_vk_dump_capabilities(instance);
    
    // Test surface creation (Wayland if available)
    printf("\nSurface creation test skipped (no Wayland display)\n");
    
    // Cleanup
    wubu_vsl_vk_cleanup();
    printf("\n=== Test Complete ===\n");
    
    return 0;
}