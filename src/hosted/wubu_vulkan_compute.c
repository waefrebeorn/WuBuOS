/*
 * wubu_vulkan_compute.c -- WuBuOS Vulkan compute pipeline + result-string /
 * memory-type utilities (extracted from the monolithic wubu_vulkan.c).
 * Self-contained; depends only on the public wubu_vulkan.h API. C11, no god headers.
 */

#include "wubu_vulkan.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
