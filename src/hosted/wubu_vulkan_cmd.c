/*
 * wubu_vulkan_cmd.c -- WuBuOS Vulkan command-pool + queue-submit helpers
 * (extracted from the monolithic wubu_vulkan.c). Self-contained; depends only
 * on the public wubu_vulkan.h API. C11, no god headers.
 */

#include "wubu_vulkan.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
