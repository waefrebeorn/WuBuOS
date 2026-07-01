/*
 * vsl_gpu_vulkan.h  --  VSL Vulkan GPU Driver Public API
 * 
 * This header exposes the Vulkan driver functionality to the compositor
 * and other components that need Vulkan instance/device access.
 */

#ifndef WUBUOS_VSL_GPU_VULKAN_H
#define WUBUOS_VSL_GPU_VULKAN_H

#include <stdint.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>

/* Forward declarations for Wayland types to avoid "declared inside parameter list" warnings */
struct wl_display;
struct wl_surface;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the VSL Vulkan driver (registers with VSL driver system) */
int vsl_vulkan_driver_init(void);

/* Get Vulkan instance (creates if not exists) */
VkInstance vsl_vulkan_get_instance(void);

/* Get Vulkan device (creates if not exists) */
VkDevice vsl_vulkan_get_device(void);

/* Get physical device */
VkPhysicalDevice vsl_vulkan_get_physical_device(void);

/* Get graphics queue */
VkQueue vsl_vulkan_get_graphics_queue(uint32_t *queue_family_out);

/* Get present queue */
VkQueue vsl_vulkan_get_present_queue(uint32_t *queue_family_out);

/* Create Wayland surface for VSL compositor */
VkResult vsl_vulkan_create_wayland_surface(
    struct wl_display *display,
    struct wl_surface *surface,
    VkSurfaceKHR *surface_out
);

/* Check if Vulkan driver is active */
bool vsl_vulkan_is_active(void);

/* Export Vulkan memory as dmabuf fd (for Wayland dmabuf protocol) */
int vsl_vulkan_export_memory_fd(VkDeviceMemory memory, int *fd_out);

/* Import dmabuf fd as Vulkan memory */
VkResult vsl_vulkan_import_memory_fd(int fd, VkDeviceMemory *memory_out, uint64_t *size_out);

/* Export Vulkan semaphore as sync fd */
int vsl_vulkan_export_semaphore_fd(VkSemaphore semaphore, int *fd_out);

/* Import sync fd as Vulkan semaphore */
VkResult vsl_vulkan_import_semaphore_fd(int fd, VkSemaphore *semaphore_out);

/* Export Vulkan fence as sync fd */
int vsl_vulkan_export_fence_fd(VkFence fence, int *fd_out);

/* Import sync fd as Vulkan fence */
VkResult vsl_vulkan_import_fence_fd(int fd, VkFence *fence_out);

/* Get physical device properties for compositor */
VkResult vsl_vulkan_get_device_properties(VkPhysicalDeviceProperties2 *props_out);

/* Get queue family properties */
VkResult vsl_vulkan_get_queue_family_properties(uint32_t *count_out, VkQueueFamilyProperties2 **props_out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOS_VSL_GPU_VULKAN_H */