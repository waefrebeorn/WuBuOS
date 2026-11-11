/*
 * wubu_vkd3d_live_uav_test.c -- VKD3D: a REAL UAV / storage buffer + compute.
 *
 * The last descriptor type.  A compute shader writes through a storage buffer
 * (D3D12's UAV) and the host reads it back -- proving the UAV descriptor, the
 * compute pipeline, and dispatch.
 *
 * GPU-requiring: a separate target, not part of the gate.
 */
#include "wubu_vkd3d_physdev.h"
#include "wubu_vkd3d_vkdev.h"
#include "wubu_vkd3d_memory.h"
#include "wubu_vkd3d_pso.h"
#include "wubu_vkd3d_descset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N       256
#define XLOCAL  64
#define SHADER_DIR "build/vkd3d_shaders"

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static VkPhysicalDeviceMemoryProperties g_memprops;
static wubu_vkd3d_memory_types_t g_types;
static VkInstance       g_inst;
static VkPhysicalDevice g_phys;
static VkDevice         g_dev;
static uint32_t         g_gfx_family;
static VkQueue          g_queue;
static VkDescriptorPool g_pool;

static bool load_spirv(const char *path, uint32_t **words, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || (sz % 4) != 0) { fclose(f); return false; }
    uint32_t *buf = (uint32_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return false; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz || buf[0] != 0x07230203u) { free(buf); return false; }
    *words = buf; *n = (size_t)sz / 4;
    return true;
}

static int memory_type(VkDeviceSize size, uint32_t bits,
                       wubu_vkd3d_heap_type_t heap, const char **why)
{
    wubu_vkd3d_alloc_request_t ar;
    memset(&ar, 0, sizeof(ar));
    ar.size = size;
    ar.heap_type = heap;
    ar.type_bits = bits;
    return wubu_vkd3d_memory_type_index(&g_types, &ar, why);
}

int main(void)
{
    printf("=== test_vkd3d_live_uav (storage buffer + compute) ===\n");

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wubu-vkd3d-live-uav";
    app.apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);
    VkInstanceCreateInfo ici;
    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    if (vkCreateInstance(&ici, NULL, &g_inst) != VK_SUCCESS) {
        printf("  SKIP: no Vulkan loader\n");
        return 0;
    }

    wubu_vkd3d_physdev_t devs[8];
    int n = wubu_vkd3d_physdev_enumerate(g_inst, devs, 8);
    int chosen = -1;
    for (int i = 0; i < n; i++) {
        if (devs[i].properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            chosen = i; break;
        }
        if (chosen < 0) chosen = i;
    }
    if (chosen < 0) { printf("  SKIP: no device\n"); return 0; }
    g_phys = devs[chosen].device;
    printf("  device: %s\n", devs[chosen].properties.deviceName);
    for (uint32_t q = 0; q < devs[chosen].queue_family_count; q++) {
        if (devs[chosen].queue_families[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            g_gfx_family = q; break;
        }
    }

    wubu_vkd3d_vkdev_desc_t dd;
    memset(&dd, 0, sizeof(dd));
    dd.queue_request_count = 1;
    dd.queues[0].family_index = g_gfx_family;
    dd.queues[0].queue_count = 1;
    dd.queues[0].priority = 1.0f;
    CHECK(wubu_vkd3d_vkdev_create(&devs[chosen], &dd, &g_dev), "logical device created");
    vkGetDeviceQueue(g_dev, g_gfx_family, 0, &g_queue);

    memset(&g_types, 0, sizeof(g_types));
    vkGetPhysicalDeviceMemoryProperties(g_phys, &g_memprops);
    g_types.count = g_memprops.memoryTypeCount;
    for (uint32_t i = 0; i < g_types.count && i < VKD3D_MAX_MEMORY_TYPES; i++)
        g_types.types[i] = g_memprops.memoryTypes[i];

    /* -- the storage buffer, host-visible so it can be read straight back -- */
    VkBuffer sbuf = VK_NULL_HANDLE;
    VkDeviceMemory smem = VK_NULL_HANDLE;
    void *shost = NULL;
    {
        VkBufferCreateInfo bci;
        memset(&bci, 0, sizeof(bci));
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = (VkDeviceSize)N * sizeof(uint32_t);
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CHECK(vkCreateBuffer(g_dev, &bci, NULL, &sbuf) == VK_SUCCESS,
              "storage buffer created");

        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(g_dev, sbuf, &reqs);
        const char *why = NULL;
        int idx = memory_type(reqs.size, reqs.memoryTypeBits, VKD3D_HEAP_UPLOAD, &why);
        CHECK(idx >= 0, "a host-visible memory type exists for the UAV");
        VkMemoryAllocateInfo mai;
        memset(&mai, 0, sizeof(mai));
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = reqs.size;
        mai.memoryTypeIndex = (uint32_t)idx;
        CHECK(vkAllocateMemory(g_dev, &mai, NULL, &smem) == VK_SUCCESS,
              "storage buffer memory allocated");
        vkBindBufferMemory(g_dev, sbuf, smem, 0);
        CHECK(vkMapMemory(g_dev, smem, 0, (VkDeviceSize)N * sizeof(uint32_t),
                          0, &shost) == VK_SUCCESS,
              "the storage buffer is mapped for readback");
    }

    /* -- the UAV descriptor: binding 0 = STORAGE_BUFFER, compute stage -- */
    VkDescriptorSetLayoutBinding dslb;
    memset(&dslb, 0, sizeof(dslb));
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dslb.descriptorCount = 1;
    dslb.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    {
        VkDescriptorSetLayoutCreateInfo dslci;
        memset(&dslci, 0, sizeof(dslci));
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 1;
        dslci.pBindings = &dslb;
        CHECK(vkCreateDescriptorSetLayout(g_dev, &dslci, NULL, &dsl) == VK_SUCCESS,
              "descriptor set layout created");
    }
    VkDescriptorSet ds = VK_NULL_HANDLE;
    {
        VkDescriptorPoolSize poolSize;
        memset(&poolSize, 0, sizeof(poolSize));
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1;
        VkDescriptorPoolCreateInfo dpci;
        memset(&dpci, 0, sizeof(dpci));
        dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpci.maxSets = 1;
        dpci.poolSizeCount = 1;
        dpci.pPoolSizes = &poolSize;
        CHECK(vkCreateDescriptorPool(g_dev, &dpci, NULL, &g_pool) == VK_SUCCESS,
              "descriptor pool created");

        VkDescriptorSetAllocateInfo dsai;
        memset(&dsai, 0, sizeof(dsai));
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = g_pool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &dsl;
        CHECK(vkAllocateDescriptorSets(g_dev, &dsai, &ds) == VK_SUCCESS,
              "descriptor set allocated");

        VkDescriptorBufferInfo dbi;
        memset(&dbi, 0, sizeof(dbi));
        dbi.buffer = sbuf;
        dbi.offset = 0;
        dbi.range = (VkDeviceSize)N * sizeof(uint32_t);
        VkWriteDescriptorSet wds;
        memset(&wds, 0, sizeof(wds));
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.dstSet = ds;
        wds.dstBinding = 0;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wds.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(g_dev, 1, &wds, 0, NULL);
    }

    VkPipelineLayout layout = VK_NULL_HANDLE;
    {
        VkPipelineLayoutCreateInfo plci;
        memset(&plci, 0, sizeof(plci));
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &dsl;
        CHECK(vkCreatePipelineLayout(g_dev, &plci, NULL, &layout) == VK_SUCCESS,
              "pipeline layout created");
    }

    /* -- the compute pipeline -- */
    uint32_t *cw = NULL;
    size_t cn = 0;
    char path[256];
    snprintf(path, sizeof(path), "%s/uav.comp.spv", SHADER_DIR);
    CHECK(load_spirv(path, &cw, &cn), "compute SPIR-V loaded");

    VkShaderModule cs = VK_NULL_HANDLE;
    if (cw) {
        VkShaderModuleCreateInfo sm;
        memset(&sm, 0, sizeof(sm));
        sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        sm.codeSize = cn * 4; sm.pCode = cw;
        CHECK(vkCreateShaderModule(g_dev, &sm, NULL, &cs) == VK_SUCCESS,
              "compute shader module created");
    }

    VkPipeline pipeline = VK_NULL_HANDLE;
    {
        VkComputePipelineCreateInfo cpci;
        memset(&cpci, 0, sizeof(cpci));
        cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        cpci.stage.module = cs;
        cpci.stage.pName = "main";
        cpci.layout = layout;
        CHECK(vkCreateComputePipelines(g_dev, VK_NULL_HANDLE, 1, &cpci, NULL,
                                       &pipeline) == VK_SUCCESS &&
              pipeline != VK_NULL_HANDLE,
              "a REAL VkPipeline is created for compute");
    }

    /* -- dispatch and read back -- */
    {
        VkCommandPoolCreateInfo cpci;
        memset(&cpci, 0, sizeof(cpci));
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = g_gfx_family;
        VkCommandPool pool = VK_NULL_HANDLE;
        vkCreateCommandPool(g_dev, &cpci, NULL, &pool);

        VkCommandBufferAllocateInfo cbai;
        memset(&cbai, 0, sizeof(cbai));
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = pool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(g_dev, &cbai, &cb);

        VkCommandBufferBeginInfo cbbi;
        memset(&cbbi, 0, sizeof(cbbi));
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &cbbi);

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE,
                                layout, 0, 1, &ds, 0, NULL);
        vkCmdDispatch(cb, N / XLOCAL, 1, 1);   /* 4 groups x 64 = 256 */
        vkEndCommandBuffer(cb);

        VkSubmitInfo si;
        memset(&si, 0, sizeof(si));
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1; si.pCommandBuffers = &cb;
        CHECK(vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS,
              "the dispatch is SUBMITTED");
        CHECK(vkQueueWaitIdle(g_queue) == VK_SUCCESS,
              "the GPU COMPLETES the dispatch");

        const uint32_t *out = (const uint32_t *)shost;
        int bad = 0;
        for (int i = 0; i < N; i++) {
            if (out[i] != (uint32_t)(i * 2)) {
                if (bad < 4) printf("  values[%d] = %u, expected %u\n",
                                    i, out[i], (unsigned)(i * 2));
                bad++;
            }
        }
        CHECK(bad == 0, "every UAV element holds its global id doubled");
        printf("  UAV verified: values[0]=%u values[1]=%u values[255]=%u\n",
               out[0], out[1], out[255]);

        vkDestroyCommandPool(g_dev, pool, NULL);
    }

    /* -- teardown -- */
    vkDestroyPipeline(g_dev, pipeline, NULL);
    vkDestroyPipelineLayout(g_dev, layout, NULL);
    vkDestroyDescriptorSetLayout(g_dev, dsl, NULL);
    vkDestroyDescriptorPool(g_dev, g_pool, NULL);
    vkDestroyShaderModule(g_dev, cs, NULL);
    vkDestroyBuffer(g_dev, sbuf, NULL);
    vkFreeMemory(g_dev, smem, NULL);
    vkDestroyDevice(g_dev, NULL);
    vkDestroyInstance(g_inst, NULL);
    free(cw);

    if (failures == 0) printf("ALL VKD3D_LIVE_UAV TESTS PASSED\n");
    else printf("%d VKD3D_LIVE_UAV FAILURES\n", failures);
    return failures ? 1 : 0;
}
