/*
 * wubu_vkd3d_live_msaa_test.c -- VKD3D: real 4x MSAA with an explicit resolve.
 *
 * A 4x multisampled colour target is rendered into, then RESOLVED into a
 * single-sampled image and read back.  The triangle must still land at the
 * computed coordinates -- proving the sample counts agree across the pipeline
 * and both attachments, and that the resolve worked.
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
#include "wubu_vkd3d_msaa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 1920
#define H 1080
#define SAMPLES 4
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
    if (got != (size_t)sz || buf[0] != 0x07230203u) { free(buf); return false; }
    *words = buf; *n = (size_t)sz / 4;
    return true;
}

static int memory_type(VkDeviceSize size, uint32_t bits,
                       wubu_vkd3d_heap_type_t heap, const char **why)
{
    wubu_vkd3d_alloc_request_t ar;
    memset(&ar, 0, sizeof(ar));
    ar.size = size; ar.heap_type = heap; ar.type_bits = bits;
    return wubu_vkd3d_memory_type_index(&g_types, &ar, why);
}

static bool make_image(VkFormat format, VkSampleCountFlagBits samples,
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
    ici.samples = samples;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(g_dev, &ici, NULL, image) != VK_SUCCESS) return false;

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(g_dev, *image, &reqs);
    const char *why = NULL;
    int idx = memory_type(reqs.size, reqs.memoryTypeBits, VKD3D_HEAP_DEFAULT, &why);
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
    printf("=== test_vkd3d_live_msaa (%dx + resolve) ===\n", SAMPLES);

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wubu-vkd3d-live-msaa";
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

    /* -- the sample description, mapped by slice 14 -- */
    wubu_vkd3d_sample_desc_t sd;
    sd.count = SAMPLES;
    sd.quality = 0;
    wubu_vkd3d_msaa_vk_t mvk;
    CHECK(wubu_vkd3d_msaa_vk(&sd, &mvk) && mvk.valid, "the sample desc maps");
    CHECK(mvk.rasterization_samples == VK_SAMPLE_COUNT_4_BIT, "4x -> SAMPLE_COUNT_4");
    CHECK(mvk.needs_resolve, "4x needs a resolve");
    CHECK(wubu_vkd3d_sample_counts_compatible(SAMPLES, SAMPLES),
          "the pipeline and the attachment agree");

    /* -- the 4x colour target and the 1x resolve target -- */
    VkImage ms_img = VK_NULL_HANDLE, rs_img = VK_NULL_HANDLE;
    VkDeviceMemory ms_mem = VK_NULL_HANDLE, rs_mem = VK_NULL_HANDLE;
    VkImageView ms_view = VK_NULL_HANDLE, rs_view = VK_NULL_HANDLE;
    CHECK(make_image(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_4_BIT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                     &ms_img, &ms_mem, &ms_view),
          "the 4x multisampled target is created");
    CHECK(make_image(VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     &rs_img, &rs_mem, &rs_view),
          "the 1x resolve target is created");

    /* -- the render pass: 4x colour + 1x resolve -- */
    wubu_vkd3d_renderpass_desc_t rp;
    memset(&rp, 0, sizeof(rp));
    rp.attachment_count = 2;
    rp.attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    rp.attachments[0].samples = SAMPLES;
    rp.attachments[0].begin = VKD3D_BEGIN_CLEAR;
    rp.attachments[0].end = VKD3D_END_PRESERVE;
    rp.attachments[0].initial_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    rp.attachments[0].final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    rp.attachments[1].format = VK_FORMAT_R8G8B8A8_UNORM;
    rp.attachments[1].samples = 1;
    rp.attachments[1].begin = VKD3D_BEGIN_NO_ACCESS;
    rp.attachments[1].end = VKD3D_END_PRESERVE;
    rp.attachments[1].initial_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    rp.attachments[1].final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription ad[8];
    int nad = wubu_vkd3d_renderpass_attachments(&rp, ad, 8);
    CHECK(nad == 2, "both attachments translate");

    VkAttachmentReference color_ref;
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference resolve_ref;
    resolve_ref.attachment = 1;
    resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub;
    memset(&sub, 0, sizeof(sub));
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &color_ref;
    sub.pResolveAttachments = &resolve_ref;      /* THE RESOLVE */

    VkRenderPassCreateInfo rpci;
    memset(&rpci, 0, sizeof(rpci));
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = (uint32_t)nad;
    rpci.pAttachments = ad;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;

    VkRenderPass renderpass = VK_NULL_HANDLE;
    CHECK(vkCreateRenderPass(g_dev, &rpci, NULL, &renderpass) == VK_SUCCESS,
          "the render pass with a resolve attachment is created");

    /* the framebuffer holds BOTH views */
    VkImageView views[2] = { ms_view, rs_view };
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    {
        VkFramebufferCreateInfo fbci;
        memset(&fbci, 0, sizeof(fbci));
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = renderpass;
        fbci.attachmentCount = 2;
        fbci.pAttachments = views;
        fbci.width = W; fbci.height = H; fbci.layers = 1;
        CHECK(vkCreateFramebuffer(g_dev, &fbci, NULL, &framebuffer) == VK_SUCCESS,
              "the framebuffer with both attachments is created");
    }

    /* -- the pipeline, with 4x rasterization -- */
    uint32_t *vw = NULL, *fw = NULL;
    size_t vn = 0, fn = 0;
    char path[256];
    snprintf(path, sizeof(path), "%s/triangle.vert.spv", SHADER_DIR);
    CHECK(load_spirv(path, &vw, &vn), "vertex SPIR-V loaded");
    snprintf(path, sizeof(path), "%s/triangle.frag.spv", SHADER_DIR);
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

    wubu_vkd3d_pso_desc_t pd;
    memset(&pd, 0, sizeof(pd));
    pd.vs_hash = 0xA11CE; pd.ps_hash = 0xBEEF;
    pd.fill_mode = 2; pd.cull_mode = 2;
    pd.primitive_topology = 4;
    pd.rtv_count = 1; pd.rtv_formats[0] = 41;
    pd.sample_count = SAMPLES;      /* must equal the attachment */
    pd.sample_mask = 0xFFFFFFFF;
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
    viewport.width = (float)W; viewport.height = (float)H; viewport.maxDepth = 1.0f;
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
    msaa.rasterizationSamples = mvk.rasterization_samples;   /* 4x */
    msaa.sampleShadingEnable = mvk.sample_shading_enable;
    msaa.minSampleShading = mvk.min_sample_shading;

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
    gpci.layout = VK_NULL_HANDLE;
    gpci.renderPass = renderpass;
    gpci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    CHECK(vkCreateGraphicsPipelines(g_dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline)
          == VK_SUCCESS && pipeline != VK_NULL_HANDLE,
          "a 4x pipeline is created");

    /* -- readback -- */
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
        const char *why = NULL;
        int ridx = memory_type(breqs.size, breqs.memoryTypeBits,
                               VKD3D_HEAP_READBACK, &why);
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

        VkClearValue clear;
        memset(&clear, 0, sizeof(clear));
        clear.color.float32[0] = 0.05f;
        clear.color.float32[1] = 0.05f;
        clear.color.float32[2] = 0.10f;
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
        vkCmdDraw(cb, 3, 1, 0, 0);
        vkCmdEndRenderPass(cb);

        /* the resolve target is now single-sampled: copy it out */
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
        b1.image = rs_img;
        b1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b1.subresourceRange.levelCount = 1;
        b1.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, NULL, 0, NULL, 1, &b1);

        VkBufferImageCopy bic;
        memset(&bic, 0, sizeof(bic));
        bic.bufferRowLength = W; bic.bufferImageHeight = H;
        bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bic.imageSubresource.layerCount = 1;
        bic.imageExtent.width = W; bic.imageExtent.height = H; bic.imageExtent.depth = 1;
        vkCmdCopyImageToBuffer(cb, rs_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
        CHECK(vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS, "submitted");
        CHECK(vkQueueWaitIdle(g_queue) == VK_SUCCESS, "GPU completed the resolve");

        void *out = NULL;
        CHECK(vkMapMemory(g_dev, rb_mem, 0, (VkDeviceSize)W * H * 4, 0, &out)
              == VK_SUCCESS, "the readback buffer is mapped");
        const unsigned char *px = (const unsigned char *)out;

        int ix = W / 2, iy = (int)((1.0 - (-0.1167)) / 2.0 * H);
        int ox = (int)((1.0 + (-0.90)) / 2.0 * W);
        int oy = (int)((1.0 - 0.90) / 2.0 * H);
        printf("  inside  (%4d,%4d) = %3u %3u %3u\n", ix, iy,
               px[(iy*W+ix)*4+0], px[(iy*W+ix)*4+1], px[(iy*W+ix)*4+2]);
        printf("  outside (%4d,%4d) = %3u %3u %3u\n", ox, oy,
               px[(oy*W+ox)*4+0], px[(oy*W+ox)*4+1], px[(oy*W+ox)*4+2]);

        #define CLOSE(a,bb) ((int)(a) >= (bb)-10 && (int)(a) <= (bb)+10)
        CHECK(CLOSE(px[(iy*W+ix)*4+0], 64) &&
              CLOSE(px[(iy*W+ix)*4+1], 166) &&
              CLOSE(px[(iy*W+ix)*4+2], 255),
              "after the resolve, the triangle pixel still has its colour");
        CHECK(CLOSE(px[(oy*W+ox)*4+0], 13) &&
              CLOSE(px[(oy*W+ox)*4+1], 13) &&
              CLOSE(px[(oy*W+ox)*4+2], 26),
              "and the background still has the clear colour");
        #undef CLOSE
        vkUnmapMemory(g_dev, rb_mem);
        vkDestroyCommandPool(g_dev, pool, NULL);
    }

    /* -- teardown -- */
    vkDestroyBuffer(g_dev, rb_buf, NULL);
    vkFreeMemory(g_dev, rb_mem, NULL);
    vkDestroyPipeline(g_dev, pipeline, NULL);
    vkDestroyFramebuffer(g_dev, framebuffer, NULL);
    vkDestroyRenderPass(g_dev, renderpass, NULL);
    vkDestroyShaderModule(g_dev, vs, NULL);
    vkDestroyShaderModule(g_dev, fs, NULL);
    vkDestroyImageView(g_dev, ms_view, NULL);
    vkDestroyImageView(g_dev, rs_view, NULL);
    vkDestroyImage(g_dev, ms_img, NULL);
    vkDestroyImage(g_dev, rs_img, NULL);
    vkFreeMemory(g_dev, ms_mem, NULL);
    vkFreeMemory(g_dev, rs_mem, NULL);
    vkDestroyDevice(g_dev, NULL);
    vkDestroyInstance(g_inst, NULL);
    free(vw); free(fw);

    if (failures == 0) printf("ALL VKD3D_LIVE_MSAA TESTS PASSED\n");
    else printf("%d VKD3D_LIVE_MSAA FAILURES\n", failures);
    return failures ? 1 : 0;
}
