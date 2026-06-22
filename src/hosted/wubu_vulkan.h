/*
 * wubu_vulkan.h  --  WuBuOS Vulkan API for Proton/DXVK
 *
 * Cell 410: Vulkan instance/device/swapchain management.
 * Provides minimal Vulkan for Proton/DXVK to render Windows games.
 * Zero libvulkan dependency - pure Vulkan API with dynamic loading.
 */

#ifndef WUBU_VULKAN_H
#define WUBU_VULKAN_H

#include <stdint.h>
#include <stddef.h>
#include <vulkan/vulkan.h>

/* -- Instance ------------------------------------------------------ */

typedef struct {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    uint32_t api_version;
    const char **enabled_layers;
    size_t layer_count;
    const char **enabled_extensions;
    size_t extension_count;
} WubuVkInstance;

/* -- Physical Device ---------------------------------------------- */

typedef struct {
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties mem_properties;
    uint32_t graphics_queue_family;
    uint32_t compute_queue_family;
    uint32_t transfer_queue_family;
    VkSurfaceCapabilitiesKHR surface_caps;
    VkSurfaceFormatKHR *surface_formats;
    uint32_t surface_format_count;
    VkPresentModeKHR *present_modes;
    uint32_t present_mode_count;
    VkInstance instance;  /* Store instance for vkGetInstanceProcAddr */
} WubuVkPhysicalDevice;

/* -- Logical Device ----------------------------------------------- */

typedef struct {
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;
    VkQueue present_queue;
    uint32_t graphics_queue_family;
    uint32_t compute_queue_family;
    uint32_t transfer_queue_family;
    WubuVkPhysicalDevice *phys_dev;
} WubuVkDevice;

/* -- Swapchain ----------------------------------------------------- */

typedef struct {
    VkSwapchainKHR swapchain;
    VkImage *images;
    VkImageView *image_views;
    VkFramebuffer *framebuffers;
    uint32_t image_count;
    VkExtent2D extent;
    VkFormat format;
    VkColorSpaceKHR color_space;
    VkSurfaceKHR surface;
    VkSemaphore *image_acquired;
    VkSemaphore *render_finished;
    uint32_t current_image;
    WubuVkDevice *device;
} WubuVkSwapchain;

/* -- Command Pool/Buffer ------------------------------------------ */

typedef struct {
    VkCommandPool pool;
    VkCommandBuffer *buffers;
    uint32_t buffer_count;
    VkFence *fences;
    WubuVkDevice *device;
} WubuVkCmdPool;

/* -- Memory Allocator (Simple) ------------------------------------ */

typedef struct {
    VkDevice device;
    VkPhysicalDeviceMemoryProperties mem_props;
} WubuVkAllocator;

/* -- Pipeline Cache ----------------------------------------------- */

typedef struct {
    VkPipelineCache cache;
    VkDevice device;
} WubuVkPipelineCache;

/* -- Compute Pipeline --------------------------------------------- */

typedef struct {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorPool desc_pool;
    VkDescriptorSet desc_set;
    WubuVkDevice *device;
} WubuVkComputePipeline;

/* -- Compute Shader Modules --------------------------------------- */

typedef struct {
    VkShaderModule module;
    VkDevice device;
} WubuVkShaderModule;

/* -- Instance API -------------------------------------------------- */

int wubu_vk_instance_create(WubuVkInstance *inst,
                            const char *app_name,
                            uint32_t app_version,
                            const char *engine_name,
                            uint32_t engine_version,
                            const char **layers, size_t layer_count,
                            const char **extensions, size_t ext_count);

void wubu_vk_instance_destroy(WubuVkInstance *inst);

VkBool32 wubu_vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                VkDebugUtilsMessageTypeFlagsEXT type,
                                const VkDebugUtilsMessengerCallbackDataEXT *data,
                                void *user_data);

/* -- Physical Device API ------------------------------------------ */

int wubu_vk_physical_device_pick(WubuVkPhysicalDevice *phys, WubuVkInstance *inst);
int wubu_vk_physical_device_init_surface(WubuVkPhysicalDevice *phys,
                                         VkInstance instance,
                                         VkSurfaceKHR surface);

/* -- Logical Device API ------------------------------------------- */

int wubu_vk_device_create(WubuVkDevice *dev, WubuVkPhysicalDevice *phys,
                          VkSurfaceKHR surface,
                          const char **extensions, size_t ext_count);

void wubu_vk_device_destroy(WubuVkDevice *dev);

/* -- Swapchain API ------------------------------------------------ */

int wubu_vk_swapchain_create(WubuVkSwapchain *sc, WubuVkDevice *dev,
                             VkSurfaceKHR surface,
                             uint32_t width, uint32_t height,
                             VkPresentModeKHR present_mode);

void wubu_vk_swapchain_destroy(WubuVkSwapchain *sc);

int wubu_vk_swapchain_acquire(WubuVkSwapchain *sc, uint64_t timeout_ns);
int wubu_vk_swapchain_present(WubuVkSwapchain *sc);

/* -- Command Pool API --------------------------------------------- */

int wubu_vk_cmd_pool_create(WubuVkCmdPool *pool, WubuVkDevice *dev,
                            uint32_t queue_family, uint32_t count);

void wubu_vk_cmd_pool_destroy(WubuVkCmdPool *pool);

VkCommandBuffer wubu_vk_cmd_begin(WubuVkCmdPool *pool, uint32_t index);
void wubu_vk_cmd_end(WubuVkCmdPool *pool, VkCommandBuffer cmd);
void wubu_vk_cmd_submit(WubuVkDevice *dev, VkQueue queue,
                        VkCommandBuffer *cmds, uint32_t count,
                        VkSemaphore *wait_semaphores, uint32_t wait_count,
                        VkSemaphore *signal_semaphores, uint32_t signal_count,
                        VkFence fence);

/* -- Memory Allocator --------------------------------------------- */

int wubu_vk_allocator_create(WubuVkAllocator *alloc, WubuVkDevice *dev);

VkDeviceMemory wubu_vk_alloc(WubuVkAllocator *alloc,
                             VkMemoryRequirements *req,
                             VkMemoryPropertyFlags props);

void wubu_vk_free(WubuVkAllocator *alloc, VkDeviceMemory mem);

/* -- Pipeline Cache ----------------------------------------------- */

int wubu_vk_pipeline_cache_create(WubuVkPipelineCache *cache, WubuVkDevice *dev);
void wubu_vk_pipeline_cache_destroy(WubuVkPipelineCache *cache);

/* -- Utility ------------------------------------------------------ */

const char *wubu_vk_result_string(VkResult r);

uint32_t wubu_vk_find_memory_type(WubuVkAllocator *alloc,
                                  uint32_t type_filter,
                                  VkMemoryPropertyFlags props);

/* -- Compute Pipeline API ----------------------------------------- */

int wubu_vk_shader_module_create(WubuVkShaderModule *shader,
                                 WubuVkDevice *dev,
                                 const uint32_t *spirv, size_t spirv_size);

void wubu_vk_shader_module_destroy(WubuVkShaderModule *shader);

int wubu_vk_compute_pipeline_create(WubuVkComputePipeline *pipe,
                                    WubuVkDevice *dev,
                                    WubuVkShaderModule *shader,
                                    const char *entry_point,
                                    VkDescriptorSetLayoutBinding *bindings,
                                    uint32_t binding_count,
                                    VkPushConstantRange *push_constants,
                                    uint32_t push_constant_count);

void wubu_vk_compute_pipeline_destroy(WubuVkComputePipeline *pipe);

int wubu_vk_compute_descriptor_sets_alloc(WubuVkComputePipeline *pipe,
                                          VkDescriptorSetLayoutBinding *bindings,
                                          uint32_t binding_count);

void wubu_vk_compute_descriptor_sets_free(WubuVkComputePipeline *pipe);

int wubu_vk_compute_descriptor_write_buffer(WubuVkComputePipeline *pipe,
                                            uint32_t binding,
                                            VkBuffer buffer,
                                            VkDeviceSize offset,
                                            VkDeviceSize range);

int wubu_vk_compute_descriptor_write_image(WubuVkComputePipeline *pipe,
                                           uint32_t binding,
                                           VkImageView image_view,
                                           VkSampler sampler,
                                           VkImageLayout layout);

int wubu_vk_cmd_dispatch(WubuVkCmdPool *pool,
                         uint32_t index,
                         WubuVkComputePipeline *pipe,
                         uint32_t group_x, uint32_t group_y, uint32_t group_z,
                         uint32_t push_constant_size, const void *push_constants);

/* -- Deferred Operations (for threaded compilation) ------------- */

int wubu_vk_init_deferred_operation(VkDevice device);
void wubu_vk_destroy_deferred_operation(VkDevice device);

#endif /* WUBU_VULKAN_H */