/*
 * wubu_vulkan_swapchain.c -- WuBuOS Vulkan swapchain + presentation (extracted
 * from the monolithic wubu_vulkan.c). Self-contained; depends only on the
 * public wubu_vulkan.h API. C11, no god headers.
 */

#include "wubu_vulkan.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

