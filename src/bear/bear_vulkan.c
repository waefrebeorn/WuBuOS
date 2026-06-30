/*
 * bear_vulkan.c  --  BearRL Vulkan Host Context Management (Pure C11)
 *
 * Vulkan instance/device initialization, memory management, pipeline caching.
 * Compute shaders live in bear_vulkan_shaders/ as .comp files.
 */

#include "bear_vulkan.h"
#include "bear_arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAS_VULKAN
#include <vulkan/vulkan.h>
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
 * High-level Operations (Forward/Backward/Env) - Implemented via pipelines
 * ================================================================== */

/* These are implemented via compute pipelines below */

/* ==================================================================
 * Compute Pipeline: Policy Forward
 * ================================================================== */

struct BearVulkanPipeline* get_or_create_policy_forward_pipeline(BearVulkanContext* ctx) {
    const char* name = "policy_forward";
    for (int i = 0; i < ctx->num_pipelines; ++i) {
        if (strcmp(ctx->pipelines[i].name, name) == 0) {
            return &ctx->pipelines[i];
        }
    }
    
    if (ctx->num_pipelines >= 64) return NULL;
    
    struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
    strncpy(p->name, name, 63);
    p->active = 0;
    
    /* Create pipeline layout */
    VkPipelineLayoutCreateInfo playout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->storage_desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &(VkPushConstantRange){
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = 16  // batch, obs_dim, hidden_dim, act_dim
        }
    };
    VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));
    
    /* Load shader */
    VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_policy_forward.comp.spv");
    if (shader == VK_NULL_HANDLE) {
        return NULL;
    }
    
    VkComputePipelineCreateInfo pipe_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main"
        },
        .layout = p->layout
    };
    VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));
    
    vkDestroyShaderModule(ctx->device, shader, NULL);
    p->active = 1;
    return p;
}

void bear_policy_forward_vulkan(BearVulkanContext* ctx,
                                const BearPolicyNet* net,
                                const BearVulkanTensor* obs,
                                const BearVulkanTensor* h_in,
                                BearVulkanTensor* actions,
                                BearVulkanTensor* logprobs,
                                BearVulkanTensor* values,
                                BearVulkanTensor* h_out,
                                BearVulkanArena* temp_arena) {
    struct BearVulkanPipeline* p = get_or_create_policy_forward_pipeline(ctx);
    if (!p) return;
    
    if (!net || !net->layers || net->num_layers < 2) return;
    
    // Get weight tensors from the network
    // Layer 0: input -> hidden (W1, b1)
    // Layer 1: hidden -> output (W2, b2) - includes actor, value, logstd
    BearLayer* layer1 = &net->layers[0];
    BearLayer* layer2 = &net->layers[1];
    
    // We need to upload weights to GPU if not already there
    // For now, create GPU buffers and upload
    // TODO: Cache these buffers in the network struct
    
    // Create and upload W1 [hidden_dim, obs_dim]
    BearVulkanTensor gpu_W1, gpu_b1, gpu_W2, gpu_b2;
    int64_t W1_shape[2] = { layer1->out_features, layer1->in_features };
    int64_t b1_shape[1] = { layer1->out_features };
    int64_t W2_shape[2] = { layer2->out_features, layer2->in_features };
    int64_t b2_shape[1] = { layer2->out_features };
    
    // Create CPU tensors from the network parameters
    BearTensor cpu_W1 = { .data = layer1->param->weight.data, .shape = W1_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
    BearTensor cpu_b1 = { .data = layer1->param->bias.data, .shape = b1_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
    BearTensor cpu_W2 = { .data = layer2->param->weight.data, .shape = W2_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
    BearTensor cpu_b2 = { .data = layer2->param->bias.data, .shape = b2_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
    
    // Upload to GPU (using temp_arena for allocation)
    bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W1, &gpu_W1);
    bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b1, &gpu_b1);
    bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W2, &gpu_W2);
    bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b2, &gpu_b2);
    
    VkDescriptorSet set = allocate_descriptor_set(ctx);
    
    /* Bindings: obs, h_in, W1, b1, W2, b2, actions, logprobs, values, h_out */
    update_descriptor_set_storage(ctx, set, 0, obs->buffer, obs->offset, obs->size);
    update_descriptor_set_storage(ctx, set, 1, h_in->buffer, h_in->offset, h_in->size);
    update_descriptor_set_storage(ctx, set, 2, gpu_W1.buffer, gpu_W1.offset, gpu_W1.size);
    update_descriptor_set_storage(ctx, set, 3, gpu_b1.buffer, gpu_b1.offset, gpu_b1.size);
    update_descriptor_set_storage(ctx, set, 4, gpu_W2.buffer, gpu_W2.offset, gpu_W2.size);
    update_descriptor_set_storage(ctx, set, 5, gpu_b2.buffer, gpu_b2.offset, gpu_b2.size);
    update_descriptor_set_storage(ctx, set, 6, actions->buffer, actions->offset, actions->size);
    update_descriptor_set_storage(ctx, set, 7, logprobs->buffer, logprobs->offset, logprobs->size);
    update_descriptor_set_storage(ctx, set, 8, values->buffer, values->offset, values->size);
    update_descriptor_set_storage(ctx, set, 9, h_out->buffer, h_out->offset, h_out->size);
    
    struct {
        uint32_t batch;
        uint32_t obs_dim;
        uint32_t hidden_dim;
        uint32_t act_dim;
    } push = {
        .batch = obs->shape[0],
        .obs_dim = obs->shape[1],
        .hidden_dim = h_out->shape[1],
        .act_dim = actions->shape[1]
    };
    
    uint32_t group_x = (push.batch + 255) / 256;
    dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));
}

/* ==================================================================
 * Compute Pipeline: GAE Advantage Computation
 * ================================================================== */

struct BearVulkanPipeline* get_or_create_gae_pipeline(BearVulkanContext* ctx) {
    const char* name = "gae";
    for (int i = 0; i < ctx->num_pipelines; ++i) {
        if (strcmp(ctx->pipelines[i].name, name) == 0) {
            return &ctx->pipelines[i];
        }
    }
    
    if (ctx->num_pipelines >= 64) return NULL;
    
    struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
    strncpy(p->name, name, 63);
    p->active = 0;
    
    VkPipelineLayoutCreateInfo playout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->storage_desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &(VkPushConstantRange){
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = 16  // T, B, gamma, gae_lambda
        }
    };
    VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));
    
    VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_gae.comp.spv");
    if (shader == VK_NULL_HANDLE) return NULL;
    
    VkComputePipelineCreateInfo pipe_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main"
        },
        .layout = p->layout
    };
    VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));
    
    vkDestroyShaderModule(ctx->device, shader, NULL);
    p->active = 1;
    return p;
}

void bear_compute_advantages_vulkan(BearVulkanContext* ctx,
                                    BearTrajectory* t,
                                    const BearPPOConfig* cfg,
                                    BearVulkanArena* temp_arena) {
    struct BearVulkanPipeline* p = get_or_create_gae_pipeline(ctx);
    if (!p) return;
    
    int T = t->rollout_len;
    int B = t->num_envs * t->max_agents;
    
    // Create GPU tensors from trajectory data
    BearVulkanTensor gpu_rewards, gpu_dones, gpu_values, gpu_advantages, gpu_returns;
    
    // Upload rewards [T, B]
    int64_t shape_2d[2] = { T, B };
    bear_vulkan_tensor_create(ctx, temp_arena, shape_2d, 2, 0, &gpu_rewards, "gae_rewards");
    bear_vulkan_tensor_from_host(ctx, temp_arena, &t->rewards, &gpu_rewards);
    
    // Upload dones [T, B] (uint8)
    bear_vulkan_tensor_create(ctx, temp_arena, shape_2d, 2, 0, &gpu_dones, "gae_dones");
    bear_vulkan_tensor_from_host(ctx, temp_arena, &t->dones, &gpu_dones);
    
    // Upload values [T, B]
    bear_vulkan_tensor_create(ctx, temp_arena, shape_2d, 2, 0, &gpu_values, "gae_values");
    bear_vulkan_tensor_from_host(ctx, temp_arena, &t->values, &gpu_values);
    
    // Create output tensors (advantages, returns)
    bear_vulkan_tensor_create(ctx, temp_arena, shape_2d, 2, 0, &gpu_advantages, "gae_advantages");
    bear_vulkan_tensor_create(ctx, temp_arena, shape_2d, 2, 0, &gpu_returns, "gae_returns");
    
    // Bind descriptor sets
    VkDescriptorSet set = allocate_descriptor_set(ctx);
    update_descriptor_set_storage(ctx, set, 0, gpu_rewards.buffer, gpu_rewards.offset, gpu_rewards.size);
    update_descriptor_set_storage(ctx, set, 1, gpu_dones.buffer, gpu_dones.offset, gpu_dones.size);
    update_descriptor_set_storage(ctx, set, 2, gpu_values.buffer, gpu_values.offset, gpu_values.size);
    update_descriptor_set_storage(ctx, set, 3, gpu_advantages.buffer, gpu_advantages.offset, gpu_advantages.size);
    update_descriptor_set_storage(ctx, set, 4, gpu_returns.buffer, gpu_returns.offset, gpu_returns.size);
    
    // Push constants: T, B, gamma, gae_lambda
    struct { uint32_t T, B; float gamma, gae_lambda; } push = {
        .T = (uint32_t)T,
        .B = (uint32_t)B,
        .gamma = cfg->gamma,
        .gae_lambda = cfg->gae_lambda
    };
    
    // Dispatch: one workgroup per environment (B workgroups)
    uint32_t group_x = (B + 255) / 256;
    dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));
    
    // Download results back to host trajectory
    bear_vulkan_tensor_to_host(ctx, &gpu_advantages, &t->advantages);
    bear_vulkan_tensor_to_host(ctx, &gpu_returns, &t->returns);
    
    // Free GPU tensors
    bear_vulkan_tensor_free(ctx, temp_arena, &gpu_rewards);
    bear_vulkan_tensor_free(ctx, temp_arena, &gpu_dones);
    bear_vulkan_tensor_free(ctx, temp_arena, &gpu_values);
    bear_vulkan_tensor_free(ctx, temp_arena, &gpu_advantages);
    bear_vulkan_tensor_free(ctx, temp_arena, &gpu_returns);
}

/* ==================================================================
 * Compute Pipeline: N-pole CartPole Step
 * ================================================================== */

struct BearVulkanPipeline* get_or_create_npole_step_pipeline(BearVulkanContext* ctx) {
    const char* name = "npole_step";
    for (int i = 0; i < ctx->num_pipelines; ++i) {
        if (strcmp(ctx->pipelines[i].name, name) == 0) {
            return &ctx->pipelines[i];
        }
    }
    
    if (ctx->num_pipelines >= 64) return NULL;
    
    struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
    strncpy(p->name, name, 63);
    p->active = 0;
    
    VkPipelineLayoutCreateInfo playout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->storage_desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &(VkPushConstantRange){
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = 80  // n_poles, num_envs, gravity, cart_mass, dt, force_mag, thresholds, pole arrays
        }
    };
    VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));
    
    VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_npole_step.comp.spv");
    if (shader == VK_NULL_HANDLE) return NULL;
    
    VkComputePipelineCreateInfo pipe_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main"
        },
        .layout = p->layout
    };
    VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));
    
    vkDestroyShaderModule(ctx->device, shader, NULL);
    p->active = 1;
    return p;
}

void bear_vulkan_env_step(BearVulkanContext* ctx,
                          BearVulkanEnvState* env,
                          const BearVulkanTensor* actions,
                          uint64_t rng_seed) {
    struct BearVulkanPipeline* p = get_or_create_npole_step_pipeline(ctx);
    if (!p) return;
    
    // Bind all env state buffers + actions
    // The env state has: x, x_dot, theta, theta_dot, force, reward, done
    // Plus pole params: pole_mass, pole_length, pole_com, pole_inertia
    // Push constants: n_poles, num_envs, gravity, cart_mass, dt, force_mag, angle_threshold, pos_threshold
    
    VkDescriptorSet set = allocate_descriptor_set(ctx);
    
    // Bindings according to shader:
    // binding 0: x_buffer [num_envs] (read/write)
    // binding 1: x_dot_buffer [num_envs] (read/write)
    // binding 2: theta_buffer [num_envs, n_poles] (read/write)
    // binding 3: theta_dot_buffer [num_envs, n_poles] (read/write)
    // binding 4: force_buffer [num_envs] (read) - we'll use actions for this
    // binding 5: reward_buffer [num_envs] (output)
    // binding 6: done_buffer [num_envs] (output, uint)
    // binding 7: pole_mass [n_poles] (read)
    // binding 8: pole_length [n_poles] (read)
    // binding 9: pole_com [n_poles] (read)
    // binding 10: pole_inertia [n_poles] (read)
    // binding 11: force_ary [num_envs] (read) - additional force array
    
    // The shader expects the env state to have VkBuffer for each field
    // For now, we need the actions tensor to provide the force
    // The env struct uses VkBuffer directly, not BearVulkanTensor
    
    // Since the BearVulkanEnvState uses VkBuffer directly, we need to 
    // create temporary BearVulkanTensor wrappers or bind directly
    // The update_descriptor_set_storage function takes VkBuffer
    
    // Upload actions to force buffer if needed
    // For now, assume the env already has the force buffer populated
    
    update_descriptor_set_storage(ctx, set, 0, env->x, 0, env->num_envs * sizeof(float));
    update_descriptor_set_storage(ctx, set, 1, env->x_dot, 0, env->num_envs * sizeof(float));
    update_descriptor_set_storage(ctx, set, 2, env->theta, 0, env->num_envs * env->n_poles * sizeof(float));
    update_descriptor_set_storage(ctx, set, 3, env->theta_dot, 0, env->num_envs * env->n_poles * sizeof(float));
    update_descriptor_set_storage(ctx, set, 4, env->force, 0, env->num_envs * sizeof(float));
    update_descriptor_set_storage(ctx, set, 5, env->reward, 0, env->num_envs * sizeof(float));
    update_descriptor_set_storage(ctx, set, 6, env->done, 0, env->num_envs * sizeof(uint32_t));
    update_descriptor_set_storage(ctx, set, 7, env->pole_mass, 0, env->n_poles * sizeof(float));
    update_descriptor_set_storage(ctx, set, 8, env->pole_length, 0, env->n_poles * sizeof(float));
    update_descriptor_set_storage(ctx, set, 9, env->pole_com, 0, env->n_poles * sizeof(float));
    update_descriptor_set_storage(ctx, set, 10, env->pole_inertia, 0, env->n_poles * sizeof(float));
    // binding 11: force_ary - could be same as force or separate
    update_descriptor_set_storage(ctx, set, 11, env->force, 0, env->num_envs * sizeof(float));
    
    // Push constants
    struct {
        uint32_t n_poles;
        uint32_t num_envs;
        float gravity;
        float cart_mass;
        float dt;
        float force_mag;
        float angle_threshold;
        float pos_threshold;
    } push = {
        .n_poles = (uint32_t)env->n_poles,
        .num_envs = (uint32_t)env->num_envs,
        .gravity = env->gravity,
        .cart_mass = env->cart_mass,
        .dt = env->dt,
        .force_mag = env->force_mag,
        .angle_threshold = env->angle_threshold,
        .pos_threshold = env->pos_threshold
    };
    
    // Dispatch: one workgroup per env (or multiple for small num_envs)
    uint32_t group_x = (env->num_envs + 255) / 256;
    dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));
}

/* ==================================================================
 * Compute Pipeline: MMA FP16 Matrix Multiply
 * ================================================================== */

struct BearVulkanPipeline* get_or_create_mma_matmul_pipeline(BearVulkanContext* ctx) {
    const char* name = "mma_matmul";
    for (int i = 0; i < ctx->num_pipelines; ++i) {
        if (strcmp(ctx->pipelines[i].name, name) == 0) {
            return &ctx->pipelines[i];
        }
    }
    
    if (ctx->num_pipelines >= 64) return NULL;
    
    struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
    strncpy(p->name, name, 63);
    p->active = 0;
    
    VkPipelineLayoutCreateInfo playout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->storage_desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &(VkPushConstantRange){
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = 12  // M, N, K
        }
    };
    VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));
    
    VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_mma_matmul.comp.spv");
    if (shader == VK_NULL_HANDLE) return NULL;
    
    VkComputePipelineCreateInfo pipe_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main"
        },
        .layout = p->layout
    };
    VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));
    
    vkDestroyShaderModule(ctx->device, shader, NULL);
    p->active = 1;
    return p;
}

void bear_mma_matmul_vulkan(BearVulkanContext* ctx,
                            const BearVulkanTensor* A,
                            const BearVulkanTensor* B,
                            BearVulkanTensor* C) {
    struct BearVulkanPipeline* p = get_or_create_mma_matmul_pipeline(ctx);
    if (!p) return;
    
    VkDescriptorSet set = allocate_descriptor_set(ctx);
    update_descriptor_set_storage(ctx, set, 0, A->buffer, A->offset, A->size);
    update_descriptor_set_storage(ctx, set, 1, B->buffer, B->offset, B->size);
    struct { uint32_t M, N, K; } push = {
            .M = A->shape[0],
            .N = B->shape[1],
            .K = A->shape[1]
        };

        uint32_t group_x = (push.N + 7) / 8;
        uint32_t group_y = (push.M + 15) / 16;
        dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, group_y, 1, &push, sizeof(push));
    }


    /* ==================================================================
     * Compute Pipeline: Policy Backward Discrete
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_policy_backward_discrete_pipeline(BearVulkanContext* ctx) {
        const char* name = "policy_backward_discrete";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 40  // batch, obs_dim, hidden_dim, act_dim, out_features, clip_coef, policy_grad_scale
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_policy_backward_discrete.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    int bear_policy_backward_discrete_vulkan(BearVulkanContext* ctx, BearPolicyNet* net,
                                              const BearVulkanTensor* obs, const BearVulkanTensor* actions,
                                              const BearVulkanTensor* old_logprobs, const BearVulkanTensor* advantages,
                                              float clip_coef, float policy_grad_scale, BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_policy_backward_discrete_pipeline(ctx);
        if (!p) return -1;

        if (!net || !net->layers || net->num_layers < 2) return -1;

        BearLayer* layer1 = &net->layers[0];
        BearLayer* layer2 = &net->layers[1];

        // Upload weights to GPU
        BearVulkanTensor gpu_W1, gpu_b1, gpu_W2, gpu_b2;
        int64_t W1_shape[2] = { layer1->out_features, layer1->in_features };
        int64_t b1_shape[1] = { layer1->out_features };
        int64_t W2_shape[2] = { layer2->out_features, layer2->in_features };
        int64_t b2_shape[1] = { layer2->out_features };

        BearTensor cpu_W1 = { .data = layer1->param->weight.data, .shape = W1_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_b1 = { .data = layer1->param->bias.data, .shape = b1_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_W2 = { .data = layer2->param->weight.data, .shape = W2_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_b2 = { .data = layer2->param->bias.data, .shape = b2_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };

        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W1, &gpu_W1);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b1, &gpu_b1);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W2, &gpu_W2);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b2, &gpu_b2);

        // Create gradient buffers (zero-initialized)
        BearVulkanTensor grad_W1, grad_b1, grad_W2, grad_b2;
        bear_vulkan_tensor_create(ctx, temp_arena, W1_shape, 2, BEAR_DTYPE_F32, &grad_W1, "grad_W1");
        bear_vulkan_tensor_create(ctx, temp_arena, b1_shape, 1, BEAR_DTYPE_F32, &grad_b1, "grad_b1");
        bear_vulkan_tensor_create(ctx, temp_arena, W2_shape, 2, BEAR_DTYPE_F32, &grad_W2, "grad_W2");
        bear_vulkan_tensor_create(ctx, temp_arena, b2_shape, 1, BEAR_DTYPE_F32, &grad_b2, "grad_b2");

        // Zero initialize gradient buffers
        void* mapped;
        bear_vulkan_tensor_map(ctx, &grad_W1);
        memset(grad_W1.mapped_ptr, 0, grad_W1.size);
        bear_vulkan_tensor_unmap(ctx, &grad_W1);
        bear_vulkan_tensor_map(ctx, &grad_b1);
        memset(grad_b1.mapped_ptr, 0, grad_b1.size);
        bear_vulkan_tensor_unmap(ctx, &grad_b1);
        bear_vulkan_tensor_map(ctx, &grad_W2);
        memset(grad_W2.mapped_ptr, 0, grad_W2.size);
        bear_vulkan_tensor_unmap(ctx, &grad_W2);
        bear_vulkan_tensor_map(ctx, &grad_b2);
        memset(grad_b2.mapped_ptr, 0, grad_b2.size);
        bear_vulkan_tensor_unmap(ctx, &grad_b2);

        VkDescriptorSet set = allocate_descriptor_set(ctx);

        /* Bindings:
         *   0: obs
         *   1: actions
         *   2: old_logprobs
         *   3: advantages
         *   4: W1
         *   5: b1
         *   6: W2
         *   7: b2
         *   8: grad_W1
         *   9: grad_b1
         *   10: grad_W2
         *   11: grad_b2
         */
        update_descriptor_set_storage(ctx, set, 0, obs->buffer, obs->offset, obs->size);
        update_descriptor_set_storage(ctx, set, 1, actions->buffer, actions->offset, actions->size);
        update_descriptor_set_storage(ctx, set, 2, old_logprobs->buffer, old_logprobs->offset, old_logprobs->size);
        update_descriptor_set_storage(ctx, set, 3, advantages->buffer, advantages->offset, advantages->size);
        update_descriptor_set_storage(ctx, set, 4, gpu_W1.buffer, gpu_W1.offset, gpu_W1.size);
        update_descriptor_set_storage(ctx, set, 5, gpu_b1.buffer, gpu_b1.offset, gpu_b1.size);
        update_descriptor_set_storage(ctx, set, 6, gpu_W2.buffer, gpu_W2.offset, gpu_W2.size);
        update_descriptor_set_storage(ctx, set, 7, gpu_b2.buffer, gpu_b2.offset, gpu_b2.size);
        update_descriptor_set_storage(ctx, set, 8, grad_W1.buffer, grad_W1.offset, grad_W1.size);
        update_descriptor_set_storage(ctx, set, 9, grad_b1.buffer, grad_b1.offset, grad_b1.size);
        update_descriptor_set_storage(ctx, set, 10, grad_W2.buffer, grad_W2.offset, grad_W2.size);
        update_descriptor_set_storage(ctx, set, 11, grad_b2.buffer, grad_b2.offset, grad_b2.size);

        struct {
            uint32_t batch;
            uint32_t obs_dim;
            uint32_t hidden_dim;
            uint32_t act_dim;
            uint32_t out_features;
            float clip_coef;
            float policy_grad_scale;
        } push = {
            .batch = obs->shape[0],
            .obs_dim = obs->shape[1],
            .hidden_dim = layer1->out_features,
            .act_dim = net->act_dim,
            .out_features = layer2->out_features,
            .clip_coef = clip_coef,
            .policy_grad_scale = policy_grad_scale
        };

        // Dispatch: one workgroup per batch element
        uint32_t group_x = push.batch;
        dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));

        // Copy gradients back to host network
        BearTensor host_grad_W1 = { .data = layer1->param->grad.data, .shape = W1_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        bear_vulkan_tensor_to_host(ctx, &grad_W1, &host_grad_W1);

        // Free GPU tensors
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_W1);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_b1);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_W2);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_b2);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_W1);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_b1);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_W2);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_b2);

        return 0;
    }


    /* ==================================================================
     * Compute Pipeline: Policy Backward Continuous
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_policy_backward_continuous_pipeline(BearVulkanContext* ctx) {
        const char* name = "policy_backward_continuous";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 44  // batch, obs_dim, hidden_dim, act_dim, out_features, clip_coef, policy_grad_scale, logstd
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_policy_backward_continuous.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    int bear_policy_backward_continuous_vulkan(BearVulkanContext* ctx, BearPolicyNet* net,
                                                const BearVulkanTensor* obs, const BearVulkanTensor* actions,
                                                const BearVulkanTensor* old_logprobs, const BearVulkanTensor* advantages,
                                                float clip_coef, float policy_grad_scale, BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_policy_backward_continuous_pipeline(ctx);
        if (!p) return -1;

        if (!net || !net->layers || net->num_layers < 2) return -1;

        BearLayer* layer1 = &net->layers[0];
        BearLayer* layer2 = &net->layers[1];

        // Upload weights to GPU
        BearVulkanTensor gpu_W1, gpu_b1, gpu_W2, gpu_b2;
        int64_t W1_shape[2] = { layer1->out_features, layer1->in_features };
        int64_t b1_shape[1] = { layer1->out_features };
        int64_t W2_shape[2] = { layer2->out_features, layer2->in_features };
        int64_t b2_shape[1] = { layer2->out_features };

        BearTensor cpu_W1 = { .data = layer1->param->weight.data, .shape = W1_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_b1 = { .data = layer1->param->bias.data, .shape = b1_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_W2 = { .data = layer2->param->weight.data, .shape = W2_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_b2 = { .data = layer2->param->bias.data, .shape = b2_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };

        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W1, &gpu_W1);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b1, &gpu_b1);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W2, &gpu_W2);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b2, &gpu_b2);

        // Create gradient buffers
        BearVulkanTensor grad_W1, grad_b1, grad_W2, grad_b2;
        bear_vulkan_tensor_create(ctx, temp_arena, W1_shape, 2, BEAR_DTYPE_F32, &grad_W1, "grad_W1");
        bear_vulkan_tensor_create(ctx, temp_arena, b1_shape, 1, BEAR_DTYPE_F32, &grad_b1, "grad_b1");
        bear_vulkan_tensor_create(ctx, temp_arena, W2_shape, 2, BEAR_DTYPE_F32, &grad_W2, "grad_W2");
        bear_vulkan_tensor_create(ctx, temp_arena, b2_shape, 1, BEAR_DTYPE_F32, &grad_b2, "grad_b2");

        // Zero initialize
        void* mapped;
        bear_vulkan_tensor_map(ctx, &grad_W1);
        memset(grad_W1.mapped_ptr, 0, grad_W1.size);
        bear_vulkan_tensor_unmap(ctx, &grad_W1);
        bear_vulkan_tensor_map(ctx, &grad_b1);
        memset(grad_b1.mapped_ptr, 0, grad_b1.size);
        bear_vulkan_tensor_unmap(ctx, &grad_b1);
        bear_vulkan_tensor_map(ctx, &grad_W2);
        memset(grad_W2.mapped_ptr, 0, grad_W2.size);
        bear_vulkan_tensor_unmap(ctx, &grad_W2);
        bear_vulkan_tensor_map(ctx, &grad_b2);
        memset(grad_b2.mapped_ptr, 0, grad_b2.size);
        bear_vulkan_tensor_unmap(ctx, &grad_b2);

        VkDescriptorSet set = allocate_descriptor_set(ctx);

        /* Bindings: same as discrete */
        update_descriptor_set_storage(ctx, set, 0, obs->buffer, obs->offset, obs->size);
        update_descriptor_set_storage(ctx, set, 1, actions->buffer, actions->offset, actions->size);
        update_descriptor_set_storage(ctx, set, 2, old_logprobs->buffer, old_logprobs->offset, old_logprobs->size);
        update_descriptor_set_storage(ctx, set, 3, advantages->buffer, advantages->offset, advantages->size);
        update_descriptor_set_storage(ctx, set, 4, gpu_W1.buffer, gpu_W1.offset, gpu_W1.size);
        update_descriptor_set_storage(ctx, set, 5, gpu_b1.buffer, gpu_b1.offset, gpu_b1.size);
        update_descriptor_set_storage(ctx, set, 6, gpu_W2.buffer, gpu_W2.offset, gpu_W2.size);
        update_descriptor_set_storage(ctx, set, 7, gpu_b2.buffer, gpu_b2.offset, gpu_b2.size);
        update_descriptor_set_storage(ctx, set, 8, grad_W1.buffer, grad_W1.offset, grad_W1.size);
        update_descriptor_set_storage(ctx, set, 9, grad_b1.buffer, grad_b1.offset, grad_b1.size);
        update_descriptor_set_storage(ctx, set, 10, grad_W2.buffer, grad_W2.offset, grad_W2.size);
        update_descriptor_set_storage(ctx, set, 11, grad_b2.buffer, grad_b2.offset, grad_b2.size);

        float logstd = net->logstd ? 0.0f : net->logstd_fixed;

        struct {
            uint32_t batch;
            uint32_t obs_dim;
            uint32_t hidden_dim;
            uint32_t act_dim;
            uint32_t out_features;
            float clip_coef;
            float policy_grad_scale;
            float logstd;
        } push = {
            .batch = obs->shape[0],
            .obs_dim = obs->shape[1],
            .hidden_dim = layer1->out_features,
            .act_dim = net->act_dim,
            .out_features = layer2->out_features,
            .clip_coef = clip_coef,
            .policy_grad_scale = policy_grad_scale,
            .logstd = logstd
        };

        // Dispatch: one workgroup per batch element
        uint32_t group_x = push.batch;
        dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));

        // Copy gradients back
        BearTensor host_grad_W1 = { .data = layer1->param->grad.data, .shape = W1_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        bear_vulkan_tensor_to_host(ctx, &grad_W1, &host_grad_W1);

        // Free GPU tensors
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_W1);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_b1);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_W2);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_b2);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_W1);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_b1);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_W2);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_b2);

        return 0;
    }


    /* ==================================================================
     * Compute Pipeline: Value Forward
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_value_forward_pipeline(BearVulkanContext* ctx) {
        const char* name = "value_forward";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 16  // batch, obs_dim, hidden_dim, hidden2_dim
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_value_forward.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    void bear_value_forward_vulkan(BearVulkanContext* ctx,
                                    const BearValueNet* vnet,
                                    const BearVulkanTensor* obs,
                                    BearVulkanTensor* values,
                                    BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_value_forward_pipeline(ctx);
        if (!p) return;

        // For now, delegate to CPU
        (void)ctx; (void)vnet; (void)obs; (void)values; (void)temp_arena;
    }


    /* ==================================================================
     * Compute Pipeline: Value Backward
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_value_backward_pipeline(BearVulkanContext* ctx) {
        const char* name = "value_backward";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 20  // batch, obs_dim, hidden_dim, hidden2_dim, vf_coef
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_value_backward.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    int bear_value_backward_vulkan(BearVulkanContext* ctx,
                                    BearValueNet* vnet,
                                    const BearVulkanTensor* obs,
                                    const BearVulkanTensor* values,
                                    const BearVulkanTensor* targets,
                                    float vf_coef,
                                    BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_value_backward_pipeline(ctx);
        if (!p) return -1;

        // For now, delegate to CPU
        (void)ctx; (void)vnet; (void)obs; (void)values; (void)targets; (void)vf_coef; (void)temp_arena;
        return 0;
    }


    /* ==================================================================
     * Compute Pipeline: PPO Loss
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_ppo_loss_pipeline(BearVulkanContext* ctx) {
        const char* name = "ppo_loss";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 52  // all push constants
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_ppo_loss.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    BearPPOLoss bear_ppo_loss_vulkan(BearVulkanContext* ctx,
                                      const BearPolicyNet* policy,
                                      const BearValueNet* critic,
                                      const BearVulkanTensor* obs,
                                      const BearVulkanTensor* actions,
                                      const BearVulkanTensor* old_logprobs,
                                      const BearVulkanTensor* advantages,
                                      const BearVulkanTensor* returns,
                                      const BearVulkanTensor* old_values,
                                      const BearPPOConfig* cfg,
                                      BearVulkanArena* temp_arena) {
        BearPPOLoss loss = {0};

        struct BearVulkanPipeline* p = get_or_create_ppo_loss_pipeline(ctx);
        if (!p) return loss;

        // For now, delegate to CPU
        (void)ctx; (void)policy; (void)critic; (void)obs; (void)actions;
        (void)old_logprobs; (void)advantages; (void)returns; (void)old_values; (void)cfg; (void)temp_arena;

        return loss;
    }


    /* ==================================================================
     * Compute Pipeline: PPO Apply Gradients (Adam)
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_ppo_apply_gradients_pipeline(BearVulkanContext* ctx) {
        const char* name = "ppo_apply_gradients";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 28  // n, lr, beta1, beta2, eps, wd, step
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_ppo_apply_gradients.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    void bear_ppo_apply_gradients_vulkan(BearVulkanContext* ctx,
                                              BearPolicyNet* policy,
                                              BearValueNet* critic,
                                              BearOptimizer* opt_policy,
                                              BearOptimizer* opt_critic) {
            struct BearVulkanPipeline* p = get_or_create_ppo_apply_gradients_pipeline(ctx);
            if (!p) return;

            // For now, delegate to CPU
            (void)ctx; (void)policy; (void)critic; (void)opt_policy; (void)opt_critic;
        }


    /* ==================================================================
     * Compute Pipeline: MinGRU Step
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_mingru_step_pipeline(BearVulkanContext* ctx) {
        const char* name = "mingru_step";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 8  // batch, hid
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_mingru_step.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    void bear_mingru_step_vulkan(BearVulkanContext* ctx,
                                  const BearMinGRU* gru,
                                  const BearVulkanTensor* x,
                                  const BearVulkanTensor* h_in,
                                  BearVulkanTensor* h_out,
                                  BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_mingru_step_pipeline(ctx);
        if (!p) return;

        // Upload weights
        BearVulkanTensor gpu_Wz, gpu_Uz, gpu_bz, gpu_Wr, gpu_Ur, gpu_br, gpu_Wn, gpu_Un, gpu_bn;
        int64_t Wz_shape[2] = { gru->hid_size, (int)gru->Wz.weight.shape[1] };
        int64_t Uz_shape[2] = { gru->hid_size, gru->hid_size };
        int64_t bz_shape[1] = { gru->hid_size };
        int64_t Wr_shape[2] = { gru->hid_size, (int)gru->Wr.weight.shape[1] };
        int64_t Ur_shape[2] = { gru->hid_size, gru->hid_size };
        int64_t br_shape[1] = { gru->hid_size };
        int64_t Wn_shape[2] = { gru->hid_size, (int)gru->Wn.weight.shape[1] };
        int64_t Un_shape[2] = { gru->hid_size, gru->hid_size };
        int64_t bn_shape[1] = { gru->hid_size };

        BearTensor cpu_Wz = { .data = gru->Wz.weight.data, .shape = Wz_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Uz = { .data = gru->Uz.weight.data, .shape = Uz_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_bz = { .data = gru->bz.bias.data, .shape = bz_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Wr = { .data = gru->Wr.weight.data, .shape = Wr_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Ur = { .data = gru->Ur.weight.data, .shape = Ur_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_br = { .data = gru->br.bias.data, .shape = br_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Wn = { .data = gru->Wn.weight.data, .shape = Wn_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Un = { .data = gru->Un.weight.data, .shape = Un_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_bn = { .data = gru->bn.bias.data, .shape = bn_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };

        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Wz, &gpu_Wz);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Uz, &gpu_Uz);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_bz, &gpu_bz);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Wr, &gpu_Wr);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Ur, &gpu_Ur);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_br, &gpu_br);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Wn, &gpu_Wn);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Un, &gpu_Un);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_bn, &gpu_bn);

        VkDescriptorSet set = allocate_descriptor_set(ctx);

        /* Bindings matching mingru_step.comp:
         *   0: x
         *   1: h_in
         *   2: Wz
         *   3: Uz
         *   4: bz
         *   5: Wr
         *   6: Ur
         *   7: br
         *   8: Wn
         *   9: Un
         *   10: bn
         *   11: h_out
         */
        update_descriptor_set_storage(ctx, set, 0, x->buffer, x->offset, x->size);
        update_descriptor_set_storage(ctx, set, 1, h_in->buffer, h_in->offset, h_in->size);
        update_descriptor_set_storage(ctx, set, 2, gpu_Wz.buffer, gpu_Wz.offset, gpu_Wz.size);
        update_descriptor_set_storage(ctx, set, 3, gpu_Uz.buffer, gpu_Uz.offset, gpu_Uz.size);
        update_descriptor_set_storage(ctx, set, 4, gpu_bz.buffer, gpu_bz.offset, gpu_bz.size);
        update_descriptor_set_storage(ctx, set, 5, gpu_Wr.buffer, gpu_Wr.offset, gpu_Wr.size);
        update_descriptor_set_storage(ctx, set, 6, gpu_Ur.buffer, gpu_Ur.offset, gpu_Ur.size);
        update_descriptor_set_storage(ctx, set, 7, gpu_br.buffer, gpu_br.offset, gpu_br.size);
        update_descriptor_set_storage(ctx, set, 8, gpu_Wn.buffer, gpu_Wn.offset, gpu_Wn.size);
        update_descriptor_set_storage(ctx, set, 9, gpu_Un.buffer, gpu_Un.offset, gpu_Un.size);
        update_descriptor_set_storage(ctx, set, 10, gpu_bn.buffer, gpu_bn.offset, gpu_bn.size);
        update_descriptor_set_storage(ctx, set, 11, h_out->buffer, h_out->offset, h_out->size);

        struct {
            uint32_t batch;
            uint32_t hid;
        } push = {
            .batch = x->shape[0],
            .hid = gru->hid_size
        };

        uint32_t group_x = (push.batch * push.hid + 255) / 256;
        dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));

        // Free GPU tensors
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Wz);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Uz);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_bz);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Wr);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Ur);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_br);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Wn);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Un);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_bn);
    }


    /* ... remaining stubs ... */


/* ===================================================================
 * Unified Backend Configuration and Auto-Dispatch
 * =================================================================== */

BearVulkanConfig bear_vulkan_config = {
    .use_vulkan = 1,
    .min_batch_for_vulkan = 256,
    .device_index = -1,
    .fallback_to_cpu = 1,
    .enable_validation = 0
};

static BearVulkanContext* g_vulkan_ctx = NULL;

int bear_backend_init_vulkan(const BearVulkanConfig* cfg) {
    if (cfg) {
        bear_vulkan_config = *cfg;
    }

    if (!bear_vulkan_config.use_vulkan) {
        return 0;  // CPU mode
    }

    BearVulkanStatus status = bear_vulkan_query();
    if (status != BEAR_VULKAN_AVAILABLE && status != BEAR_VULKAN_ACTIVE) {
        if (bear_vulkan_config.fallback_to_cpu) {
            bear_vulkan_config.use_vulkan = 0;
            return 0;
        }
        return -1;
    }

    g_vulkan_ctx = bear_vulkan_init(bear_vulkan_config.device_index);
    if (!g_vulkan_ctx) {
        if (bear_vulkan_config.fallback_to_cpu) {
            bear_vulkan_config.use_vulkan = 0;
            return 0;
        }
        return -1;
    }

    return 0;
}

void bear_backend_shutdown_vulkan(void) {
    if (g_vulkan_ctx) {
        bear_vulkan_destroy(g_vulkan_ctx);
        g_vulkan_ctx = NULL;
    }
}

void bear_policy_forward_unified_v(const BearPolicyNet* net,
                                    const BearTensor* obs,
                                    const BearTensor* h_in,
                                    BearTensor* actions,
                                    BearTensor* logprobs,
                                    BearTensor* values,
                                    BearTensor* h_out,
                                    BearArena* temp_arena) {
    if (bear_vulkan_config.use_vulkan && g_vulkan_ctx && obs->shape[0] >= bear_vulkan_config.min_batch_for_vulkan) {
        // Try Vulkan path
        // For now, fall back to CPU
    }
    // CPU fallback
    bear_policy_forward(net, obs, h_in, actions, logprobs, values, h_out, temp_arena);
}

int bear_policy_backward_unified_v(BearPolicyNet* net,
                                    const BearTensor* obs,
                                    const BearTensor* actions,
                                    const BearTensor* old_logprobs,
                                    const BearTensor* advantages,
                                    float clip_coef,
                                    float policy_grad_scale,
                                    BearArena* temp_arena) {
    if (bear_vulkan_config.use_vulkan && g_vulkan_ctx && obs->shape[0] >= bear_vulkan_config.min_batch_for_vulkan) {
        // Try Vulkan path
        // For now, fall back to CPU
    }
    // CPU fallback
    return bear_policy_backward(net, obs, actions, old_logprobs, advantages, clip_coef, policy_grad_scale, temp_arena);
}

void bear_value_forward_unified_v(const BearValueNet* vnet,
                                   const BearTensor* obs,
                                   BearTensor* values,
                                   BearArena* temp_arena) {
    if (bear_vulkan_config.use_vulkan && g_vulkan_ctx && obs->shape[0] >= bear_vulkan_config.min_batch_for_vulkan) {
        // Try Vulkan path
    }
    // CPU fallback
    bear_value_forward(vnet, obs, values, temp_arena);
}

int bear_value_backward_unified_v(BearValueNet* vnet,
                                   const BearTensor* obs,
                                   const BearTensor* values,
                                   const BearTensor* targets,
                                   float vf_coef,
                                   BearArena* temp_arena) {
    if (bear_vulkan_config.use_vulkan && g_vulkan_ctx && obs->shape[0] >= bear_vulkan_config.min_batch_for_vulkan) {
        // Try Vulkan path
    }
    // CPU fallback
    return bear_value_backward(vnet, obs, values, targets, vf_coef, temp_arena);
}

float bear_trainer_iter_unified_v(BearTrainer* trainer, uint64_t rng_state[2]) {
    // For now, just use CPU path
    return bear_trainer_iter(trainer, rng_state);
}s[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 40  // batch, obs_dim, hidden_dim, act_dim, out_features, clip_coef, policy_grad_scale
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_policy_backward_discrete.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    int bear_policy_backward_discrete_vulkan(BearVulkanContext* ctx, BearPolicyNet* net,
                                              const BearVulkanTensor* obs, const BearVulkanTensor* actions,
                                              const BearVulkanTensor* old_logprobs, const BearVulkanTensor* advantages,
                                              float clip_coef, float policy_grad_scale, BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_policy_backward_discrete_pipeline(ctx);
        if (!p) return -1;

        if (!net || !net->layers || net->num_layers < 2) return -1;

        BearLayer* layer1 = &net->layers[0];
        BearLayer* layer2 = &net->layers[1];

        // Upload weights to GPU
        BearVulkanTensor gpu_W1, gpu_b1, gpu_W2, gpu_b2;
        int64_t W1_shape[2] = { layer1->out_features, layer1->in_features };
        int64_t b1_shape[1] = { layer1->out_features };
        int64_t W2_shape[2] = { layer2->out_features, layer2->in_features };
        int64_t b2_shape[1] = { layer2->out_features };

        BearTensor cpu_W1 = { .data = layer1->param->weight.data, .shape = W1_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_b1 = { .data = layer1->param->bias.data, .shape = b1_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_W2 = { .data = layer2->param->weight.data, .shape = W2_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_b2 = { .data = layer2->param->bias.data, .shape = b2_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };

        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W1, &gpu_W1);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b1, &gpu_b1);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W2, &gpu_W2);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b2, &gpu_b2);

        // Create gradient buffers (zero-initialized)
        BearVulkanTensor grad_W1, grad_b1, grad_W2, grad_b2;
        bear_vulkan_tensor_create(ctx, temp_arena, W1_shape, 2, BEAR_DTYPE_F32, &grad_W1, "grad_W1");
        bear_vulkan_tensor_create(ctx, temp_arena, b1_shape, 1, BEAR_DTYPE_F32, &grad_b1, "grad_b1");
        bear_vulkan_tensor_create(ctx, temp_arena, W2_shape, 2, BEAR_DTYPE_F32, &grad_W2, "grad_W2");
        bear_vulkan_tensor_create(ctx, temp_arena, b2_shape, 1, BEAR_DTYPE_F32, &grad_b2, "grad_b2");

        // Zero initialize gradient buffers
        void* mapped;
        bear_vulkan_tensor_map(ctx, &grad_W1);
        memset(grad_W1.mapped_ptr, 0, grad_W1.size);
        bear_vulkan_tensor_unmap(ctx, &grad_W1);
        bear_vulkan_tensor_map(ctx, &grad_b1);
        memset(grad_b1.mapped_ptr, 0, grad_b1.size);
        bear_vulkan_tensor_unmap(ctx, &grad_b1);
        bear_vulkan_tensor_map(ctx, &grad_W2);
        memset(grad_W2.mapped_ptr, 0, grad_W2.size);
        bear_vulkan_tensor_unmap(ctx, &grad_W2);
        bear_vulkan_tensor_map(ctx, &grad_b2);
        memset(grad_b2.mapped_ptr, 0, grad_b2.size);
        bear_vulkan_tensor_unmap(ctx, &grad_b2);

        VkDescriptorSet set = allocate_descriptor_set(ctx);

        /* Bindings:
         *   0: obs
         *   1: actions
         *   2: old_logprobs
         *   3: advantages
         *   4: W1
         *   5: b1
         *   6: W2
         *   7: b2
         *   8: grad_W1
         *   9: grad_b1
         *   10: grad_W2
         *   11: grad_b2
         */
        update_descriptor_set_storage(ctx, set, 0, obs->buffer, obs->offset, obs->size);
        update_descriptor_set_storage(ctx, set, 1, actions->buffer, actions->offset, actions->size);
        update_descriptor_set_storage(ctx, set, 2, old_logprobs->buffer, old_logprobs->offset, old_logprobs->size);
        update_descriptor_set_storage(ctx, set, 3, advantages->buffer, advantages->offset, advantages->size);
        update_descriptor_set_storage(ctx, set, 4, gpu_W1.buffer, gpu_W1.offset, gpu_W1.size);
        update_descriptor_set_storage(ctx, set, 5, gpu_b1.buffer, gpu_b1.offset, gpu_b1.size);
        update_descriptor_set_storage(ctx, set, 6, gpu_W2.buffer, gpu_W2.offset, gpu_W2.size);
        update_descriptor_set_storage(ctx, set, 7, gpu_b2.buffer, gpu_b2.offset, gpu_b2.size);
        update_descriptor_set_storage(ctx, set, 8, grad_W1.buffer, grad_W1.offset, grad_W1.size);
        update_descriptor_set_storage(ctx, set, 9, grad_b1.buffer, grad_b1.offset, grad_b1.size);
        update_descriptor_set_storage(ctx, set, 10, grad_W2.buffer, grad_W2.offset, grad_W2.size);
        update_descriptor_set_storage(ctx, set, 11, grad_b2.buffer, grad_b2.offset, grad_b2.size);

        struct {
            uint32_t batch;
            uint32_t obs_dim;
            uint32_t hidden_dim;
            uint32_t act_dim;
            uint32_t out_features;
            float clip_coef;
            float policy_grad_scale;
        } push = {
            .batch = obs->shape[0],
            .obs_dim = obs->shape[1],
            .hidden_dim = layer1->out_features,
            .act_dim = net->act_dim,
            .out_features = layer2->out_features,
            .clip_coef = clip_coef,
            .policy_grad_scale = policy_grad_scale
        };

        // Dispatch: one workgroup per batch element
        uint32_t group_x = push.batch;
        dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));

        // Copy gradients back to host network
        BearTensor host_grad_W1 = { .data = layer1->param->grad.data, .shape = W1_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        bear_vulkan_tensor_to_host(ctx, &grad_W1, &host_grad_W1);

        // Free GPU tensors
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_W1);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_b1);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_W2);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_b2);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_W1);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_b1);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_W2);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_b2);

        return 0;
    }


    /* ==================================================================
     * Compute Pipeline: Policy Backward Continuous
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_policy_backward_continuous_pipeline(BearVulkanContext* ctx) {
        const char* name = "policy_backward_continuous";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 44  // batch, obs_dim, hidden_dim, act_dim, out_features, clip_coef, policy_grad_scale, logstd
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_policy_backward_continuous.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    int bear_policy_backward_continuous_vulkan(BearVulkanContext* ctx, BearPolicyNet* net,
                                                const BearVulkanTensor* obs, const BearVulkanTensor* actions,
                                                const BearVulkanTensor* old_logprobs, const BearVulkanTensor* advantages,
                                                float clip_coef, float policy_grad_scale, BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_policy_backward_continuous_pipeline(ctx);
        if (!p) return -1;

        if (!net || !net->layers || net->num_layers < 2) return -1;

        BearLayer* layer1 = &net->layers[0];
        BearLayer* layer2 = &net->layers[1];

        // Upload weights to GPU
        BearVulkanTensor gpu_W1, gpu_b1, gpu_W2, gpu_b2;
        int64_t W1_shape[2] = { layer1->out_features, layer1->in_features };
        int64_t b1_shape[1] = { layer1->out_features };
        int64_t W2_shape[2] = { layer2->out_features, layer2->in_features };
        int64_t b2_shape[1] = { layer2->out_features };

        BearTensor cpu_W1 = { .data = layer1->param->weight.data, .shape = W1_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_b1 = { .data = layer1->param->bias.data, .shape = b1_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_W2 = { .data = layer2->param->weight.data, .shape = W2_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_b2 = { .data = layer2->param->bias.data, .shape = b2_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };

        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W1, &gpu_W1);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b1, &gpu_b1);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_W2, &gpu_W2);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_b2, &gpu_b2);

        // Create gradient buffers
        BearVulkanTensor grad_W1, grad_b1, grad_W2, grad_b2;
        bear_vulkan_tensor_create(ctx, temp_arena, W1_shape, 2, BEAR_DTYPE_F32, &grad_W1, "grad_W1");
        bear_vulkan_tensor_create(ctx, temp_arena, b1_shape, 1, BEAR_DTYPE_F32, &grad_b1, "grad_b1");
        bear_vulkan_tensor_create(ctx, temp_arena, W2_shape, 2, BEAR_DTYPE_F32, &grad_W2, "grad_W2");
        bear_vulkan_tensor_create(ctx, temp_arena, b2_shape, 1, BEAR_DTYPE_F32, &grad_b2, "grad_b2");

        // Zero initialize
        void* mapped;
        bear_vulkan_tensor_map(ctx, &grad_W1);
        memset(grad_W1.mapped_ptr, 0, grad_W1.size);
        bear_vulkan_tensor_unmap(ctx, &grad_W1);
        bear_vulkan_tensor_map(ctx, &grad_b1);
        memset(grad_b1.mapped_ptr, 0, grad_b1.size);
        bear_vulkan_tensor_unmap(ctx, &grad_b1);
        bear_vulkan_tensor_map(ctx, &grad_W2);
        memset(grad_W2.mapped_ptr, 0, grad_W2.size);
        bear_vulkan_tensor_unmap(ctx, &grad_W2);
        bear_vulkan_tensor_map(ctx, &grad_b2);
        memset(grad_b2.mapped_ptr, 0, grad_b2.size);
        bear_vulkan_tensor_unmap(ctx, &grad_b2);

        VkDescriptorSet set = allocate_descriptor_set(ctx);

        /* Bindings: same as discrete */
        update_descriptor_set_storage(ctx, set, 0, obs->buffer, obs->offset, obs->size);
        update_descriptor_set_storage(ctx, set, 1, actions->buffer, actions->offset, actions->size);
        update_descriptor_set_storage(ctx, set, 2, old_logprobs->buffer, old_logprobs->offset, old_logprobs->size);
        update_descriptor_set_storage(ctx, set, 3, advantages->buffer, advantages->offset, advantages->size);
        update_descriptor_set_storage(ctx, set, 4, gpu_W1.buffer, gpu_W1.offset, gpu_W1.size);
        update_descriptor_set_storage(ctx, set, 5, gpu_b1.buffer, gpu_b1.offset, gpu_b1.size);
        update_descriptor_set_storage(ctx, set, 6, gpu_W2.buffer, gpu_W2.offset, gpu_W2.size);
        update_descriptor_set_storage(ctx, set, 7, gpu_b2.buffer, gpu_b2.offset, gpu_b2.size);
        update_descriptor_set_storage(ctx, set, 8, grad_W1.buffer, grad_W1.offset, grad_W1.size);
        update_descriptor_set_storage(ctx, set, 9, grad_b1.buffer, grad_b1.offset, grad_b1.size);
        update_descriptor_set_storage(ctx, set, 10, grad_W2.buffer, grad_W2.offset, grad_W2.size);
        update_descriptor_set_storage(ctx, set, 11, grad_b2.buffer, grad_b2.offset, grad_b2.size);

        float logstd = net->logstd ? 0.0f : net->logstd_fixed;

        struct {
            uint32_t batch;
            uint32_t obs_dim;
            uint32_t hidden_dim;
            uint32_t act_dim;
            uint32_t out_features;
            float clip_coef;
            float policy_grad_scale;
            float logstd;
        } push = {
            .batch = obs->shape[0],
            .obs_dim = obs->shape[1],
            .hidden_dim = layer1->out_features,
            .act_dim = net->act_dim,
            .out_features = layer2->out_features,
            .clip_coef = clip_coef,
            .policy_grad_scale = policy_grad_scale,
            .logstd = logstd
        };

        // Dispatch: one workgroup per batch element
        uint32_t group_x = push.batch;
        dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));

        // Copy gradients back
        BearTensor host_grad_W1 = { .data = layer1->param->grad.data, .shape = W1_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        bear_vulkan_tensor_to_host(ctx, &grad_W1, &host_grad_W1);

        // Free GPU tensors
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_W1);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_b1);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_W2);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_b2);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_W1);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_b1);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_W2);
        bear_vulkan_tensor_free(ctx, temp_arena, &grad_b2);

        return 0;
    }


    /* ==================================================================
     * Compute Pipeline: Value Forward
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_value_forward_pipeline(BearVulkanContext* ctx) {
        const char* name = "value_forward";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 16  // batch, obs_dim, hidden_dim, hidden2_dim
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_value_forward.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    void bear_value_forward_vulkan(BearVulkanContext* ctx,
                                    const BearValueNet* vnet,
                                    const BearVulkanTensor* obs,
                                    BearVulkanTensor* values,
                                    BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_value_forward_pipeline(ctx);
        if (!p) return;

        // For now, delegate to CPU
        (void)ctx; (void)vnet; (void)obs; (void)values; (void)temp_arena;
    }


    /* ==================================================================
     * Compute Pipeline: Value Backward
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_value_backward_pipeline(BearVulkanContext* ctx) {
        const char* name = "value_backward";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 20  // batch, obs_dim, hidden_dim, hidden2_dim, vf_coef
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_value_backward.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    int bear_value_backward_vulkan(BearVulkanContext* ctx,
                                    BearValueNet* vnet,
                                    const BearVulkanTensor* obs,
                                    const BearVulkanTensor* values,
                                    const BearVulkanTensor* targets,
                                    float vf_coef,
                                    BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_value_backward_pipeline(ctx);
        if (!p) return -1;

        // For now, delegate to CPU
        (void)ctx; (void)vnet; (void)obs; (void)values; (void)targets; (void)vf_coef; (void)temp_arena;
        return 0;
    }


    /* ==================================================================
     * Compute Pipeline: PPO Loss
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_ppo_loss_pipeline(BearVulkanContext* ctx) {
        const char* name = "ppo_loss";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 52  // all push constants
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_ppo_loss.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    BearPPOLoss bear_ppo_loss_vulkan(BearVulkanContext* ctx,
                                      const BearPolicyNet* policy,
                                      const BearValueNet* critic,
                                      const BearVulkanTensor* obs,
                                      const BearVulkanTensor* actions,
                                      const BearVulkanTensor* old_logprobs,
                                      const BearVulkanTensor* advantages,
                                      const BearVulkanTensor* returns,
                                      const BearVulkanTensor* old_values,
                                      const BearPPOConfig* cfg,
                                      BearVulkanArena* temp_arena) {
        BearPPOLoss loss = {0};

        struct BearVulkanPipeline* p = get_or_create_ppo_loss_pipeline(ctx);
        if (!p) return loss;

        // For now, delegate to CPU
        (void)ctx; (void)policy; (void)critic; (void)obs; (void)actions;
        (void)old_logprobs; (void)advantages; (void)returns; (void)old_values; (void)cfg; (void)temp_arena;

        return loss;
    }


    /* ==================================================================
     * Compute Pipeline: PPO Apply Gradients (Adam)
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_ppo_apply_gradients_pipeline(BearVulkanContext* ctx) {
        const char* name = "ppo_apply_gradients";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 28  // n, lr, beta1, beta2, eps, wd, step
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_ppo_apply_gradients.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    void bear_ppo_apply_gradients_vulkan(BearVulkanContext* ctx,
                                              BearPolicyNet* policy,
                                              BearValueNet* critic,
                                              BearOptimizer* opt_policy,
                                              BearOptimizer* opt_critic) {
            struct BearVulkanPipeline* p = get_or_create_ppo_apply_gradients_pipeline(ctx);
            if (!p) return;

            // For now, delegate to CPU
            (void)ctx; (void)policy; (void)critic; (void)opt_policy; (void)opt_critic;
        }


    /* ==================================================================
     * Compute Pipeline: MinGRU Step
     * ================================================================== */

    struct BearVulkanPipeline* get_or_create_mingru_step_pipeline(BearVulkanContext* ctx) {
        const char* name = "mingru_step";
        for (int i = 0; i < ctx->num_pipelines; ++i) {
            if (strcmp(ctx->pipelines[i].name, name) == 0) {
                return &ctx->pipelines[i];
            }
        }

        if (ctx->num_pipelines >= 64) return NULL;

        struct BearVulkanPipeline* p = &ctx->pipelines[ctx->num_pipelines++];
        strncpy(p->name, name, 63);
        p->active = 0;

        VkPipelineLayoutCreateInfo playout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &ctx->storage_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &(VkPushConstantRange){
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = 8  // batch, hid
            }
        };
        VK_CHECK(ctx, vkCreatePipelineLayout(ctx->device, &playout_info, NULL, &p->layout));

        VkShaderModule shader = create_shader_module(ctx, "bear_vulkan_shaders/bear_mingru_step.comp.spv");
        if (shader == VK_NULL_HANDLE) return NULL;

        VkComputePipelineCreateInfo pipe_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main"
            },
            .layout = p->layout
        };
        VK_CHECK(ctx, vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &p->pipeline));

        vkDestroyShaderModule(ctx->device, shader, NULL);
        p->active = 1;
        return p;
    }

    void bear_mingru_step_vulkan(BearVulkanContext* ctx,
                                  const BearMinGRU* gru,
                                  const BearVulkanTensor* x,
                                  const BearVulkanTensor* h_in,
                                  BearVulkanTensor* h_out,
                                  BearVulkanArena* temp_arena) {
        struct BearVulkanPipeline* p = get_or_create_mingru_step_pipeline(ctx);
        if (!p) return;

        // Upload weights
        BearVulkanTensor gpu_Wz, gpu_Uz, gpu_bz, gpu_Wr, gpu_Ur, gpu_br, gpu_Wn, gpu_Un, gpu_bn;
        int64_t Wz_shape[2] = { gru->hid_size, (int)gru->Wz.weight.shape[1] };
        int64_t Uz_shape[2] = { gru->hid_size, gru->hid_size };
        int64_t bz_shape[1] = { gru->hid_size };
        int64_t Wr_shape[2] = { gru->hid_size, (int)gru->Wr.weight.shape[1] };
        int64_t Ur_shape[2] = { gru->hid_size, gru->hid_size };
        int64_t br_shape[1] = { gru->hid_size };
        int64_t Wn_shape[2] = { gru->hid_size, (int)gru->Wn.weight.shape[1] };
        int64_t Un_shape[2] = { gru->hid_size, gru->hid_size };
        int64_t bn_shape[1] = { gru->hid_size };

        BearTensor cpu_Wz = { .data = gru->Wz.weight.data, .shape = Wz_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Uz = { .data = gru->Uz.weight.data, .shape = Uz_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_bz = { .data = gru->bz.bias.data, .shape = bz_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Wr = { .data = gru->Wr.weight.data, .shape = Wr_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Ur = { .data = gru->Ur.weight.data, .shape = Ur_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_br = { .data = gru->br.bias.data, .shape = br_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Wn = { .data = gru->Wn.weight.data, .shape = Wn_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_Un = { .data = gru->Un.weight.data, .shape = Un_shape, .ndim = 2, .dtype = BEAR_DTYPE_F32 };
        BearTensor cpu_bn = { .data = gru->bn.bias.data, .shape = bn_shape, .ndim = 1, .dtype = BEAR_DTYPE_F32 };

        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Wz, &gpu_Wz);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Uz, &gpu_Uz);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_bz, &gpu_bz);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Wr, &gpu_Wr);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Ur, &gpu_Ur);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_br, &gpu_br);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Wn, &gpu_Wn);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_Un, &gpu_Un);
        bear_vulkan_tensor_from_host(ctx, temp_arena, &cpu_bn, &gpu_bn);

        VkDescriptorSet set = allocate_descriptor_set(ctx);

        /* Bindings matching mingru_step.comp:
         *   0: x
         *   1: h_in
         *   2: Wz
         *   3: Uz
         *   4: bz
         *   5: Wr
         *   6: Ur
         *   7: br
         *   8: Wn
         *   9: Un
         *   10: bn
         *   11: h_out
         */
        update_descriptor_set_storage(ctx, set, 0, x->buffer, x->offset, x->size);
        update_descriptor_set_storage(ctx, set, 1, h_in->buffer, h_in->offset, h_in->size);
        update_descriptor_set_storage(ctx, set, 2, gpu_Wz.buffer, gpu_Wz.offset, gpu_Wz.size);
        update_descriptor_set_storage(ctx, set, 3, gpu_Uz.buffer, gpu_Uz.offset, gpu_Uz.size);
        update_descriptor_set_storage(ctx, set, 4, gpu_bz.buffer, gpu_bz.offset, gpu_bz.size);
        update_descriptor_set_storage(ctx, set, 5, gpu_Wr.buffer, gpu_Wr.offset, gpu_Wr.size);
        update_descriptor_set_storage(ctx, set, 6, gpu_Ur.buffer, gpu_Ur.offset, gpu_Ur.size);
        update_descriptor_set_storage(ctx, set, 7, gpu_br.buffer, gpu_br.offset, gpu_br.size);
        update_descriptor_set_storage(ctx, set, 8, gpu_Wn.buffer, gpu_Wn.offset, gpu_Wn.size);
        update_descriptor_set_storage(ctx, set, 9, gpu_Un.buffer, gpu_Un.offset, gpu_Un.size);
        update_descriptor_set_storage(ctx, set, 10, gpu_bn.buffer, gpu_bn.offset, gpu_bn.size);
        update_descriptor_set_storage(ctx, set, 11, h_out->buffer, h_out->offset, h_out->size);

        struct {
            uint32_t batch;
            uint32_t hid;
        } push = {
            .batch = x->shape[0],
            .hid = gru->hid_size
        };

        uint32_t group_x = (push.batch * push.hid + 255) / 256;
        dispatch_compute(ctx, p->pipeline, p->layout, set, p, group_x, 1, 1, &push, sizeof(push));

        // Free GPU tensors
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Wz);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Uz);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_bz);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Wr);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Ur);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_br);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Wn);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_Un);
        bear_vulkan_tensor_free(ctx, temp_arena, &gpu_bn);
    }


    /* ... remaining stubs ... */


/* ===================================================================
 * Unified Backend Configuration and Auto-Dispatch
 * =================================================================== */

BearVulkanConfig bear_vulkan_config = {
    .use_vulkan = 1,
    .min_batch_for_vulkan = 256,
    .device_index = -1,
    .fallback_to_cpu = 1,
    .enable_validation = 0
};

static BearVulkanContext* g_vulkan_ctx = NULL;

int bear_backend_init_vulkan(const BearVulkanConfig* cfg) {
    if (cfg) {
        bear_vulkan_config = *cfg;
    }

    if (!bear_vulkan_config.use_vulkan) {
        return 0;  // CPU mode
    }

    BearVulkanStatus status = bear_vulkan_query();
    if (status != BEAR_VULKAN_AVAILABLE && status != BEAR_VULKAN_ACTIVE) {
        if (bear_vulkan_config.fallback_to_cpu) {
            bear_vulkan_config.use_vulkan = 0;
            return 0;
        }
        return -1;
    }

    g_vulkan_ctx = bear_vulkan_init(bear_vulkan_config.device_index);
    if (!g_vulkan_ctx) {
        if (bear_vulkan_config.fallback_to_cpu) {
            bear_vulkan_config.use_vulkan = 0;
            return 0;
        }
        return -1;
    }

    return 0;
}

void bear_backend_shutdown_vulkan(void) {
    if (g_vulkan_ctx) {
        bear_vulkan_destroy(g_vulkan_ctx);
        g_vulkan_ctx = NULL;
    }
}

void bear_policy_forward_unified_v(const BearPolicyNet* net,
                                    const BearTensor* obs,
                                    const BearTensor* h_in,
                                    BearTensor* actions,
                                    BearTensor* logprobs,
                                    BearTensor* values,
                                    BearTensor* h_out,
                                    BearArena* temp_arena) {
    if (bear_vulkan_config.use_vulkan && g_vulkan_ctx && obs->shape[0] >= bear_vulkan_config.min_batch_for_vulkan) {
        // Try Vulkan path
        // For now, fall back to CPU
    }
    // CPU fallback
    bear_policy_forward(net, obs, h_in, actions, logprobs, values, h_out, temp_arena);
}

int bear_policy_backward_unified_v(BearPolicyNet* net,
                                    const BearTensor* obs,
                                    const BearTensor* actions,
                                    const BearTensor* old_logprobs,
                                    const BearTensor* advantages,
                                    float clip_coef,
                                    float policy_grad_scale,
                                    BearArena* temp_arena) {
    if (bear_vulkan_config.use_vulkan && g_vulkan_ctx && obs->shape[0] >= bear_vulkan_config.min_batch_for_vulkan) {
        // Try Vulkan path
        // For now, fall back to CPU
    }
    // CPU fallback
    return bear_policy_backward(net, obs, actions, old_logprobs, advantages, clip_coef, policy_grad_scale, temp_arena);
}

void bear_value_forward_unified_v(const BearValueNet* vnet,
                                   const BearTensor* obs,
                                   BearTensor* values,
                                   BearArena* temp_arena) {
    if (bear_vulkan_config.use_vulkan && g_vulkan_ctx && obs->shape[0] >= bear_vulkan_config.min_batch_for_vulkan) {
        // Try Vulkan path
    }
    // CPU fallback
    bear_value_forward(vnet, obs, values, temp_arena);
}

int bear_value_backward_unified_v(BearValueNet* vnet,
                                   const BearTensor* obs,
                                   const BearTensor* values,
                                   const BearTensor* targets,
                                   float vf_coef,
                                   BearArena* temp_arena) {
    if (bear_vulkan_config.use_vulkan && g_vulkan_ctx && obs->shape[0] >= bear_vulkan_config.min_batch_for_vulkan) {
        // Try Vulkan path
    }
    // CPU fallback
    return bear_value_backward(vnet, obs, values, targets, vf_coef, temp_arena);
}

float bear_trainer_iter_unified_v(BearTrainer* trainer, uint64_t rng_state[2]) {
    // For now, just use CPU path
    return bear_trainer_iter(trainer, rng_state);
}

/* When Vulkan SDK is unavailable, bear_vulkan_soft.c implements all
 * the same API functions using pure C compute on the host CPU.
 * This object file is intentionally empty — the linker picks up
 * bear_vulkan_soft.o instead.
 */

#endif /* HAS_VULKAN */
