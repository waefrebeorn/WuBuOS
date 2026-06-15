/*
 * bear_vulkan.c  --  BearRL Vulkan Host Context Management (Pure C11)
 *
 * Vulkan instance/device initialization, memory management, pipeline caching.
 * Compute shaders live in bear_vulkan_shaders/ as .comp files.
 */

#include "bear_vulkan.h"
#include "bear_arena.h"
#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ==================================================================
 * Internal Context Structure
 * ================================================================== */

#define BEAR_VK_MAX_PIPELINES 64
#define BEAR_VK_MAX_SETS 128
#define BEAR_VK_MAX_QUEUES 4

struct BearVulkanPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    char name[64];
    int active;
};

struct BearVulkanContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue compute_queue;
    uint32_t compute_queue_family;
    VkCommandPool command_pool;
    VkDescriptorPool descriptor_pool;
    
    int initialized;
    
    /* Pipelines cache */
    struct BearVulkanPipeline pipelines[BEAR_VK_MAX_PIPELINES];
    int num_pipelines;
    
    /* Descriptor set layout for storage buffers */
    VkDescriptorSetLayout storage_desc_layout;
    
    /* Memory properties */
    VkPhysicalDeviceMemoryProperties mem_props;
    VkPhysicalDeviceProperties dev_props;
    VkPhysicalDeviceFeatures dev_features;
    
    /* Error tracking */
    VkResult last_result;
    char last_error_str[512];
    
    /* Profiling */
    int profiling_enabled;
    int num_profile_events;
    BearVulkanProfileEvent profile_events[128];
    
    /* Arena allocator for temp buffers */
    struct {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize offset;
        VkDeviceSize capacity;
        int owns_memory;
    } default_arena;
};

struct BearVulkanArena {
    BearVulkanContext* ctx;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize capacity;
    int owns_memory;
};

/* ==================================================================
 * Error Handling
 * ================================================================== */

static void vk_check(BearVulkanContext* ctx, VkResult result, const char* file, int line) {
    if (result != VK_SUCCESS) {
        ctx->last_result = result;
        snprintf(ctx->last_error_str, 512, "%s:%d: VkResult %d", file, line, result);
    }
}

#define VK_CHECK(ctx, expr) vk_check(ctx, (expr), __FILE__, __LINE__)

static const char* vk_result_str(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        default: return "VK_UNKNOWN_ERROR";
    }
}

/* ==================================================================
 * Utility: Find Memory Type
 * ================================================================== */

static int find_memory_type(BearVulkanContext* ctx, uint32_t type_filter, VkMemoryPropertyFlags props) {
    for (uint32_t i = 0; i < ctx->mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && 
            (ctx->mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return -1;
}

/* ==================================================================
 * Shader Module Creation
 * ================================================================== */

static VkShaderModule create_shader_module(BearVulkanContext* ctx, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        snprintf(ctx->last_error_str, 512, "Failed to open shader: %s", path);
        return VK_NULL_HANDLE;
    }
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* code = malloc(size);
    fread(code, 1, size, f);
    fclose(f);
    
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (const uint32_t*)code
    };
    
    VkShaderModule module;
    VK_CHECK(ctx, vkCreateShaderModule(ctx->device, &create_info, NULL, &module));
    
    free(code);
    return module;
}

/* ==================================================================
 * Pipeline Creation
 * ================================================================== */

static VkPipelineLayout create_pipeline_layout(BearVulkanContext* ctx, VkDescriptorSetLayout desc_layout) {
    VkPipelineLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &(VkPushConstantRange){
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = 64  /* Max push constant size we'll use */
        }
    };
    
    VkPipelineLayout layout;
    VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &info, NULL, &layout));
    return layout;
}

static VkPipeline create_compute_pipeline(BearVulkanContext* ctx, VkPipelineLayout layout, VkShaderModule shader) {
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main"
        },
        .layout = layout
    };
    
    VkPipeline pipeline;
    VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &info, NULL, &pipeline));
    return pipeline;
}

/* ==================================================================
 * Public API: Query
 * ================================================================== */

BearVulkanStatus bear_vulkan_query(void) {
    /* Check if Vulkan loader is available by trying to create instance */
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "BearRL",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "BearRL",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };
    
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };
    
    VkInstance instance;
    VkResult result = vkCreateInstance(&create_info, NULL, &instance);
    if (result != VK_SUCCESS) {
        return BEAR_VULKAN_UNAVAILABLE;
    }
    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    vkDestroyInstance(instance, NULL);
    
    return device_count > 0 ? BEAR_VULKAN_AVAILABLE : BEAR_VULKAN_UNAVAILABLE;
}

int bear_vulkan_get_device_info(int device_index, BearVulkanDeviceInfo* info) {
    if (!info) return -1;
    
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_0
    };
    
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };
    
    VkInstance instance;
    if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) return -1;
    
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    if (device_index < 0 || (uint32_t)device_index >= count) {
        vkDestroyInstance(instance, NULL);
        return -1;
    }
    
    VkPhysicalDevice* devices = malloc(count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(instance, &count, devices);
    
    VkPhysicalDevice pd = devices[device_index];
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(pd, &props);
    vkGetPhysicalDeviceFeatures(pd, &features);
    
    info->device_index = device_index;
    strncpy(info->name, props.deviceName, 255);
    snprintf(info->driver_version, 63, "%d.%d.%d",
             VK_VERSION_MAJOR(props.driverVersion),
             VK_VERSION_MINOR(props.driverVersion),
             VK_VERSION_PATCH(props.driverVersion));
    info->api_version = props.apiVersion;
    info->vendor_id = props.vendorID;
    info->device_id = props.deviceID;
    info->max_compute_workgroup_count[0] = props.limits.maxComputeWorkGroupCount[0];
    info->max_compute_workgroup_count[1] = props.limits.maxComputeWorkGroupCount[1];
    info->max_compute_workgroup_count[2] = props.limits.maxComputeWorkGroupCount[2];
    info->max_compute_workgroup_size[0] = props.limits.maxComputeWorkGroupSize[0];
    info->max_compute_workgroup_size[1] = props.limits.maxComputeWorkGroupSize[1];
    info->max_compute_workgroup_size[2] = props.limits.maxComputeWorkGroupSize[2];
    info->max_compute_workgroup_invocations = props.limits.maxComputeWorkGroupInvocations;
    info->max_storage_buffer_range = props.limits.maxStorageBufferRange;
    info->max_uniform_buffer_range = props.limits.maxUniformBufferRange;
    info->max_push_constants_size = props.limits.maxPushConstantsSize;
    info->subgroup_size = 32; /* Default, query via VK_EXT_subgroup_size if available */
    info->has_subgroup_ops = 0; /* Requires VK_KHR_shader_subgroup */
    /* Check for subgroup support via extension */
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(pd, NULL, &ext_count, NULL);
    VkExtensionProperties* exts = malloc(ext_count * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(pd, NULL, &ext_count, exts);
    for (uint32_t i = 0; i < ext_count; ++i) {
        if (strcmp(exts[i].extensionName, "VK_KHR_shader_subgroup") == 0) {
            info->has_subgroup_ops = 1;
            break;
        }
    }
    free(exts);
    info->has_shader_float16 = 0; /* Requires VK_KHR_shader_float16_int8 */
    info->has_shader_float64 = features.shaderFloat64;
    
    free(devices);
    vkDestroyInstance(instance, NULL);
    return 0;
}

/* ==================================================================
 * Context Management
 * ================================================================== */

static int select_compute_queue_family(BearVulkanContext* ctx) {
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_count, NULL);
    
    VkQueueFamilyProperties* props = malloc(queue_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_count, props);
    
    for (uint32_t i = 0; i < queue_count; ++i) {
        if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            /* Prefer compute-only queue */
            if (!(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                free(props);
                return i;
            }
        }
    }
    
    /* Fall back to any queue with compute */
    for (uint32_t i = 0; i < queue_count; ++i) {
        if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            free(props);
            return i;
        }
    }
    
    free(props);
    return -1;
}

BearVulkanContext* bear_vulkan_init(int device_index) {
    if (bear_vulkan_query() == BEAR_VULKAN_UNAVAILABLE) {
        return NULL;
    }
    
    BearVulkanContext* ctx = calloc(1, sizeof(BearVulkanContext));
    if (!ctx) return NULL;
    
    /* Create instance */
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "BearRL",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "BearRL",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };
    
    /* Enable validation layers in debug */
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    uint32_t layer_count = 0;
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = layer_count,
        .ppEnabledLayerNames = layers
    };
    
    VK_CHECK(ctx, vkCreateInstance(&instance_info, NULL, &ctx->instance));
    if (ctx->last_result != VK_SUCCESS) {
        free(ctx);
        return NULL;
    }
    
    /* Select physical device */
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &count, NULL);
    VkPhysicalDevice* devices = malloc(count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ctx->instance, &count, devices);
    
    if (device_index < 0) device_index = 0;
    if ((uint32_t)device_index >= count) device_index = 0;
    
    ctx->physical_device = devices[device_index];
    vkGetPhysicalDeviceProperties(ctx->physical_device, &ctx->dev_props);
    vkGetPhysicalDeviceFeatures(ctx->physical_device, &ctx->dev_features);
    vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, &ctx->mem_props);
    
    free(devices);
    
    /* Find compute queue family */
    ctx->compute_queue_family = select_compute_queue_family(ctx);
    if (ctx->compute_queue_family == (uint32_t)-1) {
        bear_vulkan_destroy(ctx);
        return NULL;
    }
    
    /* Create logical device */
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ctx->compute_queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };
    
    VkPhysicalDeviceFeatures enabled_features = {
        .shaderStorageBufferArrayDynamicIndexing = VK_TRUE,
        .shaderUniformBufferArrayDynamicIndexing = VK_TRUE,
        .shaderClipDistance = VK_TRUE,
        .shaderCullDistance = VK_TRUE,
        .shaderFloat64 = ctx->dev_features.shaderFloat64,
        .shaderInt64 = ctx->dev_features.shaderInt64,
        .shaderInt16 = ctx->dev_features.shaderInt16,
    };
    
    const char* device_extensions[] = {
        VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
    };
    
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = device_extensions,
        .pEnabledFeatures = &enabled_features
    };
    
    VK_CHECK(ctx, vkCreateDevice(ctx->physical_device, &device_info, NULL, &ctx->device));
    if (ctx->last_result != VK_SUCCESS) {
        bear_vulkan_destroy(ctx);
        return NULL;
    }
    
    vkGetDeviceQueue(ctx->device, ctx->compute_queue_family, 0, &ctx->compute_queue);
    
    /* Create command pool */
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx->compute_queue_family
    };
    VK_CHECK(ctx, vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->command_pool));
    
    /* Create descriptor pool */
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 },
    };
    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = BEAR_VK_MAX_SETS,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes
    };
    VK_CHECK(ctx, vkCreateDescriptorPool(ctx->device, &desc_pool_info, NULL, &ctx->descriptor_pool));
    
    /* Create descriptor set layout for storage buffers */
    VkDescriptorSetLayoutBinding bindings[] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 8, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 8, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 4, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
    };
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };
    VK_CHECK(ctx, vkCreateDescriptorSetLayout(ctx->device, &layout_info, NULL, &ctx->storage_desc_layout));
    
    ctx->num_pipelines = 0;
    ctx->profiling_enabled = 0;
    ctx->initialized = 1;
    
    return ctx;
}

void bear_vulkan_destroy(BearVulkanContext* ctx) {
    if (!ctx) return;
    
    /* Destroy pipelines */
    for (int i = 0; i < ctx->num_pipelines; ++i) {
        if (ctx->pipelines[i].pipeline) vkDestroyPipeline(ctx->device, ctx->pipelines[i].pipeline, NULL);
        if (ctx->pipelines[i].layout) vkDestroyPipelineLayout(ctx->device, ctx->pipelines[i].layout, NULL);
    }
    
    if (ctx->storage_desc_layout) vkDestroyDescriptorSetLayout(ctx->device, ctx->storage_desc_layout, NULL);
    if (ctx->descriptor_pool) vkDestroyDescriptorPool(ctx->device, ctx->descriptor_pool, NULL);
    if (ctx->command_pool) vkDestroyCommandPool(ctx->device, ctx->command_pool, NULL);
    if (ctx->device) vkDestroyDevice(ctx->device, NULL);
    if (ctx->instance) vkDestroyInstance(ctx->instance, NULL);
    
    if (ctx->default_arena.buffer) vkDestroyBuffer(ctx->device, ctx->default_arena.buffer, NULL);
    if (ctx->default_arena.memory) vkFreeMemory(ctx->device, ctx->default_arena.memory, NULL);
    
    free(ctx);
}

void bear_vulkan_sync(BearVulkanContext* ctx) {
    if (!ctx) return;
    vkQueueWaitIdle(ctx->compute_queue);
}

const char* bear_vulkan_last_error(BearVulkanContext* ctx) {
    if (!ctx) return "NULL context";
    if (ctx->last_result == VK_SUCCESS) return "No error";
    static char buf[512];
    snprintf(buf, 512, "%s (%s)", ctx->last_error_str, vk_result_str(ctx->last_result));
    return buf;
}

/* ==================================================================
 * Vulkan Arena
 * ================================================================== */

BearVulkanArena* bear_vulkan_arena_create(BearVulkanContext* ctx, size_t capacity_bytes) {
    if (!ctx || !ctx->initialized) return NULL;
    
    BearVulkanArena* arena = calloc(1, sizeof(BearVulkanArena));
    if (!arena) return NULL;
    
    arena->ctx = ctx;
    arena->capacity = capacity_bytes;
    arena->offset = 0;
    arena->owns_memory = 1;
    
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = capacity_bytes,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(ctx, vkCreateBuffer(ctx->device, &buf_info, NULL, &arena->buffer));
    
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx->device, arena->buffer, &mem_req);
    
    int mem_type = find_memory_type(ctx, mem_req.memoryTypeBits, 
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_type < 0) {
        mem_type = find_memory_type(ctx, mem_req.memoryTypeBits, 0);
    }
    if (mem_type < 0) {
        bear_vulkan_arena_destroy(arena);
        return NULL;
    }
    
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type
    };
    VK_CHECK(ctx, vkAllocateMemory(ctx->device, &alloc_info, NULL, &arena->memory));
    
    VK_CHECK(ctx, vkBindBufferMemory(ctx->device, arena->buffer, arena->memory, 0));
    
    return arena;
}

void bear_vulkan_arena_reset(BearVulkanArena* arena) {
    if (!arena) return;
    arena->offset = 0;
}

void bear_vulkan_arena_destroy(BearVulkanArena* arena) {
    if (!arena) return;
    if (arena->owns_memory) {
        if (arena->buffer) vkDestroyBuffer(arena->ctx->device, arena->buffer, NULL);
        if (arena->memory) vkFreeMemory(arena->ctx->device, arena->memory, NULL);
    }
    free(arena);
}

VkBuffer bear_vulkan_arena_alloc_buffer(BearVulkanArena* arena, size_t bytes, VkBufferUsageFlags usage) {
    if (!arena || !arena->ctx || !arena->buffer) return VK_NULL_HANDLE;
    
    /* Align to 256 bytes */
    size_t aligned_offset = (arena->offset + 255) & ~255;
    if (aligned_offset + bytes > arena->capacity) {
        return VK_NULL_HANDLE;
    }
    
    /* Return sub-buffer - we use the same buffer with offset in descriptor sets */
    arena->offset = aligned_offset + bytes;
    return arena->buffer;  /* Caller must track offset manually */
}

/* ==================================================================
 * Vulkan Tensor Operations
 * ================================================================== */

static size_t dtype_size_vk(int dtype) {
    switch (dtype) {
        case 0: return sizeof(float);
        case 1: return sizeof(double);
        case 2: return sizeof(int32_t);
        case 3: return sizeof(uint8_t);
        default: return sizeof(float);
    }
}

static int64_t tensor_numel_vk(const int64_t* shape, int ndim) {
    int64_t n = 1;
    for (int i = 0; i < ndim; ++i) n *= shape[i];
    return n;
}

static VkBuffer create_tensor_buffer(BearVulkanContext* ctx, size_t bytes) {
    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer buffer;
    VK_CHECK(ctx, vkCreateBuffer(ctx->device, &info, NULL, &buffer));
    return buffer;
}

static VkDeviceMemory allocate_buffer_memory(BearVulkanContext* ctx, VkBuffer buffer, VkMemoryPropertyFlags props) {
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(ctx->device, buffer, &req);
    
    int mem_type = find_memory_type(ctx, req.memoryTypeBits, props);
    if (mem_type < 0) mem_type = find_memory_type(ctx, req.memoryTypeBits, 0);
    if (mem_type < 0) return VK_NULL_HANDLE;
    
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = mem_type
    };
    VkDeviceMemory memory;
    VK_CHECK(ctx, vkAllocateMemory(ctx->device, &alloc_info, NULL, &memory));
    VK_CHECK(ctx, vkBindBufferMemory(ctx->device, buffer, memory, 0));
    return memory;
}

int bear_vulkan_tensor_from_host(BearVulkanContext* ctx, BearVulkanArena* arena,
                                  const BearTensor* host, BearVulkanTensor* gpu) {
    if (!ctx || !host || !gpu) return -1;
    
    int64_t numel = bear_tensor_numel(host);
    size_t element_size = dtype_size_vk(host->dtype);
    size_t bytes = numel * element_size;
    
    gpu->ndim = host->ndim;
    gpu->dtype = host->dtype;
    gpu->numel = numel;
    gpu->size = bytes;
    gpu->offset = 0;
    gpu->mapped = 0;
    gpu->mapped_ptr = NULL;
    
    gpu->shape = malloc(host->ndim * sizeof(int64_t));
    if (!gpu->shape) return -1;
    memcpy(gpu->shape, host->shape, host->ndim * sizeof(int64_t));
    
    /* Create staging buffer for upload */
    VkBuffer staging = create_tensor_buffer(ctx, bytes);
    VkDeviceMemory staging_mem = allocate_buffer_memory(ctx, staging, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!staging_mem) {
        vkDestroyBuffer(ctx->device, staging, NULL);
        return -1;
    }
    
    void* mapped;
    vkMapMemory(ctx->device, staging_mem, 0, bytes, 0, &mapped);
    memcpy(mapped, host->data, bytes);
    vkUnmapMemory(ctx->device, staging_mem);
    
    /* Create device-local buffer */
    gpu->buffer = create_tensor_buffer(ctx, bytes);
    gpu->memory = allocate_buffer_memory(ctx, gpu->buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!gpu->memory) {
        vkDestroyBuffer(ctx->device, staging, NULL);
        vkFreeMemory(ctx->device, staging_mem, NULL);
        vkDestroyBuffer(ctx->device, gpu->buffer, NULL);
        return -1;
    }
    
    /* Copy staging -> device */
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(ctx->device, &alloc_info, &cmd);
    
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cmd, &begin);
    
    VkBufferCopy copy = { .srcOffset = 0, .dstOffset = 0, .size = bytes };
    vkCmdCopyBuffer(cmd, staging, gpu->buffer, 1, &copy);
    
    vkEndCommandBuffer(cmd);
    
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
    vkQueueSubmit(ctx->compute_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->compute_queue);
    
    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &cmd);
    vkDestroyBuffer(ctx->device, staging, NULL);
    vkFreeMemory(ctx->device, staging_mem, NULL);
    
    return 0;
}

int bear_vulkan_tensor_create(BearVulkanContext* ctx, BearVulkanArena* arena,
                               const int64_t* shape, int ndim, int dtype,
                               BearVulkanTensor* gpu, const char* name) {
    (void)name;
    if (!ctx || !gpu) return -1;
    
    int64_t numel = tensor_numel_vk(shape, ndim);
    size_t element_size = dtype_size_vk(dtype);
    size_t bytes = numel * element_size;
    
    gpu->ndim = ndim;
    gpu->dtype = dtype;
    gpu->numel = numel;
    gpu->size = bytes;
    gpu->offset = 0;
    gpu->mapped = 0;
    gpu->mapped_ptr = NULL;
    
    gpu->shape = malloc(ndim * sizeof(int64_t));
    if (!gpu->shape) return -1;
    memcpy(gpu->shape, shape, ndim * sizeof(int64_t));
    
    gpu->buffer = create_tensor_buffer(ctx, bytes);
    gpu->memory = allocate_buffer_memory(ctx, gpu->buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!gpu->memory) {
        vkDestroyBuffer(ctx->device, gpu->buffer, NULL);
        free(gpu->shape);
        return -1;
    }
    
    return 0;
}

int bear_vulkan_tensor_to_host(BearVulkanContext* ctx, const BearVulkanTensor* gpu, BearTensor* host) {
    if (!ctx || !gpu || !host) return -1;
    
    /* Create staging buffer */
    VkBuffer staging = create_tensor_buffer(ctx, gpu->size);
    VkDeviceMemory staging_mem = allocate_buffer_memory(ctx, staging,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!staging_mem) {
        vkDestroyBuffer(ctx->device, staging, NULL);
        return -1;
    }
    
    /* Copy device -> staging */
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(ctx->device, &alloc_info, &cmd);
    
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cmd, &begin);
    
    VkBufferCopy copy = { .srcOffset = 0, .dstOffset = 0, .size = gpu->size };
    vkCmdCopyBuffer(cmd, gpu->buffer, staging, 1, &copy);
    
    vkEndCommandBuffer(cmd);
    
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
    vkQueueSubmit(ctx->compute_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->compute_queue);
    
    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &cmd);
    
    /* Map and copy to host */
    void* mapped;
    vkMapMemory(ctx->device, staging_mem, 0, gpu->size, 0, &mapped);
    memcpy(host->data, mapped, gpu->size);
    vkUnmapMemory(ctx->device, staging_mem);
    
    vkDestroyBuffer(ctx->device, staging, NULL);
    vkFreeMemory(ctx->device, staging_mem, NULL);
    
    return 0;
}

int bear_vulkan_tensor_copy(BearVulkanContext* ctx, const BearVulkanTensor* src, BearVulkanTensor* dst) {
    if (!ctx || !src || !dst) return -1;
    if (src->size != dst->size) return -1;
    
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(ctx->device, &alloc_info, &cmd);
    
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cmd, &begin);
    
    VkBufferCopy copy = { .srcOffset = 0, .dstOffset = 0, .size = src->size };
    vkCmdCopyBuffer(cmd, src->buffer, dst->buffer, 1, &copy);
    
    vkEndCommandBuffer(cmd);
    
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
    vkQueueSubmit(ctx->compute_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->compute_queue);
    
    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &cmd);
    
    return 0;
}

int bear_vulkan_tensor_map(BearVulkanContext* ctx, BearVulkanTensor* gpu) {
    if (!ctx || !gpu || gpu->mapped) return -1;
    
    /* Need host-visible memory for persistent mapping */
    /* This requires the buffer to be created with host-visible memory */
    /* For now, return error - would need separate buffer type */
    return -1;
}

void bear_vulkan_tensor_unmap(BearVulkanContext* ctx, BearVulkanTensor* gpu) {
    if (!ctx || !gpu || !gpu->mapped) return;
    vkUnmapMemory(ctx->device, gpu->memory);
    gpu->mapped = 0;
    gpu->mapped_ptr = NULL;
}

void bear_vulkan_tensor_free(BearVulkanContext* ctx, BearVulkanArena* arena, BearVulkanTensor* gpu) {
    (void)arena;
    if (!ctx || !gpu) return;
    if (gpu->shape) { free(gpu->shape); gpu->shape = NULL; }
    if (gpu->buffer) { vkDestroyBuffer(ctx->device, gpu->buffer, NULL); gpu->buffer = VK_NULL_HANDLE; }
    if (gpu->memory) { vkFreeMemory(ctx->device, gpu->memory, NULL); gpu->memory = VK_NULL_HANDLE; }
}

/* ==================================================================
 * Pipeline Cache / Get-or-Create
 * ================================================================== */

static struct BearVulkanPipeline* get_or_create_pipeline(BearVulkanContext* ctx, const char* name, const char* shader_path) {
    for (int i = 0; i < ctx->num_pipelines; ++i) {
        if (strcmp(ctx->pipelines[i].name, name) == 0 && ctx->pipelines[i].active) {
            return &ctx->pipelines[i];
        }
    }
    
    if (ctx->num_pipelines >= BEAR_VK_MAX_PIPELINES) return NULL;
    
    struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
    strncpy(p->name, name, 63);
    p->name[63] = '\0';
    
    VkShaderModule shader = create_shader_module(ctx, shader_path);
    if (shader == VK_NULL_HANDLE) return NULL;
    
    p->layout = create_pipeline_layout(ctx, ctx->storage_desc_layout);
    p->pipeline = create_compute_pipeline(ctx, p->layout, shader);
    p->active = 1;
    
    vkDestroyShaderModule(ctx->device, shader, NULL);
    return p;
}

/* ==================================================================
 * Profiling
 * ================================================================== */

void bear_vulkan_profile_enable(BearVulkanContext* ctx, int enable) {
    if (!ctx) return;
    ctx->profiling_enabled = enable;
    if (enable) ctx->num_profile_events = 0;
}

int bear_vulkan_profile_get_events(BearVulkanContext* ctx, BearVulkanProfileEvent* out, int max_events) {
    if (!ctx || !out || max_events <= 0) return 0;
    int n = ctx->num_profile_events < max_events ? ctx->num_profile_events : max_events;
    memcpy(out, ctx->profile_events, n * sizeof(BearVulkanProfileEvent));
    return n;
}

void bear_vulkan_profile_reset(BearVulkanContext* ctx) {
    if (!ctx) return;
    ctx->num_profile_events = 0;
}

/* ==================================================================
 * Descriptor Set Helpers
 * ================================================================== */

static VkDescriptorSet allocate_descriptor_set(BearVulkanContext* ctx) {
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ctx->storage_desc_layout
    };
    VkDescriptorSet set;
    VK_CHECK(ctx, vkAllocateDescriptorSets(ctx->device, &alloc_info, &set));
    return set;
}

static void update_descriptor_set_storage(BearVulkanContext* ctx, VkDescriptorSet set, uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    VkDescriptorBufferInfo info = { .buffer = buffer, .offset = offset, .range = range };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &info
    };
    vkUpdateDescriptorSets(ctx->device, 1, &write, 0, NULL);
}

/* ==================================================================
 * Dispatch Helper
 * ================================================================== */

static void dispatch_compute(BearVulkanContext* ctx, VkPipeline pipeline, VkPipelineLayout layout,
                              VkDescriptorSet set, const struct BearVulkanPipeline* p,
                              uint32_t group_x, uint32_t group_y, uint32_t group_z,
                              const void* push_constants, size_t push_size) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(ctx->device, &alloc_info, &cmd);
    
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cmd, &begin);
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, NULL);
    if (push_constants && push_size > 0) {
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, push_size, push_constants);
    }
    vkCmdDispatch(cmd, group_x, group_y, group_z);
    
    vkEndCommandBuffer(cmd);
    
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
    vkQueueSubmit(ctx->compute_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->compute_queue);
    
    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &cmd);
}

/* ==================================================================
 * Compute Shader Operations - Stubs (implemented via shaders)
 * ================================================================== */

/* MatMul: C = A @ B  (A: MxK, B: KxN, C: MxN) */
void bear_vulkan_matmul(BearVulkanContext* ctx,
                         const BearVulkanTensor* A, const BearVulkanTensor* B, BearVulkanTensor* C,
                         int M, int K, int N,
                         BearVulkanArena* temp_arena) {
    struct BearVulkanPipeline* p = get_or_create_pipeline(ctx, "matmul", "bear_vulkan_shaders/matmul.comp.spv");
    if (!p) return;
    
    VkDescriptorSet set = allocate_descriptor_set(ctx);
    update_descriptor_set_storage(ctx, set, 0, A->buffer, A->offset, A->size);
    update_descriptor_set_storage(ctx, set, 1, B->buffer, B->offset, B->size);
    update_descriptor_set_storage(ctx, set, 2, C->buffer, C->offset, C->size);
    
    struct { uint32_t M, K, N; } push = { M, K, N };
    
    uint32_t group_x = (N + 15) / 16;
    uint32_t group_y = (M + 15) / 16;
    dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, group_y, 1, &push, sizeof(push));
}

/* Add: C = A + B (element-wise) */
void bear_vulkan_add(BearVulkanContext* ctx,
                      const BearVulkanTensor* A, const BearVulkanTensor* B, BearVulkanTensor* C,
                      int numel) {
    struct BearVulkanPipeline* p = get_or_create_pipeline(ctx, "add", "bear_vulkan_shaders/add.comp.spv");
    if (!p) return;
    
    VkDescriptorSet set = allocate_descriptor_set(ctx);
    update_descriptor_set_storage(ctx, set, 0, A->buffer, A->offset, A->size);
    update_descriptor_set_storage(ctx, set, 1, B->buffer, B->offset, B->size);
    update_descriptor_set_storage(ctx, set, 2, C->buffer, C->offset, C->size);
    
    struct { uint32_t total; } push = { numel };
    
    uint32_t group_x = (numel + 255) / 256;
    dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));
}

/* ReLU: out = max(0, in) */
void bear_vulkan_relu(BearVulkanContext* ctx,
                       const BearVulkanTensor* input, BearVulkanTensor* output,
                       int numel) {
    struct BearVulkanPipeline* p = get_or_create_pipeline(ctx, "relu", "bear_vulkan_shaders/relu.comp.spv");
    if (!p) return;
    
    VkDescriptorSet set = allocate_descriptor_set(ctx);
    update_descriptor_set_storage(ctx, set, 0, input->buffer, input->offset, input->size);
    update_descriptor_set_storage(ctx, set, 1, output->buffer, output->offset, output->size);
    
    struct { uint32_t size; } push = { numel };
    
    uint32_t group_x = (numel + 255) / 256;
    dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));
}

/* Softmax */
void bear_vulkan_softmax(BearVulkanContext* ctx,
                          const BearVulkanTensor* input, BearVulkanTensor* output,
                          int size) {
    struct BearVulkanPipeline* p = get_or_create_pipeline(ctx, "softmax", "bear_vulkan_shaders/softmax.comp.spv");
    if (!p) return;
    
    VkDescriptorSet set = allocate_descriptor_set(ctx);
    update_descriptor_set_storage(ctx, set, 0, input->buffer, input->offset, input->size);
    update_descriptor_set_storage(ctx, set, 1, output->buffer, output->offset, output->size);
    
    struct { uint32_t size; } push = { size };
    
    uint32_t group_x = 1;
    dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));
}

/* Sigmoid */
void bear_vulkan_sigmoid(BearVulkanContext* ctx,
                          const BearVulkanTensor* input, BearVulkanTensor* output,
                          int numel) {
    struct BearVulkanPipeline* p = get_or_create_pipeline(ctx, "sigmoid", "bear_vulkan_shaders/sigmoid.comp.spv");
    if (!p) return;
    
    VkDescriptorSet set = allocate_descriptor_set(ctx);
    update_descriptor_set_storage(ctx, set, 0, input->buffer, input->offset, input->size);
    update_descriptor_set_storage(ctx, set, 1, output->buffer, output->offset, output->size);
    
    struct { uint32_t size; } push = { numel };
    
    uint32_t group_x = (numel + 255) / 256;
    dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));
}

/* ==================================================================
 * High-level Operations (Forward/Backward/Env) - Stubs
 * ================================================================== */

/* These are stubs - full implementation requires composing the above ops */

void bear_policy_forward_vulkan(BearVulkanContext* ctx,
                                const BearPolicyNet* net,
                                const BearVulkanTensor* obs,
                                const BearVulkanTensor* h_in,
                                BearVulkanTensor* actions,
                                BearVulkanTensor* logprobs,
                                BearVulkanTensor* values,
                                BearVulkanTensor* h_out,
                                BearVulkanArena* temp_arena) {
    (void)ctx; (void)net; (void)obs; (void)h_in; (void)actions; (void)logprobs; (void)values; (void)h_out; (void)temp_arena;
    /* TODO: Implement full forward pass using matmul, add, relu, softmax kernels */
}

/* ... other stubs for backward, value, env, etc. */

int bear_policy_backward_discrete_vulkan(BearVulkanContext* ctx, BearPolicyNet* net,
                                          const BearVulkanTensor* obs, const BearVulkanTensor* actions,
                                          const BearVulkanTensor* old_logprobs, const BearVulkanTensor* advantages,
                                          float clip_coef, float policy_grad_scale, BearVulkanArena* temp_arena) {
    (void)ctx; (void)net; (void)obs; (void)actions; (void)old_logprobs; (void)advantages; (void)clip_coef; (void)policy_grad_scale; (void)temp_arena;
    return 0;
}

/* ... remaining stubs ... */
