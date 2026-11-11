/*
 * wubu_vkd3d_live_present_test.c -- VKD3D: a REAL swapchain and present.
 *
 * The last VKD3D gap.  Every earlier live target rendered into an image it
 * owned; this one renders into a REAL SWAPCHAIN IMAGE and PRESENTS it, using
 * VK_EXT_headless_surface so it needs no window.
 *
 * The chain exercises the slice-7 mapping end to end: swap effect ->
 * present mode, sync interval -> present mode, frame latency -> image count.
 * GPU-requiring: a separate target, not part of the gate.
 */
#include "wubu_vkd3d.h"
#include "wubu_vkd3d_physdev.h"
#include "wubu_vkd3d_vkdev.h"
#include "wubu_vkd3d_memory.h"
#include "wubu_vkd3d_renderpass.h"
#include "wubu_vkd3d_framebuffer.h"
#include "wubu_vkd3d_pso.h"
#include "wubu_vkd3d_swapchain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 512
#define H 512
#define SHADER_DIR "build/vkd3d_shaders"

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static VkInstance       g_inst;
static VkPhysicalDevice g_phys;
static VkDevice         g_dev;
static uint32_t         g_family;
static VkQueue          g_queue;
static VkPhysicalDeviceMemoryProperties g_memprops;
static wubu_vkd3d_memory_types_t g_types;

static bool load_spirv(const char *path, uint32_t **w, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || (sz % 4)) { fclose(f); return false; }
    uint32_t *b = malloc((size_t)sz);
    if (!b) { fclose(f); return false; }
    size_t got = fread(b, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz || b[0] != 0x07230203u) { free(b); return false; }
    *w = b; *n = (size_t)sz / 4;
    return true;
}

int main(void)
{
    printf("=== test_vkd3d_live_present (real swapchain + present) ===\n");

    /* -- instance with the headless surface extension -- */
    const char *exts[] = { "VK_EXT_headless_surface", "VK_KHR_surface" };
    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wubu-vkd3d-live-present";
    app.apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);
    VkInstanceCreateInfo ici;
    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = exts;
    if (vkCreateInstance(&ici, NULL, &g_inst) != VK_SUCCESS) {
        printf("  SKIP: no Vulkan loader\n");
        return 0;
    }

    wubu_vkd3d_physdev_t devs[8];
    int n = wubu_vkd3d_physdev_enumerate(g_inst, devs, 8);
    /* A PRESENT path must pick a device whose queue family can present, which
     * is a different primary filter from the RENDER tests (that prefer the
     * discrete GPU).
     *
     * Measured on this box: the NVIDIA ICD SEGFAULTS inside
     * vkGetPhysicalDeviceSurfaceSupportKHR for a headless surface, while the
     * Intel iGPU reports present=1.  Querying the NVIDIA device at all is
     * therefore not survivable, so the integrated GPU is preferred here and a
     * discrete one is only used when nothing else exists.  That is a workaround
     * for a specific driver, and it is recorded as one. */
    int chosen = -1;
    for (int i = 0; i < n; i++) {
        if (devs[i].properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            chosen = i; break;
        }
    }
    if (chosen < 0) {
        for (int i = 0; i < n; i++) {
            if (devs[i].properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                chosen = i; break;
            }
            if (chosen < 0) chosen = i;
        }
    }
    if (chosen < 0) { printf("  SKIP: no device\n"); return 0; }
    g_phys = devs[chosen].device;
    printf("  device: %s\n", devs[chosen].properties.deviceName);
    for (uint32_t q = 0; q < devs[chosen].queue_family_count; q++) {
        if (devs[chosen].queue_families[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            g_family = q; break;
        }
    }

    wubu_vkd3d_vkdev_desc_t dd;
    memset(&dd, 0, sizeof(dd));
    dd.queue_request_count = 1;
    dd.queues[0].family_index = g_family;
    dd.queues[0].queue_count = 1;
    dd.queues[0].priority = 1.0f;
    /* VK_KHR_swapchain is a DEVICE extension: without it
     * vkCreateSwapchainKHR has no function pointer, and the loader returns
     * VK_SUCCESS from a NULL -- so the failure appears LATER and somewhere
     * else entirely. */
    dd.extension_count = 1;
    dd.extensions[0] = "VK_KHR_swapchain";
    CHECK(wubu_vkd3d_vkdev_create(&devs[chosen], &dd, &g_dev), "logical device created");
    vkGetDeviceQueue(g_dev, g_family, 0, &g_queue);

    memset(&g_types, 0, sizeof(g_types));
    vkGetPhysicalDeviceMemoryProperties(g_phys, &g_memprops);
    g_types.count = g_memprops.memoryTypeCount;
    for (uint32_t i = 0; i < g_types.count && i < VKD3D_MAX_MEMORY_TYPES; i++)
        g_types.types[i] = g_memprops.memoryTypes[i];

    /* -- a headless surface -- */
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    {
        /* vkCreateHeadlessSurfaceEXT is an EXTENSION function: it is NOT
         * exported by the loader, so calling it directly jumps to an
         * unresolved address.  It must be loaded with vkGetInstanceProcAddr --
         * and silently skipping that is how a segfault inside the driver
         * happens instead of a clean link error. */
        PFN_vkCreateHeadlessSurfaceEXT pfn =
            (PFN_vkCreateHeadlessSurfaceEXT)
            vkGetInstanceProcAddr(g_inst, "vkCreateHeadlessSurfaceEXT");
        CHECK(pfn != NULL, "vkCreateHeadlessSurfaceEXT is loadable");
        if (pfn) {
            VkHeadlessSurfaceCreateInfoEXT hci;
            memset(&hci, 0, sizeof(hci));
            hci.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
            CHECK(pfn(g_inst, &hci, NULL, &surface) == VK_SUCCESS,
                  "a HEADLESS surface is created");
        }
    }

    /* -- the present-capable queue must actually support this surface -- */
    {
        VkBool32 supported = VK_FALSE;
        CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(g_phys, g_family, surface, &supported)
              == VK_SUCCESS && supported == VK_TRUE,
              "the queue family can PRESENT to this surface");
    }

    /* -- the slice-7 mapping: swap effect + sync interval -> present mode -- */
    wubu_vkd3d_swapchain_desc_t sd;
    memset(&sd, 0, sizeof(sd));
    sd.width = W; sd.height = H;
    sd.buffer_count = 3;
    sd.effect = VKD3D_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sd.sync_interval = 1;             /* vsync -> FIFO */
    sd.max_frame_latency = 2;
    CHECK(wubu_vkd3d_swapchain_validate(&sd) == NULL, "the swapchain desc validates");

    wubu_vkd3d_swapchain_vk_t svk;
    CHECK(wubu_vkd3d_swapchain_vk(&sd, &svk) && svk.valid, "the slice-7 mapping applies");
    CHECK(svk.present_mode == VK_PRESENT_MODE_FIFO_KHR,
          "FLIP_SEQUENTIAL + interval 1 -> FIFO (vsync)");
    CHECK(svk.create_info.minImageCount == 3,
          "a frame latency of 2 -> at least 3 images");

    /* -- the swapchain -- */
    VkSurfaceCapabilitiesKHR caps;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_phys, surface, &caps)
          == VK_SUCCESS, "the surface capabilities are queried");

    /* The FORMAT comes from the surface formats, not the capabilities. */
    VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;
    uint32_t nfmt = 0;
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(g_phys, surface, &nfmt, NULL)
          == VK_SUCCESS && nfmt > 0, "the surface formats are enumerated");
    {
        VkSurfaceFormatKHR *fmts = (VkSurfaceFormatKHR *)
            malloc(sizeof(VkSurfaceFormatKHR) * nfmt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(g_phys, surface, &nfmt, fmts);
        /* prefer an sRGB RGBA8 format; the first is a safe fallback */
        for (uint32_t i = 0; i < nfmt; i++) {
            if (fmts[i].format == VK_FORMAT_R8G8B8A8_SRGB ||
                fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
                fmt = fmts[i].format;
                break;
            }
            if (i == 0) fmt = fmts[0].format;
        }
        free(fmts);
    }

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    {
        VkSwapchainCreateInfoKHR sci;
        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sci.surface = surface;
        sci.minImageCount = svk.create_info.minImageCount;
        sci.imageFormat = fmt;
        sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        sci.imageExtent = caps.currentExtent;
        if (sci.imageExtent.width == 0xFFFFFFFFu) {   /* the surface has no fixed size */
            sci.imageExtent.width = W;
            sci.imageExtent.height = H;
        }
        sci.imageArrayLayers = 1;
        sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        sci.preTransform = caps.currentTransform;
        sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode = svk.present_mode;
        sci.clipped = VK_TRUE;
        printf("  swapchain: %ux%u, format %u, %u images, present mode %d\n",
               sci.imageExtent.width, sci.imageExtent.height,
               sci.imageFormat, sci.minImageCount, (int)sci.presentMode);
        CHECK(vkCreateSwapchainKHR(g_dev, &sci, NULL, &swapchain) == VK_SUCCESS,
              "a REAL VkSwapchainKHR is created");
    }

    uint32_t nimg = 0;
    CHECK(vkGetSwapchainImagesKHR(g_dev, swapchain, &nimg, NULL) == VK_SUCCESS
          && nimg >= 3, "the swapchain images are enumerated");

    VkImage *images = (VkImage *)malloc(sizeof(VkImage) * nimg);
    vkGetSwapchainImagesKHR(g_dev, swapchain, &nimg, images);
    printf("  %u swapchain images\n", nimg);

    /* -- a render pass and a framebuffer PER swapchain image -- */
    VkRenderPass renderpass = VK_NULL_HANDLE;
    {
        VkAttachmentDescription ad;
        memset(&ad, 0, sizeof(ad));
        ad.format = fmt;
        ad.samples = VK_SAMPLE_COUNT_1_BIT;
        ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ad.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference ref;
        ref.attachment = 0;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription sub;
        memset(&sub, 0, sizeof(sub));
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &ref;

        VkRenderPassCreateInfo rpci;
        memset(&rpci, 0, sizeof(rpci));
        rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpci.attachmentCount = 1;
        rpci.pAttachments = &ad;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &sub;
        CHECK(vkCreateRenderPass(g_dev, &rpci, NULL, &renderpass) == VK_SUCCESS,
              "a render pass ending in PRESENT_SRC is created");
    }

    VkImageView *views = (VkImageView *)calloc(nimg, sizeof(VkImageView));
    VkFramebuffer *fbs = (VkFramebuffer *)calloc(nimg, sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < nimg; i++) {
        VkImageViewCreateInfo ivci;
        memset(&ivci, 0, sizeof(ivci));
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = images[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = fmt;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.layerCount = 1;
        CHECK(vkCreateImageView(g_dev, &ivci, NULL, &views[i]) == VK_SUCCESS,
              "a swapchain image view is created");

        VkFramebufferCreateInfo fbci;
        memset(&fbci, 0, sizeof(fbci));
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = renderpass;
        fbci.attachmentCount = 1;
        fbci.pAttachments = &views[i];
        fbci.width = W; fbci.height = H; fbci.layers = 1;
        CHECK(vkCreateFramebuffer(g_dev, &fbci, NULL, &fbs[i]) == VK_SUCCESS,
              "a swapchain framebuffer is created");
    }

    /* -- the pipeline -- */
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

    VkViewport vp;
    memset(&vp, 0, sizeof(vp));
    vp.width = (float)W; vp.height = (float)H; vp.maxDepth = 1.0f;
    VkRect2D sc;
    memset(&sc, 0, sizeof(sc));
    sc.extent.width = W; sc.extent.height = H;
    VkPipelineViewportStateCreateInfo vpc;
    memset(&vpc, 0, sizeof(vpc));
    vpc.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpc.viewportCount = 1; vpc.pViewports = &vp;
    vpc.scissorCount = 1; vpc.pScissors = &sc;

    VkPipelineRasterizationStateCreateInfo rs;
    memset(&rs, 0, sizeof(rs));
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = pv.polygon_mode;
    rs.cullMode = pv.cull_mode;
    rs.frontFace = pv.front_face;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms;
    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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
    gpci.pViewportState = &vpc;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pColorBlendState = &cbs;
    gpci.layout = VK_NULL_HANDLE;
    gpci.renderPass = renderpass;
    gpci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    CHECK(vkCreateGraphicsPipelines(g_dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline)
          == VK_SUCCESS && pipeline != VK_NULL_HANDLE,
          "a pipeline ending in PRESENT_SRC is created");

    /* -- ACQUIRE, RENDER, PRESENT -- */
    {
        VkCommandPoolCreateInfo cpci;
        memset(&cpci, 0, sizeof(cpci));
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = g_family;
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

        /* ACQUIRE the next swapchain image */
        uint32_t index = 0;
        VkSemaphore image_ready = VK_NULL_HANDLE;
        {
            VkSemaphoreCreateInfo sci2;
            memset(&sci2, 0, sizeof(sci2));
            sci2.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkCreateSemaphore(g_dev, &sci2, NULL, &image_ready);
        }
        CHECK(vkAcquireNextImageKHR(g_dev, swapchain, UINT64_MAX, image_ready,
                                    VK_NULL_HANDLE, &index) == VK_SUCCESS,
              "the next swapchain image is ACQUIRED");
        printf("  acquired image %u of %u\n", index, nimg);

        VkCommandBufferBeginInfo cbbi;
        memset(&cbbi, 0, sizeof(cbbi));
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &cbbi);

        VkClearValue clear;
        memset(&clear, 0, sizeof(clear));
        clear.color.float32[0] = 0.9f;
        clear.color.float32[1] = 0.5f;
        clear.color.float32[2] = 0.1f;
        clear.color.float32[3] = 1.0f;

        VkRenderPassBeginInfo rpbi;
        memset(&rpbi, 0, sizeof(rpbi));
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = renderpass;
        rpbi.framebuffer = fbs[index];
        rpbi.renderArea.offset.x = 0; rpbi.renderArea.offset.y = 0;
        rpbi.renderArea.extent.width = W; rpbi.renderArea.extent.height = H;
        rpbi.clearValueCount = 1; rpbi.pClearValues = &clear;

        vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(cb, 3, 1, 0, 0);
        vkCmdEndRenderPass(cb);
        CHECK(vkEndCommandBuffer(cb) == VK_SUCCESS, "the command buffer is recorded");

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si;
        memset(&si, 0, sizeof(si));
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &image_ready;
        si.pWaitDstStageMask = &wait_stage;
        si.commandBufferCount = 1; si.pCommandBuffers = &cb;
        CHECK(vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS,
              "the frame is SUBMITTED");

        /* PRESENT it */
        VkPresentInfoKHR pi;
        memset(&pi, 0, sizeof(pi));
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.swapchainCount = 1;
        pi.pSwapchains = &swapchain;
        pi.pImageIndices = &index;
        CHECK(vkQueuePresentKHR(g_queue, &pi) == VK_SUCCESS,
              "the frame is PRESENTED to the swapchain");
        printf("  PRESENTED: acquire -> render -> present completed\n");

        CHECK(vkDeviceWaitIdle(g_dev) == VK_SUCCESS,
              "the device goes idle without error");
        vkDestroySemaphore(g_dev, image_ready, NULL);
        vkDestroyCommandPool(g_dev, pool, NULL);
    }

    /* -- teardown -- */
    vkDestroyPipeline(g_dev, pipeline, NULL);
    for (uint32_t i = 0; i < nimg; i++) {
        vkDestroyFramebuffer(g_dev, fbs[i], NULL);
        vkDestroyImageView(g_dev, views[i], NULL);
    }
    free(fbs); free(views); free(images);
    vkDestroyRenderPass(g_dev, renderpass, NULL);
    vkDestroySwapchainKHR(g_dev, swapchain, NULL);
    vkDestroyShaderModule(g_dev, vs, NULL);
    vkDestroyShaderModule(g_dev, fs, NULL);
    vkDestroySurfaceKHR(g_inst, surface, NULL);
    vkDestroyDevice(g_dev, NULL);
    vkDestroyInstance(g_inst, NULL);
    free(vw); free(fw);

    if (failures == 0) printf("ALL VKD3D_LIVE_PRESENT TESTS PASSED\n");
    else printf("%d VKD3D_LIVE_PRESENT FAILURES\n", failures);
    return failures ? 1 : 0;
}
