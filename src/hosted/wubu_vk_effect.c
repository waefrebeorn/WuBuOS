/*
 * wubu_vk_effect.c -- the GPU leg of the effect engine.
 *
 * See wubu_vk_effect.h.  Everything Vulkan here goes through the public
 * wubu_vulkan.h helpers; only VkBuffer creation is raw, because the helper
 * does not expose it.
 *
 * Hygiene rules that are mandatory on this host (see the wubuos-gpu-shim
 * skill): zero-init every Vk* struct, enumerate physical devices twice, pick a
 * HOST_VISIBLE|HOST_COHERENT memory type for mapped buffers, and name the
 * compute entry point "main".
 */
#include "wubu_vk_effect.h"

#include "wubu_vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Must match the push_constant block in wubu_vk_effect.comp. */
typedef struct {
    uint32_t count;
    int32_t  kind;
    uint32_t color;
    int32_t  strength;
    int32_t  width;
    int32_t  height;
} VkEffectPush;

#define VK_EFFECT_LOCAL_SIZE 64

struct WubuVkEffect {
    WubuVkInstance     inst;
    WubuVkPhysicalDevice phys;
    WubuVkDevice       dev;
    WubuVkCmdPool      pool;
    WubuVkAllocator    alloc;
    WubuVkComputePipeline pipe;
    VkBuffer           buffer;
    VkDeviceMemory     buffer_mem;
    uint32_t           buffer_cap;   /* pixels the buffer can hold */
    char               device_name[128];
    bool               has_pipeline;
};

static const char *g_err;   /* last error, static by design */

static void set_err(const char *m) { g_err = m; }

/* ---------------------------------------------------------------- setup */

static uint32_t *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint32_t *buf = (uint32_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) { free(buf); return NULL; }
    *out_len = (size_t)n;
    return buf;
}

WubuVkEffect *wubu_vk_effect_new(const char *spirv_path) {
    WubuVkEffect *e = (WubuVkEffect *)calloc(1, sizeof *e);
    if (!e) { set_err("out of memory"); return NULL; }

    /* SPIR-V first: no point making a device if the shader is missing. */
    size_t spirv_len = 0;
    uint32_t *spirv = spirv_path ? read_file(spirv_path, &spirv_len) : NULL;
    if (!spirv) { set_err("cannot read the effect shader"); free(e); return NULL; }

    if (wubu_vk_instance_create(&e->inst, "WuBuOS Effects", 1, "WuBuOS", 1, NULL, 0, NULL, 0) != 0) {
        set_err("vkCreateInstance failed"); free(spirv); free(e); return NULL;
    }
    if (wubu_vk_physical_device_pick(&e->phys, &e->inst) != 0) {
        set_err("no Vulkan physical device"); free(spirv); free(e); return NULL;
    }
    snprintf(e->device_name, sizeof e->device_name, "%s",
             e->phys.properties.deviceName);

    if (wubu_vk_device_create(&e->dev, &e->phys, VK_NULL_HANDLE, NULL, 0) != 0) {
        set_err("vkCreateDevice failed"); free(spirv); free(e); return NULL;
    }
    if (wubu_vk_cmd_pool_create(&e->pool, &e->dev, e->dev.compute_queue_family, 1) != 0) {
        set_err("command pool failed"); free(spirv); free(e); return NULL;
    }
    if (wubu_vk_allocator_create(&e->alloc, &e->dev) != 0) {
        set_err("allocator failed"); free(spirv); free(e); return NULL;
    }

    /* One storage binding + a push-constant range for the pass parameters. */
    VkDescriptorSetLayoutBinding binding = {0};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPushConstantRange pcr = {0};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(VkEffectPush);

    WubuVkShaderModule shader;
    if (wubu_vk_shader_module_create(&shader, &e->dev, spirv, spirv_len) != 0) {
        set_err("shader module failed"); free(spirv); free(e); return NULL;
    }
    free(spirv);   /* the module owns a copy of the code */

    int rc = wubu_vk_compute_pipeline_create(&e->pipe, &e->dev, &shader, "main",
                                             &binding, 1, &pcr, 1);
    wubu_vk_shader_module_destroy(&shader);
    if (rc != 0) { set_err("compute pipeline failed"); free(e); return NULL; }

    if (wubu_vk_compute_descriptor_sets_alloc(&e->pipe, &binding, 1) != 0) {
        set_err("descriptor set failed"); free(e); return NULL;
    }

    e->has_pipeline = true;
    return e;
}

void wubu_vk_effect_free(WubuVkEffect *e) {
    if (!e) return;
    if (e->buffer)        vkDestroyBuffer(e->dev.device, e->buffer, NULL);
    if (e->buffer_mem)    wubu_vk_free(&e->alloc, e->buffer_mem);
    if (e->has_pipeline) wubu_vk_compute_pipeline_destroy(&e->pipe);
    if (e->pool.pool)    wubu_vk_cmd_pool_destroy(&e->pool);
    if (e->dev.device)   wubu_vk_device_destroy(&e->dev);
    if (e->inst.instance) wubu_vk_instance_destroy(&e->inst);
    free(e);
}

const char *wubu_vk_effect_device_name(WubuVkEffect *e) {
    return e ? e->device_name : NULL;
}

bool wubu_vk_effect_available(WubuVkEffect *e) {
    return e && e->has_pipeline;
}

/* ------------------------------------------------------------- resources */

static int ensure_buffer(WubuVkEffect *e, uint32_t pixels) {
    if (e->buffer && pixels <= e->buffer_cap) return 0;

    if (e->buffer)     vkDestroyBuffer(e->dev.device, e->buffer, NULL);
    if (e->buffer_mem) wubu_vk_free(&e->alloc, e->buffer_mem);
    e->buffer = VK_NULL_HANDLE;
    e->buffer_mem = VK_NULL_HANDLE;

    /* Round up so a few resizes do not reallocate every frame. */
    uint32_t cap = pixels < 1024 ? 1024 : pixels;
    VkDeviceSize bytes = (VkDeviceSize)cap * sizeof(uint32_t);

    VkBufferCreateInfo bci = {0};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = bytes;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(e->dev.device, &bci, NULL, &e->buffer) != VK_SUCCESS) {
        set_err("vkCreateBuffer failed");
        return -1;
    }

    VkMemoryRequirements req = {0};
    vkGetBufferMemoryRequirements(e->dev.device, e->buffer, &req);

    /* HOST_VISIBLE|HOST_COHERENT so the CPU can map it directly. */
    e->buffer_mem = wubu_vk_alloc(&e->alloc, &req,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!e->buffer_mem) { set_err("no host-visible memory type"); return -1; }
    if (vkBindBufferMemory(e->dev.device, e->buffer, e->buffer_mem, 0) != VK_SUCCESS) {
        set_err("vkBindBufferMemory failed");
        return -1;
    }
    e->buffer_cap = cap;
    return 0;
}

int wubu_vk_effect_apply_stack(WubuVkEffect *e, const WubuEffectStack *s,
                               uint32_t *px, int w, int h, const char **err) {
    if (!s || wubu_effect_stack_count(s) == 0) return 0;   /* nothing to do */
    int n = wubu_effect_stack_count(s);
    for (int i = 0; i < n; i++) {
        const WubuEffect *fx = wubu_effect_stack_at(s, i);
        if (!fx) continue;
        if (wubu_vk_effect_apply(e, px, w, h, fx->kind, fx->color, fx->strength,
                                 err) != 0)
            return -1;
    }
    return 0;
}

/* ---------------------------------------------------- the desktop path */

static WubuVkEffect *g_desktop_ctx;
static int           g_desktop_tried;

const char *wubu_vk_effect_desktop_name(void) {
    if (!g_desktop_tried) (void)wubu_vk_effect_desktop_apply(NULL, NULL, 0, 0);
    return g_desktop_ctx ? wubu_vk_effect_device_name(g_desktop_ctx) : "none";
}

/*
 * Apply a stack on the GPU when this host has one.  The context is created
 * lazily and only once, so a host with no Vulkan pays a failed open and then
 * uses the CPU engine forever after.
 */
int wubu_vk_effect_desktop_apply(const WubuEffectStack *s, uint32_t *px,
                                 int w, int h) {
    if (!s || wubu_effect_stack_count(s) == 0) return 0;   /* nothing to do */
    if (!g_desktop_tried) {
        g_desktop_tried = 1;
        const char *path = getenv("WUBU_VK_EFFECT_SPV");
        g_desktop_ctx = wubu_vk_effect_new(path ? path
                                                : "src/hosted/wubu_vk_effect.spv");
    }
    if (!g_desktop_ctx) return 0;
    const char *err = NULL;
    return wubu_vk_effect_apply_stack(g_desktop_ctx, s, px, w, h, &err) == 0
               ? 1 : 0;
}

/* ------------------------------------------------------------------ pass */

int wubu_vk_effect_apply(WubuVkEffect *e, uint32_t *px, int w, int h,
                         WubuEffectKind kind, uint32_t color, int strength,
                         const char **err) {
    if (err) *err = NULL;
    if (!e || !e->has_pipeline || !px || w <= 0 || h <= 0) {
        if (err) *err = "no GPU pipeline";
        return -1;
    }
    uint32_t count = (uint32_t)(w * h);
    if (ensure_buffer(e, count) != 0) { if (err) *err = g_err; return -1; }

    /* Upload. */
    void *mapped = NULL;
    if (vkMapMemory(e->dev.device, e->buffer_mem, 0,
                    (VkDeviceSize)count * sizeof(uint32_t), 0, &mapped) != VK_SUCCESS) {
        if (err) *err = "vkMapMemory failed";
        return -1;
    }
    memcpy(mapped, px, (size_t)count * sizeof(uint32_t));
    vkUnmapMemory(e->dev.device, e->buffer_mem);

    if (wubu_vk_compute_descriptor_write_buffer(&e->pipe, 0, e->buffer, 0,
                                                (VkDeviceSize)count * sizeof(uint32_t)) != 0) {
        if (err) *err = "descriptor write failed";
        return -1;
    }

    VkEffectPush push = {0};
    push.count    = count;
    push.kind     = (int32_t)kind;
    push.color    = color & 0xFFFFFFu;
    push.strength = strength < 0 ? 0 : (strength > 100 ? 100 : strength);
    push.width    = w;
    push.height   = h;

    uint32_t groups = (count + VK_EFFECT_LOCAL_SIZE - 1) / VK_EFFECT_LOCAL_SIZE;

    /* The pool helpers split the job: begin/reset, record (dispatch), end,
     * submit, wait.  Dispatch alone only records -- without submit the GPU
     * never runs the pass, which reads back as nonsense rather than an error.
     */
    const uint32_t slot = 0;
    wubu_vk_cmd_begin(&e->pool, slot);
    if (wubu_vk_cmd_dispatch(&e->pool, slot, &e->pipe,
                             groups, 1, 1, sizeof push, &push) != 0) {
        if (err) *err = "dispatch failed";
        return -1;
    }
    wubu_vk_cmd_end(&e->pool, e->pool.buffers[slot]);
    wubu_vk_cmd_submit(&e->dev, e->dev.compute_queue,
                       &e->pool.buffers[slot], 1, NULL, 0, NULL, 0,
                       e->pool.fences[slot]);
    vkWaitForFences(e->dev.device, 1, &e->pool.fences[slot], VK_TRUE, UINT64_MAX);
    vkResetFences(e->dev.device, 1, &e->pool.fences[slot]);

    /* Download. */
    mapped = NULL;
    if (vkMapMemory(e->dev.device, e->buffer_mem, 0,
                    (VkDeviceSize)count * sizeof(uint32_t), 0, &mapped) != VK_SUCCESS) {
        if (err) *err = "vkMapMemory (read) failed";
        return -1;
    }
    memcpy(px, mapped, (size_t)count * sizeof(uint32_t));
    vkUnmapMemory(e->dev.device, e->buffer_mem);
    return 0;
}
