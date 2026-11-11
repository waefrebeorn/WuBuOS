/*
 * wubu_vkd3d_live_binding_test.c -- VKD3D: a REAL root signature on hardware.
 *
 * The live_pipeline test hardcoded the triangle's colour in the shader.  This
 * one drives it through D3D12-style binding:
 *
 *   a root constant (b0, 1 x float)   -> a Vulkan PUSH CONSTANT
 *   a constant buffer   (b0, vec3)    -> a real CBV in a DESCRIPTOR SET
 *
 * Both slices 13's mappings are used to build the pipeline layout, and the
 * readback proves the values arrived: the shader computes base * scale, so
 * changing EITHER must change the rendered pixel.
 *
 * GPU-requiring: a separate target, not part of the gate.
 */
#include "wubu_vkd3d.h"
#include "wubu_vkd3d_physdev.h"
#include "wubu_vkd3d_vkdev.h"
#include "wubu_vkd3d_memory.h"
#include "wubu_vkd3d_renderpass.h"
#include "wubu_vkd3d_framebuffer.h"
#include "wubu_vkd3d_pso.h"
#include "wubu_vkd3d_descset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 1920
#define H 1080
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
static VkDescriptorPool g_pool;      /* must outlive every set allocated from it */

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
    *words = buf;
    *n = (size_t)sz / 4;
    return true;
}

/* Upload `data` to a host-visible buffer using slice 12's UPLOAD heap. */
static bool make_upload_buffer(const void *data, VkDeviceSize size,
                               VkBuffer *buf, VkDeviceMemory *mem, void **mapped)
{
    VkBufferCreateInfo bci;
    memset(&bci, 0, sizeof(bci));
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(g_dev, &bci, NULL, buf) != VK_SUCCESS) return false;

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(g_dev, *buf, &reqs);
    wubu_vkd3d_alloc_request_t ar;
    memset(&ar, 0, sizeof(ar));
    ar.size = reqs.size;
    ar.heap_type = VKD3D_HEAP_UPLOAD;
    ar.type_bits = reqs.memoryTypeBits;
    const char *why = NULL;
    int idx = wubu_vkd3d_memory_type_index(&g_types, &ar, &why);
    if (idx < 0) { printf("  no upload memory: %s\n", why ? why : "?"); return false; }

    VkMemoryAllocateInfo mai;
    memset(&mai, 0, sizeof(mai));
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = reqs.size;
    mai.memoryTypeIndex = (uint32_t)idx;
    if (vkAllocateMemory(g_dev, &mai, NULL, mem) != VK_SUCCESS) return false;
    if (vkBindBufferMemory(g_dev, *buf, *mem, 0) != VK_SUCCESS) return false;
    if (vkMapMemory(g_dev, *mem, 0, size, 0, mapped) != VK_SUCCESS) return false;
    memcpy(*mapped, data, (size_t)size);
    return true;
}

static bool make_attachment(VkFormat format, bool depth,
                            VkImageUsageFlags usage,
                            VkImage *image, VkDeviceMemory *mem, VkImageView *view)
{
    VkImageCreateInfo ici;
    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent.width = W; ici.extent.height = H; ici.extent.depth = 1;
    ici.mipLevels = 1; ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(g_dev, &ici, NULL, image) != VK_SUCCESS) return false;

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(g_dev, *image, &reqs);
    wubu_vkd3d_alloc_request_t ar;
    memset(&ar, 0, sizeof(ar));
    ar.size = reqs.size;
    ar.heap_type = VKD3D_HEAP_DEFAULT;
    ar.type_bits = reqs.memoryTypeBits;
    const char *why = NULL;
    int idx = wubu_vkd3d_memory_type_index(&g_types, &ar, &why);
    if (idx < 0) return false;

    VkMemoryAllocateInfo mai;
    memset(&mai, 0, sizeof(mai));
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = reqs.size;
    mai.memoryTypeIndex = (uint32_t)idx;
    if (vkAllocateMemory(g_dev, &mai, NULL, mem) != VK_SUCCESS) return false;
    if (vkBindImageMemory(g_dev, *image, *mem, 0) != VK_SUCCESS) return false;

    VkImageAspectFlags aspect;
    if (!wubu_vkd3d_aspect_flags(format, &aspect)) return false;
    VkImageViewCreateInfo ivci;
    memset(&ivci, 0, sizeof(ivci));
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = *image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = format;
    ivci.subresourceRange.aspectMask = aspect;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    return vkCreateImageView(g_dev, &ivci, NULL, view) == VK_SUCCESS;
}

int main(void)
{
    printf("=== test_vkd3d_live_binding (push constant + CBV) ===\n");

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wubu-vkd3d-live-binding";
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

    /* -- the CBV: a real constant buffer holding the base colour -- */
    float base[3] = { 0.9f, 0.2f, 0.1f };
    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceMemory ubo_mem = VK_NULL_HANDLE;
    void *ubo_host = NULL;
    CHECK(make_upload_buffer(base, sizeof(base), &ubo, &ubo_mem, &ubo_host),
          "the constant buffer is created and filled");

    /* -- the colour image -- */
    VkImage color_img = VK_NULL_HANDLE;
    VkDeviceMemory color_mem = VK_NULL_HANDLE;
    VkImageView color_view = VK_NULL_HANDLE;
    CHECK(make_attachment(VK_FORMAT_R8G8B8A8_UNORM, false,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                          &color_img, &color_mem, &color_view),
          "colour attachment created");

    /* -- render pass + framebuffer -- */
    wubu_vkd3d_renderpass_desc_t rp;
    memset(&rp, 0, sizeof(rp));
    rp.attachment_count = 1;
    rp.attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    rp.attachments[0].samples = 1;
    rp.attachments[0].begin = VKD3D_BEGIN_CLEAR;
    rp.attachments[0].end = VKD3D_END_PRESERVE;
    rp.attachments[0].initial_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    rp.attachments[0].final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription ad[8];
    int nad = wubu_vkd3d_renderpass_attachments(&rp, ad, 8);
    VkAttachmentReference crefs[8];
    int ncref = wubu_vkd3d_renderpass_color_refs(&rp, crefs, 8);

    VkSubpassDescription sub;
    memset(&sub, 0, sizeof(sub));
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = (uint32_t)ncref;
    sub.pColorAttachments = crefs;

    VkRenderPassCreateInfo rpci;
    memset(&rpci, 0, sizeof(rpci));
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = (uint32_t)nad;
    rpci.pAttachments = ad;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;

    VkRenderPass renderpass = VK_NULL_HANDLE;
    CHECK(vkCreateRenderPass(g_dev, &rpci, NULL, &renderpass) == VK_SUCCESS,
          "render pass created");

    wubu_vkd3d_framebuffer_desc_t fb;
    memset(&fb, 0, sizeof(fb));
    fb.attachment_count = 1;
    fb.attachments[0].view = color_view;
    fb.attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    fb.attachments[0].width = W; fb.attachments[0].height = H;
    fb.attachments[0].samples = 1; fb.attachments[0].is_depth_stencil = false;
    fb.width = W; fb.height = H; fb.layers = 1;

    VkFramebufferCreateInfo fbci;
    CHECK(wubu_vkd3d_framebuffer_create_info(&fb, &fbci), "framebuffer translates");
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    CHECK(vkCreateFramebuffer(g_dev, &fbci, NULL, &framebuffer) == VK_SUCCESS,
          "framebuffer created");

    /* -- THE ROOT SIGNATURE (slice 13) -- */
    wubu_vkd3d_root_signature_desc_t rs;
    memset(&rs, 0, sizeof(rs));
    /* b0 = one root constant (the scale) */
    rs.params[rs.param_count].type = VKD3D_ROOT_CONSTANTS;
    rs.params[rs.param_count].register_num = 0;
    rs.params[rs.param_count].num_32bit_values = 1;
    rs.params[rs.param_count].stages = VK_SHADER_STAGE_VERTEX_BIT;
    rs.param_count++;
    /* b0 = the CBV (the base colour) */
    rs.params[rs.param_count].type = VKD3D_ROOT_CBV;
    rs.params[rs.param_count].register_num = 0;
    rs.params[rs.param_count].stages = VK_SHADER_STAGE_VERTEX_BIT;
    rs.param_count++;
    CHECK(wubu_vkd3d_root_signature_validate(&rs) == NULL, "the root signature is legal");

    uint32_t pc_off = 0, pc_size = 0;
    VkShaderStageFlags pc_stages = 0;
    CHECK(wubu_vkd3d_root_signature_push_constants(&rs, &pc_off, &pc_size, &pc_stages),
          "the root constants translate");
    CHECK(pc_off == 0 && pc_size == 4,
          "b0 with 1 value -> byte offset 0, size 4");

    VkDescriptorSetLayoutBinding bindings[8];
    int nbind = wubu_vkd3d_root_signature_bindings(&rs, bindings, 8);
    CHECK(nbind == 1 && bindings[0].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          "the CBV becomes one UNIFORM_BUFFER binding");

    /* -- descriptor set layout + pool + set -- */
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    {
        VkDescriptorSetLayoutCreateInfo dslci;
        memset(&dslci, 0, sizeof(dslci));
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = (uint32_t)nbind;
        dslci.pBindings = bindings;
        CHECK(vkCreateDescriptorSetLayout(g_dev, &dslci, NULL, &dsl) == VK_SUCCESS,
              "descriptor set layout created");
    }
    VkDescriptorSet ds = VK_NULL_HANDLE;
    {
        VkDescriptorPoolSize poolSize;
        memset(&poolSize, 0, sizeof(poolSize));
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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
        dbi.buffer = ubo;
        dbi.offset = 0;
        dbi.range = sizeof(base);
        VkWriteDescriptorSet wds;
        memset(&wds, 0, sizeof(wds));
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.dstSet = ds;
        wds.dstBinding = 0;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(g_dev, 1, &wds, 0, NULL);
        /* NOTE: the pool must NOT be destroyed here -- destroying a pool
         * invalidates every descriptor set allocated from it, and binding a
         * stale set handle crashes the driver.  It is torn down at the end. */
    }

    /* -- pipeline layout: descriptor set + push constants -- */
    VkPushConstantRange pcr;
    memset(&pcr, 0, sizeof(pcr));
    pcr.stageFlags = pc_stages;
    pcr.offset = pc_off;
    pcr.size = pc_size;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    {
        VkPipelineLayoutCreateInfo plci;
        memset(&plci, 0, sizeof(plci));
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &dsl;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        CHECK(vkCreatePipelineLayout(g_dev, &plci, NULL, &layout) == VK_SUCCESS,
              "pipeline layout created from the root signature");
    }

    /* -- the shaders -- */
    uint32_t *vw = NULL, *fw = NULL;
    size_t vn = 0, fn = 0;
    char path[256];
    snprintf(path, sizeof(path), "%s/binding.vert.spv", SHADER_DIR);
    CHECK(load_spirv(path, &vw, &vn), "vertex SPIR-V loaded");
    snprintf(path, sizeof(path), "%s/binding.frag.spv", SHADER_DIR);
    CHECK(load_spirv(path, &fw, &fn), "fragment SPIR-V loaded");

    VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
    if (vw) {
        VkShaderModuleCreateInfo sm;
        memset(&sm, 0, sizeof(sm));
        sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        sm.codeSize = vn * 4; sm.pCode = vw;
        vkCreateShaderModule(g_dev, &sm, NULL, &vs);
    }
    if (fw) {
        VkShaderModuleCreateInfo sm;
        memset(&sm, 0, sizeof(sm));
        sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        sm.codeSize = fn * 4; sm.pCode = fw;
        vkCreateShaderModule(g_dev, &sm, NULL, &fs);
    }
    CHECK(vs && fs, "shader modules created");

    /* -- the pipeline, from slice 4's mapping -- */
    wubu_vkd3d_pso_desc_t pd;
    memset(&pd, 0, sizeof(pd));
    pd.vs_hash = 0xA11CE; pd.ps_hash = 0xBEEF;
    pd.fill_mode = 2; pd.cull_mode = 2;
    pd.depth_enable = false;
    pd.primitive_topology = 4;
    pd.rtv_count = 1; pd.rtv_formats[0] = 41;
    pd.sample_count = 1; pd.sample_mask = 0xFFFFFFFF;
    pd.blend_src_color = 5; pd.blend_dst_color = 6; pd.blend_op_color = 1;
    pd.blend_src_alpha = 5; pd.blend_dst_alpha = 6; pd.blend_op_alpha = 1;

    wubu_vkd3d_pso_vk_t pv;
    CHECK(wubu_vkd3d_pso_vk(&pd, &pv) && pv.valid, "the PSO slice maps the desc");

    VkPipelineShaderStageCreateInfo stages[2];
    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vs; stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fs; stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vinput;
    memset(&vinput, 0, sizeof(vinput));
    vinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo iasm;
    memset(&iasm, 0, sizeof(iasm));
    iasm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iasm.topology = pv.topology;

    VkViewport viewport;
    memset(&viewport, 0, sizeof(viewport));
    viewport.width = (float)W; viewport.height = (float)H;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor;
    memset(&scissor, 0, sizeof(scissor));
    scissor.extent.width = W; scissor.extent.height = H;
    VkPipelineViewportStateCreateInfo vport;
    memset(&vport, 0, sizeof(vport));
    vport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vport.viewportCount = 1; vport.pViewports = &viewport;
    vport.scissorCount = 1; vport.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster;
    memset(&raster, 0, sizeof(raster));
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = pv.polygon_mode;
    raster.cullMode = pv.cull_mode;
    raster.frontFace = pv.front_face;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msaa;
    memset(&msaa, 0, sizeof(msaa));
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba;
    memset(&cba, 0, sizeof(cba));
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbs.attachmentCount = 1; cbs.pAttachments = &cba;

    VkGraphicsPipelineCreateInfo gpci;
    memset(&gpci, 0, sizeof(gpci));
    gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount = 2; gpci.pStages = stages;
    gpci.pVertexInputState = &vinput;
    gpci.pInputAssemblyState = &iasm;
    gpci.pViewportState = &vport;
    gpci.pRasterizationState = &raster;
    gpci.pMultisampleState = &msaa;
    gpci.pColorBlendState = &cbs;
    gpci.layout = layout;
    gpci.renderPass = renderpass;
    gpci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    CHECK(vkCreateGraphicsPipelines(g_dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline)
          == VK_SUCCESS && pipeline != VK_NULL_HANDLE,
          "pipeline created with a real pipeline layout");

    /* -- a readback buffer -- */
    VkBuffer rb_buf = VK_NULL_HANDLE;
    VkDeviceMemory rb_mem = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo bci;
        memset(&bci, 0, sizeof(bci));
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = (VkDeviceSize)W * H * 4;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(g_dev, &bci, NULL, &rb_buf);
        VkMemoryRequirements breqs;
        vkGetBufferMemoryRequirements(g_dev, rb_buf, &breqs);
        wubu_vkd3d_alloc_request_t bar;
        memset(&bar, 0, sizeof(bar));
        bar.size = breqs.size;
        bar.heap_type = VKD3D_HEAP_READBACK;
        bar.type_bits = breqs.memoryTypeBits;
        const char *rwhy = NULL;
        int ridx = wubu_vkd3d_memory_type_index(&g_types, &bar, &rwhy);
        CHECK(ridx >= 0, "readback memory type exists");
        VkMemoryAllocateInfo rb_mai;
        memset(&rb_mai, 0, sizeof(rb_mai));
        rb_mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        rb_mai.allocationSize = breqs.size;
        rb_mai.memoryTypeIndex = (uint32_t)ridx;
        CHECK(vkAllocateMemory(g_dev, &rb_mai, NULL, &rb_mem) == VK_SUCCESS,
              "readback memory allocated");
        vkBindBufferMemory(g_dev, rb_buf, rb_mem, 0);
    }

    VkCommandPool pool = VK_NULL_HANDLE;
    {
        VkCommandPoolCreateInfo cpci;
        memset(&cpci, 0, sizeof(cpci));
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = g_gfx_family;
        vkCreateCommandPool(g_dev, &cpci, NULL, &pool);
    }

    /* -- draw with scale 1.0, then scale 0.5, and CHECK both readbacks -- */
    {
        struct { float scale; int r, g, b; const char *what; } cases[2] = {
            { 1.0f, 230, 51, 26, "scale 1.0 -> base * 1.0" },
            { 0.5f, 115, 26, 13, "scale 0.5 -> base * 0.5 (the PUSH CONSTANT changed)" },
        };
        int ix = W / 2, iy = (int)((1.0 - (-0.1167)) / 2.0 * H);

        for (int c = 0; c < 2; c++) {
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

            VkClearValue clear;
            memset(&clear, 0, sizeof(clear));
            clear.color.float32[0] = 0.0f;
            clear.color.float32[1] = 0.0f;
            clear.color.float32[2] = 0.0f;
            clear.color.float32[3] = 1.0f;

            VkRenderPassBeginInfo rpbi;
            memset(&rpbi, 0, sizeof(rpbi));
            rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpbi.renderPass = renderpass;
            rpbi.framebuffer = framebuffer;
            rpbi.renderArea.offset.x = 0; rpbi.renderArea.offset.y = 0;
            rpbi.renderArea.extent.width = W; rpbi.renderArea.extent.height = H;
            rpbi.clearValueCount = 1; rpbi.pClearValues = &clear;

            vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    layout, 0, 1, &ds, 0, NULL);
            vkCmdPushConstants(cb, layout, pc_stages, pc_off, pc_size,
                               &cases[c].scale);
            vkCmdDraw(cb, 3, 1, 0, 0);
            vkCmdEndRenderPass(cb);

            /* transition + copy to the readback buffer */
            wubu_vkd3d_state_t sb, sa;
            wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_RENDER_TARGET, &sb);
            wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_COPY_SOURCE, &sa);
            VkImageMemoryBarrier b1;
            memset(&b1, 0, sizeof(b1));
            b1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b1.srcAccessMask = sb.access;
            b1.dstAccessMask = sa.access;
            b1.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            b1.image = color_img;
            b1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b1.subresourceRange.levelCount = 1;
            b1.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, NULL, 0, NULL, 1, &b1);

            VkBufferImageCopy bic;
            memset(&bic, 0, sizeof(bic));
            bic.bufferRowLength = W;
            bic.bufferImageHeight = H;
            bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bic.imageSubresource.layerCount = 1;
            bic.imageExtent.width = W; bic.imageExtent.height = H; bic.imageExtent.depth = 1;
            vkCmdCopyImageToBuffer(cb, color_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   rb_buf, 1, &bic);

            VkMemoryBarrier mb;
            memset(&mb, 0, sizeof(mb));
            mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);

            vkEndCommandBuffer(cb);

            VkSubmitInfo si;
            memset(&si, 0, sizeof(si));
            si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            si.commandBufferCount = 1; si.pCommandBuffers = &cb;
            CHECK(vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS,
                  "submitted");
            CHECK(vkQueueWaitIdle(g_queue) == VK_SUCCESS, "GPU completed");

            void *pixels = NULL;
            if (vkMapMemory(g_dev, rb_mem, 0, (VkDeviceSize)W * H * 4, 0, &pixels)
                == VK_SUCCESS) {
                const unsigned char *px = (const unsigned char *)pixels;
                unsigned r = px[(iy*W+ix)*4+0];
                unsigned g = px[(iy*W+ix)*4+1];
                unsigned b = px[(iy*W+ix)*4+2];
                printf("  %s: pixel (%d,%d) = %u %u %u\n",
                       cases[c].what, ix, iy, r, g, b);
                #define CLOSE(a,bb) ((int)(a) >= (bb)-6 && (int)(a) <= (bb)+6)
                CHECK(CLOSE(r, cases[c].r) && CLOSE(g, cases[c].g) &&
                      CLOSE(b, cases[c].b),
                      "the rendered pixel matches base * scale");
                #undef CLOSE
                vkUnmapMemory(g_dev, rb_mem);
            }
            vkFreeCommandBuffers(g_dev, pool, 1, &cb);
        }
    }

    /* -- teardown -- */
    vkDestroyCommandPool(g_dev, pool, NULL);
    vkDestroyDescriptorPool(g_dev, g_pool, NULL);
    vkDestroyBuffer(g_dev, rb_buf, NULL);
    vkFreeMemory(g_dev, rb_mem, NULL);
    vkDestroyPipeline(g_dev, pipeline, NULL);
    vkDestroyPipelineLayout(g_dev, layout, NULL);
    vkDestroyDescriptorSetLayout(g_dev, dsl, NULL);
    vkDestroyShaderModule(g_dev, vs, NULL);
    vkDestroyShaderModule(g_dev, fs, NULL);
    vkDestroyFramebuffer(g_dev, framebuffer, NULL);
    vkDestroyRenderPass(g_dev, renderpass, NULL);
    vkDestroyImageView(g_dev, color_view, NULL);
    vkDestroyImage(g_dev, color_img, NULL);
    vkFreeMemory(g_dev, color_mem, NULL);
    vkDestroyBuffer(g_dev, ubo, NULL);
    vkFreeMemory(g_dev, ubo_mem, NULL);
    vkDestroyDevice(g_dev, NULL);
    vkDestroyInstance(g_inst, NULL);
    free(vw); free(fw);

    if (failures == 0) printf("ALL VKD3D_LIVE_BINDING TESTS PASSED\n");
    else printf("%d VKD3D_LIVE_BINDING FAILURES\n", failures);
    return failures ? 1 : 0;
}
