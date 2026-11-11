/*
 * wubu_vkd3d_live_pipeline_test.c -- VKD3D: a REAL graphics pipeline + draw.
 *
 * The live_render test cleared an empty render pass, which exercised every
 * object but drew nothing.  This one loads real SPIR-V, builds a graphics
 * pipeline FROM THE PSO SLICE's mapped state, and draws a triangle.
 *
 * GPU-requiring: a separate target, not part of the gate.
 */
#include "wubu_vkd3d.h"             /* the D3D12 state enum + state_map */
#include "wubu_vkd3d_physdev.h"
#include "wubu_vkd3d_vkdev.h"
#include "wubu_vkd3d_memory.h"
#include "wubu_vkd3d_renderpass.h"
#include "wubu_vkd3d_framebuffer.h"
#include "wubu_vkd3d_pso.h"

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
    if (got != (size_t)sz) { free(buf); return false; }
    /* SPIR-V is little-endian words; the magic must be 0x07230203. */
    if (buf[0] != 0x07230203u) { free(buf); return false; }
    *words = buf;
    *n = (size_t)sz / 4;
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
    if (idx < 0) { printf("  no memory type: %s\n", why ? why : "?"); return false; }

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
    printf("=== test_vkd3d_live_pipeline (real draw call) ===\n");

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wubu-vkd3d-live-pipeline";
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

    /* -- images -- */
    VkImage color_img = VK_NULL_HANDLE, depth_img = VK_NULL_HANDLE;
    VkDeviceMemory color_mem = VK_NULL_HANDLE, depth_mem = VK_NULL_HANDLE;
    VkImageView color_view = VK_NULL_HANDLE, depth_view = VK_NULL_HANDLE;
    CHECK(make_attachment(VK_FORMAT_R8G8B8A8_UNORM, false,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                          &color_img, &color_mem, &color_view),
          "colour attachment created");
    CHECK(make_attachment(VK_FORMAT_D32_SFLOAT, true,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                          &depth_img, &depth_mem, &depth_view),
          "depth attachment created");

    /* -- render pass + framebuffer (slices 10 and 11) -- */
    wubu_vkd3d_renderpass_desc_t rp;
    memset(&rp, 0, sizeof(rp));
    rp.attachment_count = 2;
    rp.attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    rp.attachments[0].samples = 1;
    rp.attachments[0].begin = VKD3D_BEGIN_CLEAR;
    rp.attachments[0].end = VKD3D_END_PRESERVE;
    rp.attachments[0].initial_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    rp.attachments[0].final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    rp.attachments[1].format = VK_FORMAT_D32_SFLOAT;
    rp.attachments[1].samples = 1;
    rp.attachments[1].begin = VKD3D_BEGIN_CLEAR;
    rp.attachments[1].end = VKD3D_END_PRESERVE;
    rp.attachments[1].initial_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    rp.attachments[1].final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    rp.has_depth_stencil = true;
    rp.depth_stencil_attachment = 1;

    VkAttachmentDescription ad[8];
    int nad = wubu_vkd3d_renderpass_attachments(&rp, ad, 8);
    VkAttachmentReference crefs[8];
    int ncref = wubu_vkd3d_renderpass_color_refs(&rp, crefs, 8);
    VkAttachmentReference dref;
    int ndref = wubu_vkd3d_renderpass_depth_ref(&rp, &dref);

    VkSubpassDescription sub;
    memset(&sub, 0, sizeof(sub));
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = (uint32_t)ncref;
    sub.pColorAttachments = crefs;
    sub.pDepthStencilAttachment = (ndref >= 0) ? &dref : NULL;

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
    fb.attachment_count = 2;
    fb.attachments[0].view = color_view; fb.attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    fb.attachments[0].width = W; fb.attachments[0].height = H; fb.attachments[0].samples = 1;
    fb.attachments[0].is_depth_stencil = false;
    fb.attachments[1].view = depth_view; fb.attachments[1].format = VK_FORMAT_D32_SFLOAT;
    fb.attachments[1].width = W; fb.attachments[1].height = H; fb.attachments[1].samples = 1;
    fb.attachments[1].is_depth_stencil = true;
    fb.width = W; fb.height = H; fb.layers = 1;

    VkFramebufferCreateInfo fbci;
    CHECK(wubu_vkd3d_framebuffer_create_info(&fb, &fbci), "framebuffer translates");
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    CHECK(vkCreateFramebuffer(g_dev, &fbci, NULL, &framebuffer) == VK_SUCCESS,
          "framebuffer created");

    /* -- a READBACK buffer: the first live use of slice 12's READBACK heap -- */
    VkBuffer rb_buf = VK_NULL_HANDLE;
    VkDeviceMemory rb_mem = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo bci;
        memset(&bci, 0, sizeof(bci));
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = (VkDeviceSize)W * H * 4;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CHECK(vkCreateBuffer(g_dev, &bci, NULL, &rb_buf) == VK_SUCCESS,
              "readback buffer created");

        VkMemoryRequirements breqs;
        vkGetBufferMemoryRequirements(g_dev, rb_buf, &breqs);
        wubu_vkd3d_alloc_request_t bar;
        memset(&bar, 0, sizeof(bar));
        bar.size = breqs.size;
        bar.heap_type = VKD3D_HEAP_READBACK;
        bar.type_bits = breqs.memoryTypeBits;
        const char *rwhy = NULL;
        int ridx = wubu_vkd3d_memory_type_index(&g_types, &bar, &rwhy);
        CHECK(ridx >= 0, "a readback memory type exists");
        VkMemoryAllocateInfo rb_mai;
        memset(&rb_mai, 0, sizeof(rb_mai));
        rb_mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        rb_mai.allocationSize = breqs.size;
        rb_mai.memoryTypeIndex = (uint32_t)ridx;
        CHECK(vkAllocateMemory(g_dev, &rb_mai, NULL, &rb_mem) == VK_SUCCESS,
              "readback memory allocated");
        CHECK(vkBindBufferMemory(g_dev, rb_buf, rb_mem, 0) == VK_SUCCESS,
              "readback memory bound");
        printf("  readback buffer: %llu bytes, memory type %d (host-visible)\n",
               (unsigned long long)breqs.size, ridx);
    }

    /* -- load the REAL SPIR-V -- */
    uint32_t *vw = NULL, *fw = NULL;
    size_t vn = 0, fn = 0;
    char path[256];
    snprintf(path, sizeof(path), "%s/triangle.vert.spv", SHADER_DIR);
    CHECK(load_spirv(path, &vw, &vn), "vertex SPIR-V loaded and magic-checked");
    snprintf(path, sizeof(path), "%s/triangle.frag.spv", SHADER_DIR);
    CHECK(load_spirv(path, &fw, &fn), "fragment SPIR-V loaded and magic-checked");

    VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
    if (vw) {
        VkShaderModuleCreateInfo sm;
        memset(&sm, 0, sizeof(sm));
        sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        sm.codeSize = vn * 4;
        sm.pCode = vw;
        CHECK(vkCreateShaderModule(g_dev, &sm, NULL, &vs) == VK_SUCCESS,
              "vertex shader module created");
    }
    if (fw) {
        VkShaderModuleCreateInfo sm;
        memset(&sm, 0, sizeof(sm));
        sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        sm.codeSize = fn * 4;
        sm.pCode = fw;
        CHECK(vkCreateShaderModule(g_dev, &sm, NULL, &fs) == VK_SUCCESS,
              "fragment shader module created");
    }

    /* -- build the pipeline FROM THE PSO SLICE -- */
    wubu_vkd3d_pso_desc_t pd;
    memset(&pd, 0, sizeof(pd));
    pd.vs_hash = 0xA11CE;
    pd.ps_hash = 0xBEEF;
    pd.fill_mode = 2;             /* SOLID */
    pd.cull_mode = 2;             /* BACK */
    pd.front_counter_clockwise = false;
    pd.depth_enable = true;
    pd.depth_write_enable = true;
    pd.depth_func = 4;            /* LESS_EQUAL */
    pd.primitive_topology = 4;    /* TRIANGLE */
    pd.rtv_count = 1;
    pd.rtv_formats[0] = 41;       /* R8G8B8A8_UNORM */
    pd.dsv_format = 2;            /* D32_SFLOAT */
    pd.sample_count = 1;
    pd.sample_mask = 0xFFFFFFFF;
    pd.blend_src_color = 5; pd.blend_dst_color = 6; pd.blend_op_color = 1;
    pd.blend_src_alpha = 5; pd.blend_dst_alpha = 6; pd.blend_op_alpha = 1;

    wubu_vkd3d_pso_vk_t pv;
    CHECK(wubu_vkd3d_pso_vk(&pd, &pv) && pv.valid, "the PSO slice maps the desc");

    VkPipelineShaderStageCreateInfo stages[2];
    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vinput;
    memset(&vinput, 0, sizeof(vinput));
    vinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    /* no vertex buffers: the shader uses gl_VertexIndex */

    VkPipelineInputAssemblyStateCreateInfo iasm;
    memset(&iasm, 0, sizeof(iasm));
    iasm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iasm.topology = pv.topology;
    iasm.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport;
    memset(&viewport, 0, sizeof(viewport));
    viewport.x = 0; viewport.y = 0;
    viewport.width = (float)W; viewport.height = (float)H;
    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
    VkRect2D scissor;
    memset(&scissor, 0, sizeof(scissor));
    scissor.offset.x = 0; scissor.offset.y = 0;
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
    raster.depthClampEnable = VK_FALSE;   /* D3D12 calls this depthClipEnable */

    VkPipelineMultisampleStateCreateInfo msaa;
    memset(&msaa, 0, sizeof(msaa));
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msaa.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo dss;
    memset(&dss, 0, sizeof(dss));
    dss.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dss.depthTestEnable = pd.depth_enable ? VK_TRUE : VK_FALSE;
    dss.depthWriteEnable = pd.depth_write_enable ? VK_TRUE : VK_FALSE;
    dss.depthCompareOp = pv.depth_func;

    VkPipelineColorBlendAttachmentState cba;
    memset(&cba, 0, sizeof(cba));
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbs.attachmentCount = 1;
    cbs.pAttachments = &cba;

    VkGraphicsPipelineCreateInfo gpci;
    memset(&gpci, 0, sizeof(gpci));
    gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vinput;
    gpci.pInputAssemblyState = &iasm;
    gpci.pViewportState = &vport;
    gpci.pRasterizationState = &raster;
    gpci.pMultisampleState = &msaa;
    gpci.pDepthStencilState = &dss;
    gpci.pColorBlendState = &cbs;
    gpci.layout = VK_NULL_HANDLE;   /* no descriptors: the shader uses none */
    gpci.renderPass = renderpass;
    gpci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    CHECK(vkCreateGraphicsPipelines(g_dev, VK_NULL_HANDLE, 1, &gpci, NULL,
                                    &pipeline) == VK_SUCCESS &&
          pipeline != VK_NULL_HANDLE,
          "a REAL VkPipeline is created from the PSO slice's mapped state");

    /* -- record and EXECUTE a real draw -- */
    {
        VkCommandPoolCreateInfo cpci;
        memset(&cpci, 0, sizeof(cpci));
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = g_gfx_family;
        VkCommandPool pool = VK_NULL_HANDLE;
        CHECK(vkCreateCommandPool(g_dev, &cpci, NULL, &pool) == VK_SUCCESS,
              "command pool created");

        VkCommandBufferAllocateInfo cbai;
        memset(&cbai, 0, sizeof(cbai));
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = pool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        CHECK(vkAllocateCommandBuffers(g_dev, &cbai, &cb) == VK_SUCCESS,
              "command buffer allocated");

        VkCommandBufferBeginInfo cbbi;
        memset(&cbbi, 0, sizeof(cbbi));
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CHECK(vkBeginCommandBuffer(cb, &cbbi) == VK_SUCCESS, "command buffer begins");

        VkClearValue clears[2];
        memset(clears, 0, sizeof(clears));
        clears[0].color.float32[0] = 0.05f;
        clears[0].color.float32[1] = 0.05f;
        clears[0].color.float32[2] = 0.10f;
        clears[0].color.float32[3] = 1.0f;
        clears[1].depthStencil.depth = 1.0f;

        VkRenderPassBeginInfo rpbi;
        memset(&rpbi, 0, sizeof(rpbi));
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = renderpass;
        rpbi.framebuffer = framebuffer;
        rpbi.renderArea.offset.x = 0; rpbi.renderArea.offset.y = 0;
        rpbi.renderArea.extent.width = W; rpbi.renderArea.extent.height = H;
        rpbi.clearValueCount = 2;
        rpbi.pClearValues = clears;

        vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(cb, 3, 1, 0, 0);          /* THE DRAW CALL */
        vkCmdEndRenderPass(cb);

        /* -- transition the colour image to a copy source, using slice 1 -- */
        {
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
            vkCmdPipelineBarrier(cb,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, NULL, 0, NULL, 1, &b1);

            VkBufferImageCopy bic;
            memset(&bic, 0, sizeof(bic));
            bic.bufferRowLength = W;
            bic.bufferImageHeight = H;
            bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bic.imageSubresource.layerCount = 1;
            bic.imageExtent.width = W;
            bic.imageExtent.height = H;
            bic.imageExtent.depth = 1;
            vkCmdCopyImageToBuffer(cb, color_img,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   rb_buf, 1, &bic);

            /* make the GPU's writes visible to the host before we read */
            VkMemoryBarrier mb;
            memset(&mb, 0, sizeof(mb));
            mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT,
                                 0, 1, &mb, 0, NULL, 0, NULL);
        }
        CHECK(vkEndCommandBuffer(cb) == VK_SUCCESS, "command buffer recorded");

        VkSubmitInfo si;
        memset(&si, 0, sizeof(si));
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        CHECK(vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS,
              "the draw is SUBMITTED to the GPU");
        CHECK(vkQueueWaitIdle(g_queue) == VK_SUCCESS,
              "the GPU COMPLETES the draw without error");
        printf("  DRAW EXECUTED: 3 vertices, 1 instance, real pipeline\n");

        /* -- READ THE PIXELS BACK and prove the triangle is where it should be -- */
        void *pixels = NULL;
        CHECK(vkMapMemory(g_dev, rb_mem, 0, (VkDeviceSize)W * H * 4, 0, &pixels)
              == VK_SUCCESS, "the readback buffer is mapped");
        const unsigned char *px = (const unsigned char *)pixels;

        /* NDC -> framebuffer: x=(nx+1)/2*W, y=(1-ny)/2*H (Vulkan's y points down).
         * Triangle centroid is NDC (0, -0.1167); the apex is NDC (0, 0.55). */
        int ix = W / 2, iy = (int)((1.0 - (-0.1167)) / 2.0 * H);
        int ox = (int)((1.0 + (-0.90)) / 2.0 * W);
        int oy = (int)((1.0 - 0.90) / 2.0 * H);
        printf("  inside pixel (%d,%d): %u %u %u\n", ix, iy,
               px[(iy*W+ix)*4+0], px[(iy*W+ix)*4+1], px[(iy*W+ix)*4+2]);
        printf("  outside pixel (%d,%d): %u %u %u\n", ox, oy,
               px[(oy*W+ox)*4+0], px[(oy*W+ox)*4+1], px[(oy*W+ox)*4+2]);

        /* triangle colour (0.25,0.65,1.0) -> (64,166,255); clear (0.05,0.05,0.10) */
        #define CLOSE(a,b) ((a) >= (b)-10 && (a) <= (b)+10)
        CHECK(CLOSE(px[(iy*W+ix)*4+0], 64) &&
              CLOSE(px[(iy*W+ix)*4+1], 166) &&
              CLOSE(px[(iy*W+ix)*4+2], 255),
              "the pixel INSIDE the triangle has the triangle's colour");
        CHECK(CLOSE(px[(oy*W+ox)*4+0], 13) &&
              CLOSE(px[(oy*W+ox)*4+1], 13) &&
              CLOSE(px[(oy*W+ox)*4+2], 26),
              "the pixel OUTSIDE the triangle still has the clear colour");
        #undef CLOSE

        vkUnmapMemory(g_dev, rb_mem);
        vkDestroyCommandPool(g_dev, pool, NULL);
    }

    /* -- teardown -- */
    vkDestroyBuffer(g_dev, rb_buf, NULL);
    vkFreeMemory(g_dev, rb_mem, NULL);
    vkDestroyPipeline(g_dev, pipeline, NULL);
    vkDestroyShaderModule(g_dev, vs, NULL);
    vkDestroyShaderModule(g_dev, fs, NULL);
    vkDestroyFramebuffer(g_dev, framebuffer, NULL);
    vkDestroyRenderPass(g_dev, renderpass, NULL);
    vkDestroyImageView(g_dev, color_view, NULL);
    vkDestroyImageView(g_dev, depth_view, NULL);
    vkDestroyImage(g_dev, color_img, NULL);
    vkDestroyImage(g_dev, depth_img, NULL);
    vkFreeMemory(g_dev, color_mem, NULL);
    vkFreeMemory(g_dev, depth_mem, NULL);
    vkDestroyDevice(g_dev, NULL);
    vkDestroyInstance(g_inst, NULL);
    free(vw);
    free(fw);

    if (failures == 0) printf("ALL VKD3D_LIVE_PIPELINE TESTS PASSED\n");
    else printf("%d VKD3D_LIVE_PIPELINE FAILURES\n", failures);
    return failures ? 1 : 0;
}
