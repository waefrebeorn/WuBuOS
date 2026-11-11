/*
 * wubu_vkd3d_live_render_test.c -- VKD3D: the first REAL GPU work.
 *
 * Everything before this opened a device or built descriptors.  This one
 * allocates real images, creates a real VkRenderPass and VkFramebuffer, records
 * a command buffer that CLEARS them, submits it, and waits for the GPU.
 *
 * It is GPU-requiring and therefore a SEPARATE target, not part of the gate.
 */
#include "wubu_vkd3d_physdev.h"
#include "wubu_vkd3d_vkdev.h"
#include "wubu_vkd3d_memory.h"
#include "wubu_vkd3d_renderpass.h"
#include "wubu_vkd3d_framebuffer.h"

#include <stdio.h>
#include <string.h>

#define W 1920
#define H 1080

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

typedef struct {
    VkImage        image;
    VkDeviceMemory memory;
    VkImageView    view;
    VkFormat       format;
    bool           depth;
} live_attachment_t;

static VkPhysicalDeviceMemoryProperties g_memprops;
static VkInstance       g_inst;
static VkPhysicalDevice g_phys;
static VkDevice         g_dev;
static uint32_t         g_gfx_family;
static VkQueue          g_queue;

/* Create an image + its memory + its view.  Uses the slice-12 heap mapping to
 * pick the memory type, which is exactly the live payoff of that slice. */
static bool make_attachment(const char *name, VkFormat format, bool depth,
                            VkImageUsageFlags usage, live_attachment_t *out)
{
    memset(out, 0, sizeof(*out));
    out->format = format;
    out->depth = depth;

    VkImageCreateInfo ici;
    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent.width = W;
    ici.extent.height = H;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(g_dev, &ici, NULL, &out->image) != VK_SUCCESS) {
        printf("  FAIL: vkCreateImage(%s)\n", name);
        return false;
    }

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(g_dev, out->image, &reqs);

    /* -- slice 12: D3D12 DEFAULT heap -> DEVICE_LOCAL -- */
    wubu_vkd3d_alloc_request_t ar;
    memset(&ar, 0, sizeof(ar));
    ar.size = reqs.size;
    ar.heap_type = VKD3D_HEAP_DEFAULT;
    ar.type_bits = reqs.memoryTypeBits;

    wubu_vkd3d_memory_types_t types;
    memset(&types, 0, sizeof(types));
    types.count = 32;
    vkGetPhysicalDeviceMemoryProperties(g_phys, &g_memprops);
    types.count = g_memprops.memoryTypeCount;
    for (uint32_t i = 0; i < types.count && i < 32; i++)
        types.types[i] = g_memprops.memoryTypes[i];

    const char *why = NULL;
    int idx = wubu_vkd3d_memory_type_index(&types, &ar, &why);
    if (idx < 0) {
        printf("  FAIL: no memory type for %s: %s\n", name, why ? why : "?");
        return false;
    }

    VkMemoryAllocateInfo mai;
    memset(&mai, 0, sizeof(mai));
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = reqs.size;
    mai.memoryTypeIndex = (uint32_t)idx;

    if (vkAllocateMemory(g_dev, &mai, NULL, &out->memory) != VK_SUCCESS) {
        printf("  FAIL: vkAllocateMemory(%s)\n", name);
        return false;
    }
    if (vkBindImageMemory(g_dev, out->image, out->memory, 0) != VK_SUCCESS) {
        printf("  FAIL: vkBindImageMemory(%s)\n", name);
        return false;
    }

    /* -- the view, with the aspect taken from the FORMAT (slice 11) -- */
    VkImageAspectFlags aspect;
    if (!wubu_vkd3d_aspect_flags(format, &aspect)) {
        printf("  FAIL: no aspect flags for %s\n", name);
        return false;
    }
    VkImageViewCreateInfo ivci;
    memset(&ivci, 0, sizeof(ivci));
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = out->image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = format;
    ivci.subresourceRange.aspectMask = aspect;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount = 1;

    if (vkCreateImageView(g_dev, &ivci, NULL, &out->view) != VK_SUCCESS) {
        printf("  FAIL: vkCreateImageView(%s)\n", name);
        return false;
    }
    printf("  %s: image+memory+view created (memory type %d)\n", name, idx);
    return true;
}

int main(void)
{
    printf("=== test_vkd3d_live_render (real GPU work) ===\n");

    /* -- instance -- */
    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wubu-vkd3d-live-render";
    app.apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);
    VkInstanceCreateInfo ici;
    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    if (vkCreateInstance(&ici, NULL, &g_inst) != VK_SUCCESS) {
        printf("  SKIP: no Vulkan loader\n");
        return 0;
    }

    /* -- pick the discrete GPU -- */
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

    /* -- logical device -- */
    wubu_vkd3d_vkdev_desc_t dd;
    memset(&dd, 0, sizeof(dd));
    dd.queue_request_count = 1;
    dd.queues[0].family_index = g_gfx_family;
    dd.queues[0].queue_count = 1;
    dd.queues[0].priority = 1.0f;
    CHECK(wubu_vkd3d_vkdev_create(&devs[chosen], &dd, &g_dev), "logical device created");
    vkGetDeviceQueue(g_dev, g_gfx_family, 0, &g_queue);
    CHECK(g_queue != VK_NULL_HANDLE, "a graphics queue was retrieved");

    /* -- real images -- */
    live_attachment_t color, depth;
    CHECK(make_attachment("colour", VK_FORMAT_R8G8B8A8_UNORM, false,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &color),
          "colour attachment created");
    CHECK(make_attachment("depth", VK_FORMAT_D32_SFLOAT, true,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &depth),
          "depth attachment created");

    /* -- a real render pass, from the slice-10 desc -- */
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
    CHECK(wubu_vkd3d_renderpass_validate(&rp) == NULL, "the render pass desc is legal");

    VkAttachmentDescription ad[8];
    int nad = wubu_vkd3d_renderpass_attachments(&rp, ad, 8);
    VkAttachmentReference crefs[8];
    int ncref = wubu_vkd3d_renderpass_color_refs(&rp, crefs, 8);
    VkAttachmentReference dref;
    int ndref = wubu_vkd3d_renderpass_depth_ref(&rp, &dref);
    CHECK(nad == 2 && ncref == 1 && ndref == 1, "the render pass pieces translate");

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
    CHECK(vkCreateRenderPass(g_dev, &rpci, NULL, &renderpass) == VK_SUCCESS &&
          renderpass != VK_NULL_HANDLE,
          "a REAL VkRenderPass is created");

    /* -- a real framebuffer, from the slice-11 desc -- */
    wubu_vkd3d_framebuffer_desc_t fb;
    memset(&fb, 0, sizeof(fb));
    fb.attachment_count = 2;
    fb.attachments[0].view = color.view;
    fb.attachments[0].format = color.format;
    fb.attachments[0].width = W;  fb.attachments[0].height = H;
    fb.attachments[0].samples = 1; fb.attachments[0].is_depth_stencil = false;
    fb.attachments[1].view = depth.view;
    fb.attachments[1].format = depth.format;
    fb.attachments[1].width = W;  fb.attachments[1].height = H;
    fb.attachments[1].samples = 1; fb.attachments[1].is_depth_stencil = true;
    fb.width = W; fb.height = H; fb.layers = 1;
    CHECK(wubu_vkd3d_framebuffer_validate(&fb) == NULL, "the framebuffer desc is legal");

    VkFramebufferCreateInfo fbci;
    CHECK(wubu_vkd3d_framebuffer_create_info(&fb, &fbci), "the framebuffer translates");

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    CHECK(vkCreateFramebuffer(g_dev, &fbci, NULL, &framebuffer) == VK_SUCCESS &&
          framebuffer != VK_NULL_HANDLE,
          "a REAL VkFramebuffer is created");

    /* -- record and EXECUTE a clear: the first GPU work this tree has done -- */
    {
        VkCommandPoolCreateInfo cpci;
        memset(&cpci, 0, sizeof(cpci));
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = g_gfx_family;
        VkCommandPool pool = VK_NULL_HANDLE;
        CHECK(vkCreateCommandPool(g_dev, &cpci, NULL, &pool) == VK_SUCCESS,
              "a command pool is created");

        VkCommandBufferAllocateInfo cbai;
        memset(&cbai, 0, sizeof(cbai));
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = pool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        CHECK(vkAllocateCommandBuffers(g_dev, &cbai, &cb) == VK_SUCCESS,
              "a command buffer is allocated");

        VkCommandBufferBeginInfo cbbi;
        memset(&cbbi, 0, sizeof(cbbi));
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CHECK(vkBeginCommandBuffer(cb, &cbbi) == VK_SUCCESS, "the command buffer begins");

        VkClearValue clears[2];
        memset(clears, 0, sizeof(clears));
        clears[0].color.float32[0] = 0.2f;
        clears[0].color.float32[1] = 0.4f;
        clears[0].color.float32[2] = 0.8f;
        clears[0].color.float32[3] = 1.0f;
        clears[1].depthStencil.depth = 1.0f;
        clears[1].depthStencil.stencil = 0;

        VkRenderPassBeginInfo rpbi;
        memset(&rpbi, 0, sizeof(rpbi));
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = renderpass;
        rpbi.framebuffer = framebuffer;
        rpbi.renderArea.offset.x = 0;
        rpbi.renderArea.offset.y = 0;
        rpbi.renderArea.extent.width = W;
        rpbi.renderArea.extent.height = H;
        rpbi.clearValueCount = 2;
        rpbi.pClearValues = clears;

        vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(cb);
        CHECK(vkEndCommandBuffer(cb) == VK_SUCCESS, "the command buffer is recorded");

        VkSubmitInfo si;
        memset(&si, 0, sizeof(si));
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        CHECK(vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS,
              "the clear is SUBMITTED to the GPU");
        CHECK(vkQueueWaitIdle(g_queue) == VK_SUCCESS,
              "the GPU COMPLETES the clear without error");
        printf("  clear executed: GPU accepted and completed the render pass\n");

        vkDestroyCommandPool(g_dev, pool, NULL);
    }

    /* -- teardown -- */
    vkDestroyFramebuffer(g_dev, framebuffer, NULL);
    vkDestroyRenderPass(g_dev, renderpass, NULL);
    vkDestroyImageView(g_dev, color.view, NULL);
    vkDestroyImageView(g_dev, depth.view, NULL);
    vkDestroyImage(g_dev, color.image, NULL);
    vkDestroyImage(g_dev, depth.image, NULL);
    vkFreeMemory(g_dev, color.memory, NULL);
    vkFreeMemory(g_dev, depth.memory, NULL);
    vkDestroyDevice(g_dev, NULL);
    vkDestroyInstance(g_inst, NULL);

    if (failures == 0) printf("ALL VKD3D_LIVE_RENDER TESTS PASSED\n");
    else printf("%d VKD3D_LIVE_RENDER FAILURES\n", failures);
    return failures ? 1 : 0;
}
