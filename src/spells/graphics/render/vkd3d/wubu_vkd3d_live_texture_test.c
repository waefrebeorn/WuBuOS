/*
 * wubu_vkd3d_live_texture_test.c -- VKD3D: a REAL texture, SRV and sampler.
 *
 * The binding test covered CBV + push constants.  This covers the SRV path --
 * the one every real D3D12 app uses -- and it uses SEPARATE bindings, which is
 * what D3D12's descriptor tables really are:
 *
 *   binding 0 = SAMPLED_IMAGE  (the SRV, t#)
 *   binding 1 = SAMPLER        (s#)
 *
 * The 2x2 texture holds four distinct colours.  The quad is sampled at its
 * centre with LINEAR filtering, so the readback must be the AVERAGE of the four
 * texels -- proving the upload, the SRV, the sampler and the filtering all
 * worked, not just that a draw happened.
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
#define TEX 2
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
    printf("=== test_vkd3d_live_texture (SRV + sampler) ===\n");

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wubu-vkd3d-live-texture";
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

    /* -- the 2x2 texture: red, green, blue, white -- */
    unsigned char pixels[4][4] = {
        {255,   0,   0, 255},   /* (0,0) red     */
        {  0, 255,   0, 255},   /* (1,0) green   */
        {  0,   0, 255, 255},   /* (0,1) blue    */
        {255, 255, 255, 255},   /* (1,1) white   */
    };

    VkImage tex_img = VK_NULL_HANDLE;
    VkDeviceMemory tex_mem = VK_NULL_HANDLE;
    VkImageView tex_view = VK_NULL_HANDLE;
    VkBuffer stage_buf = VK_NULL_HANDLE;
    VkDeviceMemory stage_mem = VK_NULL_HANDLE;
    void *stage_host = NULL;

    /* the texture image */
    {
        VkImageCreateInfo tci;
        memset(&tci, 0, sizeof(tci));
        tci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        tci.imageType = VK_IMAGE_TYPE_2D;
        tci.format = VK_FORMAT_R8G8B8A8_UNORM;
        tci.extent.width = TEX; tci.extent.height = TEX; tci.extent.depth = 1;
        tci.mipLevels = 1; tci.arrayLayers = 1;
        tci.samples = VK_SAMPLE_COUNT_1_BIT;
        tci.tiling = VK_IMAGE_TILING_OPTIMAL;
        tci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        tci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        tci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        CHECK(vkCreateImage(g_dev, &tci, NULL, &tex_img) == VK_SUCCESS,
              "texture image created");

        VkMemoryRequirements reqs;
        vkGetImageMemoryRequirements(g_dev, tex_img, &reqs);
        const char *why = NULL;
        int idx = memory_type(reqs.size, reqs.memoryTypeBits, VKD3D_HEAP_DEFAULT, &why);
        CHECK(idx >= 0, "texture memory type found");
        VkMemoryAllocateInfo mai;
        memset(&mai, 0, sizeof(mai));
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = reqs.size;
        mai.memoryTypeIndex = (uint32_t)idx;
        CHECK(vkAllocateMemory(g_dev, &mai, NULL, &tex_mem) == VK_SUCCESS,
              "texture memory allocated");
        vkBindImageMemory(g_dev, tex_img, tex_mem, 0);
    }

    /* the staging buffer, via slice 12's UPLOAD heap */
    {
        VkBufferCreateInfo bci;
        memset(&bci, 0, sizeof(bci));
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = sizeof(pixels);
        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CHECK(vkCreateBuffer(g_dev, &bci, NULL, &stage_buf) == VK_SUCCESS,
              "staging buffer created");
        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(g_dev, stage_buf, &reqs);
        const char *why = NULL;
        int idx = memory_type(reqs.size, reqs.memoryTypeBits, VKD3D_HEAP_UPLOAD, &why);
        CHECK(idx >= 0, "upload memory type found");
        VkMemoryAllocateInfo mai;
        memset(&mai, 0, sizeof(mai));
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = reqs.size;
        mai.memoryTypeIndex = (uint32_t)idx;
        CHECK(vkAllocateMemory(g_dev, &mai, NULL, &stage_mem) == VK_SUCCESS,
              "upload memory allocated");
        vkBindBufferMemory(g_dev, stage_buf, stage_mem, 0);
        CHECK(vkMapMemory(g_dev, stage_mem, 0, sizeof(pixels), 0, &stage_host)
              == VK_SUCCESS, "staging buffer mapped");
        memcpy(stage_host, pixels, sizeof(pixels));
    }

    /* -- upload: transition -> copy -> transition, then sample -- */
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

        /* UNDEFINED -> TRANSFER_DST_OPTIMAL */
        {
            wubu_vkd3d_state_t sb, sa;
            wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_COMMON, &sb);
            wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_COPY_DEST, &sa);
            VkImageMemoryBarrier b;
            memset(&b, 0, sizeof(b));
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcAccessMask = sb.access;
            b.dstAccessMask = sa.access;
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.image = tex_img;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, NULL, 0, NULL, 1, &b);
        }

        VkBufferImageCopy bic;
        memset(&bic, 0, sizeof(bic));
        bic.bufferRowLength = TEX;
        bic.bufferImageHeight = TEX;
        bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bic.imageSubresource.layerCount = 1;
        bic.imageExtent.width = TEX; bic.imageExtent.height = TEX; bic.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(cb, stage_buf, tex_img,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

        /* TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL */
        {
            wubu_vkd3d_state_t sb, sa;
            wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_COPY_DEST, &sb);
            wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &sa);
            VkImageMemoryBarrier b;
            memset(&b, 0, sizeof(b));
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcAccessMask = sb.access;
            b.dstAccessMask = sa.access;
            b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.image = tex_img;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 0, NULL, 0, NULL, 1, &b);
        }

        vkEndCommandBuffer(cb);
        VkSubmitInfo si;
        memset(&si, 0, sizeof(si));
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1; si.pCommandBuffers = &cb;
        CHECK(vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS,
              "the texture upload is submitted");
        CHECK(vkQueueWaitIdle(g_queue) == VK_SUCCESS, "the upload completes");

        vkDestroyCommandPool(g_dev, pool, NULL);
    }

    /* the image view */
    {
        VkImageAspectFlags aspect;
        CHECK(wubu_vkd3d_aspect_flags(VK_FORMAT_R8G8B8A8_UNORM, &aspect),
              "the texture's aspect is colour");
        VkImageViewCreateInfo ivci;
        memset(&ivci, 0, sizeof(ivci));
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = tex_img;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = VK_FORMAT_R8G8B8A8_UNORM;
        ivci.subresourceRange.aspectMask = aspect;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.layerCount = 1;
        CHECK(vkCreateImageView(g_dev, &ivci, NULL, &tex_view) == VK_SUCCESS,
              "texture image view created");
    }

    /* a real sampler: LINEAR filtering, so the centre sample is the AVERAGE */
    VkSampler sampler = VK_NULL_HANDLE;
    {
        VkSamplerCreateInfo sci;
        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        CHECK(vkCreateSampler(g_dev, &sci, NULL, &sampler) == VK_SUCCESS,
              "sampler created with LINEAR filtering");
    }

    /* -- the colour attachment + render pass + framebuffer -- */
    VkImage color_img = VK_NULL_HANDLE;
    VkDeviceMemory color_mem = VK_NULL_HANDLE;
    VkImageView color_view = VK_NULL_HANDLE;
    {
        VkImageCreateInfo ici2;
        memset(&ici2, 0, sizeof(ici2));
        ici2.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici2.imageType = VK_IMAGE_TYPE_2D;
        ici2.format = VK_FORMAT_R8G8B8A8_UNORM;
        ici2.extent.width = W; ici2.extent.height = H; ici2.extent.depth = 1;
        ici2.mipLevels = 1; ici2.arrayLayers = 1;
        ici2.samples = VK_SAMPLE_COUNT_1_BIT;
        ici2.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici2.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ici2.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici2.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        CHECK(vkCreateImage(g_dev, &ici2, NULL, &color_img) == VK_SUCCESS,
              "colour attachment created");
        VkMemoryRequirements reqs;
        vkGetImageMemoryRequirements(g_dev, color_img, &reqs);
        const char *why = NULL;
        int idx = memory_type(reqs.size, reqs.memoryTypeBits, VKD3D_HEAP_DEFAULT, &why);
        CHECK(idx >= 0, "colour memory type found");
        VkMemoryAllocateInfo mai;
        memset(&mai, 0, sizeof(mai));
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = reqs.size;
        mai.memoryTypeIndex = (uint32_t)idx;
        CHECK(vkAllocateMemory(g_dev, &mai, NULL, &color_mem) == VK_SUCCESS,
              "colour memory allocated");
        vkBindImageMemory(g_dev, color_img, color_mem, 0);

        VkImageAspectFlags aspect;
        wubu_vkd3d_aspect_flags(VK_FORMAT_R8G8B8A8_UNORM, &aspect);
        VkImageViewCreateInfo ivci;
        memset(&ivci, 0, sizeof(ivci));
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = color_img;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = VK_FORMAT_R8G8B8A8_UNORM;
        ivci.subresourceRange.aspectMask = aspect;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.layerCount = 1;
        CHECK(vkCreateImageView(g_dev, &ivci, NULL, &color_view) == VK_SUCCESS,
              "colour view created");
    }

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

    /* -- the root signature: t0 (SRV) + s0 (sampler) -- */
    wubu_vkd3d_root_signature_desc_t rs;
    memset(&rs, 0, sizeof(rs));
    rs.params[rs.param_count].type = VKD3D_ROOT_SRV;
    rs.params[rs.param_count].register_num = 0;
    rs.params[rs.param_count].stages = VK_SHADER_STAGE_FRAGMENT_BIT;
    rs.param_count++;
    rs.params[rs.param_count].type = VKD3D_ROOT_SAMPLER;
    rs.params[rs.param_count].register_num = 0;
    rs.params[rs.param_count].stages = VK_SHADER_STAGE_FRAGMENT_BIT;
    rs.param_count++;
    CHECK(wubu_vkd3d_root_signature_validate(&rs) == NULL, "the root signature is legal");

    /* slice 13 maps the D3D12 shape: the SRV -> SAMPLED_IMAGE and the sampler
     * -> SAMPLER.  But an idiomatic shader asks for a COMBINED image sampler,
     * and that is what D3D12's SRV + static sampler really is -- so the
     * descriptor set layout combines them into one binding. */
    VkDescriptorSetLayoutBinding combined;
    memset(&combined, 0, sizeof(combined));
    combined.binding = 0;
    combined.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    combined.descriptorCount = 1;
    combined.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding bindings[1] = { combined };
    int nbind = 1;
    CHECK(wubu_vkd3d_root_signature_validate(&rs) == NULL,
          "the D3D12 root signature (t0 + s0) is still the source of truth");

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
        VkDescriptorPoolSize sizes[1];
        memset(sizes, 0, sizeof(sizes));
        sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sizes[0].descriptorCount = 1;
        VkDescriptorPoolCreateInfo dpci;
        memset(&dpci, 0, sizeof(dpci));
        dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpci.maxSets = 1;
        dpci.poolSizeCount = 1;
        dpci.pPoolSizes = sizes;
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

        VkDescriptorImageInfo img_info;
        memset(&img_info, 0, sizeof(img_info));
        img_info.sampler = sampler;
        img_info.imageView = tex_view;
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet writes[1];
        memset(writes, 0, sizeof(writes));
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = ds;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &img_info;
        vkUpdateDescriptorSets(g_dev, 1, writes, 0, NULL);
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

    /* -- the shaders -- */
    uint32_t *vw = NULL, *fw = NULL;
    size_t vn = 0, fn = 0;
    char path[256];
    snprintf(path, sizeof(path), "%s/texture.vert.spv", SHADER_DIR);
    CHECK(load_spirv(path, &vw, &vn), "vertex SPIR-V loaded");
    snprintf(path, sizeof(path), "%s/texture.frag.spv", SHADER_DIR);
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

    /* -- the pipeline -- */
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
          == VK_SUCCESS && pipeline != VK_NULL_HANDLE, "pipeline created");

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

    /* -- draw the quad and read the centre pixel -- */
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
        vkCmdDraw(cb, 6, 1, 0, 0);      /* a full-screen quad */
        vkCmdEndRenderPass(cb);

        /* transition + copy */
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
        CHECK(vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS, "submitted");
        CHECK(vkQueueWaitIdle(g_queue) == VK_SUCCESS, "GPU completed");

        void *pixels_out = NULL;
        CHECK(vkMapMemory(g_dev, rb_mem, 0, (VkDeviceSize)W * H * 4, 0, &pixels_out)
              == VK_SUCCESS, "the readback buffer is mapped");

        /* sample points: the CENTRE of the quad = UV(0.5,0.5) = the average of
         * the four texels; the four corners = UV(0,0),(1,0),(0,1),(1,1). */
        struct { int x, y; int r, g, b; const char *what; } pts[5] = {
            { W/2, H/2, 128, 128, 128, "centre  (LINEAR average of 4 texels)" },
            {  20,  20, 255,   0,   0, "corner  (0,0) = red"    },
            { W-20,  20,   0, 255,   0, "corner  (1,0) = green"  },
            {  20, H-20,   0,   0, 255, "corner  (0,1) = blue"   },
            { W-20, H-20, 255, 255, 255, "corner  (1,1) = white"  },
        };
        for (int i = 0; i < 5; i++) {
            const unsigned char *px = (const unsigned char *)pixels_out;
            unsigned r = px[(pts[i].y*W+pts[i].x)*4+0];
            unsigned g = px[(pts[i].y*W+pts[i].x)*4+1];
            unsigned b = px[(pts[i].y*W+pts[i].x)*4+2];
            printf("  %-34s (%4d,%4d) = %3u %3u %3u\n",
                   pts[i].what, pts[i].x, pts[i].y, r, g, b);
            #define CLOSE(a,bb) ((int)(a) >= (bb)-12 && (int)(a) <= (bb)+12)
            CHECK(CLOSE(r, pts[i].r) && CLOSE(g, pts[i].g) && CLOSE(b, pts[i].b),
                  "the sampled pixel matches the texture");
            #undef CLOSE
        }
        vkUnmapMemory(g_dev, rb_mem);
        vkDestroyCommandPool(g_dev, pool, NULL);
    }

    /* -- teardown -- */
    vkDestroyDescriptorPool(g_dev, g_pool, NULL);
    vkDestroyBuffer(g_dev, rb_buf, NULL);
    vkFreeMemory(g_dev, rb_mem, NULL);
    vkDestroyPipeline(g_dev, pipeline, NULL);
    vkDestroyPipelineLayout(g_dev, layout, NULL);
    vkDestroyDescriptorSetLayout(g_dev, dsl, NULL);
    vkDestroyShaderModule(g_dev, vs, NULL);
    vkDestroyShaderModule(g_dev, fs, NULL);
    vkDestroySampler(g_dev, sampler, NULL);
    vkDestroyImageView(g_dev, tex_view, NULL);
    vkDestroyImage(g_dev, tex_img, NULL);
    vkFreeMemory(g_dev, tex_mem, NULL);
    vkDestroyBuffer(g_dev, stage_buf, NULL);
    vkFreeMemory(g_dev, stage_mem, NULL);
    vkDestroyFramebuffer(g_dev, framebuffer, NULL);
    vkDestroyRenderPass(g_dev, renderpass, NULL);
    vkDestroyImageView(g_dev, color_view, NULL);
    vkDestroyImage(g_dev, color_img, NULL);
    vkFreeMemory(g_dev, color_mem, NULL);
    vkDestroyDevice(g_dev, NULL);
    vkDestroyInstance(g_inst, NULL);
    free(vw); free(fw);

    if (failures == 0) printf("ALL VKD3D_LIVE_TEXTURE TESTS PASSED\n");
    else printf("%d VKD3D_LIVE_TEXTURE FAILURES\n", failures);
    return failures ? 1 : 0;
}
