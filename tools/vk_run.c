/* vk_run -- out-of-process Vulkan compute runner for the SPIR-V/Vulkan backend.
 *
 * WHY THIS EXISTS
 *   wubu_isa_vulkan.c compiles MIR to SPIR-V and then popen()s this binary:
 *
 *     /tmp/vk_run <dev_idx> <shader.spv> <arg> <mem_cells> [mem_image.bin]
 *
 *   That means the Vulkan path had a source file that was never committed --
 *   only a prebuilt binary in /tmp. A wiped /tmp, a fresh clone, or a plain
 *   `git clean` broke every Vulkan test, and did so SILENTLY: the backend's
 *   popen() failure path returns 0, which looks exactly like "the shader
 *   computed 0". This file makes the dependency real and reviewable.
 *
 * CONTRACT (must match wubu_isa_vulkan.c and vk_run's argv order exactly)
 *   dev_idx      index into the enumerated physical devices
 *   shader.spv   path to a SPIR-V module
 *   arg          the ISA run contract's MEMORY POINTER, as a decimal. It is
 *                only useful as a number across a process boundary; the bytes
 *                it points at travel in mem_image.bin instead.
 *   mem_cells    SSBO element count. MUST match what the emitter reserved --
 *                wubu_isa_vulkan.c derives it from p->total_mem.
 *   mem_image.bin  optional; uploaded into the SSBO before dispatch so that
 *                MIR programs which READ memory see the caller's data.
 *
 * OUTPUT
 *   The cell-0 value (the MIR return slot) as a bare decimal on stdout --
 *   vulkan_run() skips leading non-numeric lines to tolerate driver banners.
 *   With WUBU_VK_DUMP=N, also prints "cell[i] = v" for i in [0,N) to stderr.
 *
 * Build: make vk_run      (see mk/tests.mk)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define CHK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
    fprintf(stderr, "vk error %d (%s) at line %d\n", (int)_r, #x, __LINE__); \
    return 2; } } while (0)

static const char *res_name(VkResult r)
{
    switch (r) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "OUT_OF_POOL_MEMORY";
    case VK_ERROR_DEVICE_LOST: return "DEVICE_LOST";
    case VK_ERROR_INITIALIZATION_FAILED: return "INITIALIZATION_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "INCOMPATIBLE_DRIVER";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "FORMAT_NOT_SUPPORTED";
    default: return "UNKNOWN";
    }
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr,
            "usage: %s <dev_idx> <shader.spv> <arg> <mem_cells> [mem_image.bin]\n",
            argv[0]);
        return 2;
    }
    uint32_t dev_idx = (uint32_t)atoi(argv[1]);
    const char *spv_path  = argv[2];
    const char *img_path  = (argc >= 6) ? argv[5] : NULL;
    const uint32_t cells  = (uint32_t)strtoul(argv[4], NULL, 10);

    /* ---- load the module ---- */
    FILE *sf = fopen(spv_path, "rb");
    if (!sf) { perror(spv_path); return 2; }
    fseek(sf, 0, SEEK_END);
    long spv_len = ftell(sf);
    fseek(sf, 0, SEEK_SET);
    if (spv_len <= 0) { fprintf(stderr, "empty spv\n"); fclose(sf); return 2; }
    void *spv = malloc((size_t)spv_len);
    if (!spv || fread(spv, 1, (size_t)spv_len, sf) != (size_t)spv_len) {
        fprintf(stderr, "cannot read %s\n", spv_path); fclose(sf); return 2;
    }
    fclose(sf);

    /* ---- instance ---- */
    VkApplicationInfo app = {0};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wubu_vk_run";
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ici = {0};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    VkInstance inst = VK_NULL_HANDLE;
    CHK(vkCreateInstance(&ici, NULL, &inst));

    /* ---- physical device ---- */
    uint32_t ndev = 0;
    CHK(vkEnumeratePhysicalDevices(inst, &ndev, NULL));
    if (ndev == 0) { fprintf(stderr, "no vulkan devices\n"); return 2; }
    if (dev_idx >= ndev) dev_idx = 0;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    CHK(vkEnumeratePhysicalDevices(inst, &ndev, &phys));

    uint32_t nq = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, NULL);
    if (nq == 0) { fprintf(stderr, "no queue families\n"); return 2; }
    VkQueueFamilyProperties qf[8];
    if (nq > 8) nq = 8;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, qf);
    uint32_t qi = 0;
    for (uint32_t i = 0; i < nq; i++)
        if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { qi = i; break; }

    /* ---- device ---- */
    float prio = 1.0f;
    VkDeviceQueueCreateInfo dq = {0};
    dq.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    dq.queueFamilyIndex = qi;
    dq.queueCount = 1;
    dq.pQueuePriorities = &prio;

    VkDeviceCreateInfo dci = {0};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &dq;
    VkDevice dev = VK_NULL_HANDLE;
    CHK(vkCreateDevice(phys, &dci, NULL, &dev));
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(dev, qi, 0, &queue);

    /* ---- host-visible buffer for the SSBO ---- */
    const uint32_t bytes = cells * 8u;
    VkBufferCreateInfo bci = {0};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = bytes ? bytes : 8;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buf = VK_NULL_HANDLE;
    CHK(vkCreateBuffer(dev, &bci, NULL, &buf));

    VkPhysicalDeviceMemoryProperties mprops;
    vkGetPhysicalDeviceMemoryProperties(phys, &mprops);
    uint32_t mt = UINT32_MAX;
    for (uint32_t i = 0; i < mprops.memoryTypeCount; i++) {
        /* Prefer host-visible + host-coherent; fall back to host-visible only,
         * because lavapipe/dzn do not all advertise HOST_COHERENT. */
        const VkMemoryPropertyFlags f = mprops.memoryTypes[i].propertyFlags;
        if (!(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) continue;
        if (mt == UINT32_MAX) mt = i;
        if (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) { mt = i; break; }
    }
    if (mt == UINT32_MAX) { fprintf(stderr, "no host-visible memory\n"); return 2; }

    VkMemoryAllocateInfo mai = {0};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = bytes ? bytes : 8;
    mai.memoryTypeIndex = mt;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    CHK(vkAllocateMemory(dev, &mai, NULL, &mem));
    CHK(vkBindBufferMemory(dev, buf, mem, 0));

    void *mapped = NULL;
    CHK(vkMapMemory(dev, mem, 0, bytes ? bytes : 8, 0, &mapped));
    memset(mapped, 0, bytes ? bytes : 8);

    /* ---- optional pre-dispatch upload ----
     * Without this the SSBO is zeroed and every MIR_LOAD returns 0, which is
     * exactly how the SPIR-V memory path looked broken before it was fixed. */
    if (img_path && strcmp(img_path, "none") != 0) {
        FILE *mf = fopen(img_path, "rb");
        if (!mf) { perror(img_path); return 2; }
        size_t got = fread(mapped, 1, bytes ? bytes : 8, mf);
        fclose(mf);
        fprintf(stderr, "[vk_run] uploaded %zu bytes from %s\n", got, img_path);
    }

    /* ---- descriptor set layout + pool ---- */
    VkDescriptorSetLayoutBinding bind = {0};
    bind.binding = 0;
    bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bind.descriptorCount = 1;
    bind.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dslci = {0};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 1;
    dslci.pBindings = &bind;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    CHK(vkCreateDescriptorSetLayout(dev, &dslci, NULL, &dsl));

    VkPipelineLayoutCreateInfo plci = {0};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &dsl;
    VkPipelineLayout pl = VK_NULL_HANDLE;
    CHK(vkCreatePipelineLayout(dev, &plci, NULL, &pl));

    VkDescriptorPoolSize ps = {0};
    ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ps.descriptorCount = 1;
    VkDescriptorPoolCreateInfo dpci = {0};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &ps;
    VkDescriptorPool dpool = VK_NULL_HANDLE;
    CHK(vkCreateDescriptorPool(dev, &dpci, NULL, &dpool));

    VkDescriptorSetAllocateInfo dsai = {0};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = dpool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &dsl;
    VkDescriptorSet ds = VK_NULL_HANDLE;
    CHK(vkAllocateDescriptorSets(dev, &dsai, &ds));

    VkDescriptorBufferInfo dbi = {0};
    dbi.buffer = buf;
    dbi.offset = 0;
    dbi.range = bytes ? bytes : 8;
    VkWriteDescriptorSet wr = {0};
    wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr.dstSet = ds;
    wr.dstBinding = 0;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(dev, 1, &wr, 0, NULL);

    /* ---- pipeline ---- */
    VkShaderModuleCreateInfo smci = {0};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = (size_t)spv_len;
    smci.pCode = spv;
    VkShaderModule shm = VK_NULL_HANDLE;
    CHK(vkCreateShaderModule(dev, &smci, NULL, &shm));

    VkComputePipelineCreateInfo cpci = {0};
    cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = shm;
    cpci.stage.pName = "main";
    cpci.layout = pl;
    VkPipeline pipe = VK_NULL_HANDLE;
    CHK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, &pipe));

    /* ---- dispatch + read back ---- */
    VkCommandPoolCreateInfo cpi = {0};
    cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpi.queueFamilyIndex = qi;
    VkCommandPool cp = VK_NULL_HANDLE;
    CHK(vkCreateCommandPool(dev, &cpi, NULL, &cp));

    VkCommandBufferAllocateInfo cbai = {0};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = cp;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    CHK(vkAllocateCommandBuffers(dev, &cbai, &cmd));

    VkCommandBufferBeginInfo cbbi = {0};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CHK(vkBeginCommandBuffer(cmd, &cbbi));
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &ds, 0, NULL);
    /* The emitter writes LocalSize 1, so one workgroup is one invocation. */
    vkCmdDispatch(cmd, 1, 1, 1);
    CHK(vkEndCommandBuffer(cmd));

    VkFenceCreateInfo fci = {0};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    CHK(vkCreateFence(dev, &fci, NULL, &fence));

    VkSubmitInfo si = {0};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    CHK(vkQueueSubmit(queue, 1, &si, fence));
    CHK(vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX));

    /* cell 0 is the MIR return slot */
    const int64_t *cellv = (const int64_t *)mapped;
    const int64_t ret = cellv[0];

    const char *dump = getenv("WUBU_VK_DUMP");
    if (dump) {
        unsigned want = (unsigned)strtoul(dump, NULL, 10);
        if (want > cells) want = cells;
        for (unsigned i = 0; i < want; i++)
            fprintf(stderr, "cell[%u] = %lld\n", i, (long long)cellv[i]);
    }

    printf("%lld\n", (long long)ret);
    fflush(stdout);

    /* ---- teardown (ordered: GPU objects before their backing memory) ---- */
    vkDestroyFence(dev, fence, NULL);
    vkDestroyCommandPool(dev, cp, NULL);
    vkDestroyPipeline(dev, pipe, NULL);
    vkDestroyShaderModule(dev, shm, NULL);
    vkDestroyDescriptorPool(dev, dpool, NULL);
    vkDestroyPipelineLayout(dev, pl, NULL);
    vkDestroyDescriptorSetLayout(dev, dsl, NULL);
    vkUnmapMemory(dev, mem);
    vkDestroyBuffer(dev, buf, NULL);
    vkFreeMemory(dev, mem, NULL);
    vkDestroyDevice(dev, NULL);
    vkDestroyInstance(inst, NULL);
    free(spv);
    (void)res_name; /* kept for the error table above */
    return 0;
}