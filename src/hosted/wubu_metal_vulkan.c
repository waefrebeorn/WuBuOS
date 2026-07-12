/* wubu_metal_vulkan.c -- WuBuOS Vulkan surface creation (extracted from wubu_metal.c).
 * Self-contained: forward-declares Vulkan types locally (mirrors original
 * wubu_metal.c, which did not include wubu_metal_internal.h). C11, no god headers. */

#include "wubu_metal.h"
#include <stdio.h>
#include <string.h>

extern WubuDisplay g_display;
extern bool g_initialized;
#ifdef WUBU_USE_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan_wayland.h>

/* X11 surface creation */
VkResult wubu_vk_create_xlib_surface(VkInstance instance, Display *dpy, Window window, VkSurfaceKHR *surface) {
    VkXlibSurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = dpy,
        .window = window,
    };
    PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR = 
        (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
    if (!vkCreateXlibSurfaceKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return vkCreateXlibSurfaceKHR(instance, &create_info, NULL, surface);
}

/* Wayland surface creation */
VkResult wubu_vk_create_wayland_surface(VkInstance instance, struct wl_display *display, struct wl_surface *surface, VkSurfaceKHR *vk_surface) {
    VkWaylandSurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = display,
        .surface = surface,
    };
    PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR = 
        (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
    if (!vkCreateWaylandSurfaceKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return vkCreateWaylandSurfaceKHR(instance, &create_info, NULL, vk_surface);
}

/* DRM/KMS surface creation (for bare-metal) */
VkResult wubu_vk_create_display_plane_surface(VkInstance instance, VkDisplayModeKHR display_mode, VkDisplayPlaneAlphaFlagKHR alpha, VkExtent2D *image_extent, VkSurfaceKHR *surface) {
    VkDisplaySurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
        .displayMode = display_mode,
        .planeIndex = 0,
        .planeStackIndex = 0,
        .transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .globalAlpha = 1.0f,
        .alphaMode = alpha,
        .imageExtent = *image_extent,
    };
    PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR = 
        (PFN_vkCreateDisplayPlaneSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateDisplayPlaneSurfaceKHR");
    if (!vkCreateDisplayPlaneSurfaceKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return vkCreateDisplayPlaneSurfaceKHR(instance, &create_info, NULL, surface);
}

/* Unified surface creation based on current backend */
VkResult wubu_vk_create_surface(VkInstance instance, VkSurfaceKHR *surface) {
    if (!g_initialized) return VK_ERROR_INITIALIZATION_FAILED;
    
    WubuDisplay *disp = &g_display;
    
    switch (disp->backend) {
        case DISP_X11:
            if (disp->x11_display && disp->x11_window) {
                return wubu_vk_create_xlib_surface(instance, 
                    (Display*)disp->x11_display, 
                    (Window)disp->x11_window, 
                    surface);
            }
            return VK_ERROR_INITIALIZATION_FAILED;
            
        case DISP_WAYLAND:
            /* Wayland surface requires wl_surface from compositor */
            return VK_ERROR_INITIALIZATION_FAILED;
            
        case DISP_DRM:
            /* DRM/KMS direct display requires display enumeration */
            return VK_ERROR_INITIALIZATION_FAILED;
            
        default:
            return VK_ERROR_INITIALIZATION_FAILED;
    }
}
#else
/* Local Vulkan type shims (no vulkan headers available) */
typedef int VkResult;
typedef void *VkInstance;
typedef void *VkSurfaceKHR;
typedef void *VkDisplayModeKHR;
typedef int VkDisplayPlaneAlphaFlagKHR;
typedef struct { uint32_t width, height; } VkExtent2D;
typedef struct _XDisplay Display;
typedef unsigned long Window;

#define VK_ERROR_EXTENSION_NOT_PRESENT -86
#define VK_ERROR_INITIALIZATION_FAILED -3

VkResult wubu_vk_create_xlib_surface(VkInstance instance, Display *dpy, Window window, VkSurfaceKHR *surface) {
    (void)instance; (void)dpy; (void)window; (void)surface;
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}
VkResult wubu_vk_create_wayland_surface(VkInstance instance, void *display, void *surface, VkSurfaceKHR *vk_surface) {
    (void)instance; (void)display; (void)surface; (void)vk_surface;
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}
VkResult wubu_vk_create_display_plane_surface(VkInstance instance, VkDisplayModeKHR display_mode, VkDisplayPlaneAlphaFlagKHR alpha, VkExtent2D *image_extent, VkSurfaceKHR *surface) {
    (void)instance; (void)display_mode; (void)alpha; (void)image_extent; (void)surface;
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}
VkResult wubu_vk_create_surface(VkInstance instance, VkSurfaceKHR *surface) {
    (void)instance; (void)surface;
    return VK_ERROR_INITIALIZATION_FAILED;
}
#endif
