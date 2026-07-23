/*
 * VSL Vulkan Layer - WuBuOS Vulkan Abstraction
 * 
 * Provides Vulkan 1.3+ API on WuBuOS via:
 *   - WSL/VM: VirtIO-GPU Venus (Vulkan → VirGL → host GPU)
 *   - Bare metal: VSL GPU driver (custom kernel driver)
 *   - Hosted Linux: Standard ICD loader (nouveau, radv, anv)
 *   - WSL2 GPU-PV: /dev/dxg (gfxstream_vk) — real host GPU
 *   - Machine-wide: /etc/profile.d/wubu-gpu-shim.sh selects ICDs

 *   App → vsl_vkCreateInstance() → VSL ICD Selector → Backend
 *                                              ├─ Venus (VirtIO)
 *                                              ├─ VSL GPU (bare metal)
 *                                              └─ Linux ICD (hosted)
 */

#ifndef WUBU_VSL_VULKAN_H
#define WUBU_VSL_VULKAN_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations for Wayland types to avoid dependency
struct wl_display;
struct wl_surface;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// VSL Vulkan Backend Types
// ============================================================================

typedef enum {
    WUBU_VK_BACKEND_UNKNOWN = 0,
    WUBU_VK_BACKEND_VENUS   = 1,  // VirtIO-GPU Venus (WSL/VM)
    WUBU_VK_BACKEND_VSL_GPU = 2,  // Bare metal VSL GPU driver
    WUBU_VK_BACKEND_LINUX_ICD = 3, // Standard Linux ICD (hosted)
    WUBU_VK_BACKEND_DXG     = 4,  // WSL2 GPU-PV via /dev/dxg (gfxstream_vk)
} WubuVkBackend;

// VSL Vulkan instance create info (extends VkInstanceCreateInfo)
typedef struct WubuVkInstanceCreateInfo {
    VkInstanceCreateInfo vk;           // Standard Vulkan create info
    WubuVkBackend preferred_backend;   // Preferred backend (0 = auto)
    const char *venus_socket_path;     // VirtIO socket path (WSL: /dev/virtio-ports/virtio-gpu)
    const char *vsl_gpu_device;        // VSL GPU device path (bare metal: /dev/vsl/gpu0)
    bool enable_validation;            // Enable validation layers
    bool enable_syncobj;               // Enable explicit sync (VK_KHR_synchronization2)
    uint32_t max_frames_in_flight;     // For Venus frame pacing
} WubuVkInstanceCreateInfo;

// VSL Vulkan physical device properties (extends VkPhysicalDeviceProperties2)
typedef struct WubuVkPhysicalDeviceProps {
    VkPhysicalDeviceProperties2 vk;
    WubuVkBackend backend;
    char device_path[256];
    bool supports_dmabuf;
    bool supports_explicit_sync;
    bool supports_present_wait;
    uint32_t max_frames_in_flight;
} WubuVkPhysicalDeviceProps;

// VSL Vulkan surface create info (for Wayland/X11/VBE)
typedef struct WubuVkSurfaceCreateInfo {
    VkFlags flags;
    enum {
        WUBU_VK_SURFACE_VBE = 0,      // Bare metal VBE framebuffer
        WUBU_VK_SURFACE_WAYLAND = 1,  // Wayland surface
        WUBU_VK_SURFACE_XCB = 2,      // XCB (hosted)
        WUBU_VK_SURFACE_VIRTIO = 3,   // VirtIO-GPU native
    } type;
    union {
        struct {
            void *framebuffer;        // VBE framebuffer pointer
            uint32_t width, height;
            uint32_t pitch;
            uint32_t bpp;
        } vbe;
        struct {
            void *display;            // wl_display*
            void *surface;            // wl_surface*
        } wayland;
        struct {
            void *connection;         // xcb_connection_t*
            uint32_t window;          // xcb_window_t
        } xcb;
        struct {
            int virtio_fd;
        } virtio;
    };
} WubuVkSurfaceCreateInfo;

// ============================================================================
// VSL Vulkan API
// ============================================================================

// Initialize VSL Vulkan subsystem (call once at startup)
VkResult wubu_vsl_vk_init(const WubuVkInstanceCreateInfo *create_info);

// Cleanup VSL Vulkan subsystem
void wubu_vsl_vk_cleanup(void);

// Get selected backend
WubuVkBackend wubu_vsl_vk_get_backend(void);

// Get backend name string
const char *wubu_vsl_vk_backend_name(WubuVkBackend backend);

// Create Vulkan instance (wraps vkCreateInstance with backend selection)
VkResult wubu_vsl_vk_create_instance(
    const WubuVkInstanceCreateInfo *create_info,
    const VkAllocationCallbacks *allocator,
    VkInstance *instance
);

// Enumerate physical devices (with backend info)
VkResult wubu_vsl_vk_enumerate_physical_devices(
    VkInstance instance,
    uint32_t *count,
    WubuVkPhysicalDeviceProps *props
);

// Create surface for WuBuOS display system
VkResult wubu_vsl_vk_create_surface(
    VkInstance instance,
    const WubuVkSurfaceCreateInfo *create_info,
    const VkAllocationCallbacks *allocator,
    VkSurfaceKHR *surface
);

// Get VSL GPU device FD for dmabuf import/export (bare metal)
VkResult wubu_vsl_vk_get_gpu_device_fd(VkPhysicalDevice phys_dev, int *fd);

// Venus-specific: Get VirtIO GPU context
VkResult wubu_vsl_venus_get_context(VkInstance instance, void **venus_context);

// Venus-specific: Configure frame pacing
VkResult wubu_vsl_venus_set_frame_pacing(VkInstance instance, uint32_t max_frames);

// ============================================================================
// Extension Helpers
// ============================================================================

// Check if extension is supported by current backend
bool wubu_vsl_vk_check_extension(VkPhysicalDevice phys_dev, const char *ext_name);

// Get required instance extensions for backend
const char **wubu_vsl_vk_get_required_instance_extensions(WubuVkBackend backend, uint32_t *count);

// Get required device extensions for backend
const char **wubu_vsl_vk_get_required_device_extensions(WubuVkBackend backend, uint32_t *count);

// ============================================================================
// Debug / Validation
// ============================================================================

// Set debug callback
typedef void (*WubuVkDebugCallback)(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                     VkDebugUtilsMessageTypeFlagsEXT type,
                                     const VkDebugUtilsMessengerCallbackDataEXT *data,
                                     void *user_data);

VkResult wubu_vsl_vk_set_debug_callback(WubuVkDebugCallback callback, void *user_data);

// Dump backend capabilities to log
void wubu_vsl_vk_dump_capabilities(VkInstance instance);

#ifdef __cplusplus
}
#endif

#endif // WUBU_VSL_VULKAN_H