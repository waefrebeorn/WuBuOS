/*
 * wubu_vkd3d_fence.c -- VKD3D slice 6: fences as Vulkan timeline semaphores.
 *
 * See wubu_vkd3d_fence.h.  The counter IS the module; the Vulkan mapping is a
 * struct a real device slice would feed into VkSemaphoreTypeCreateInfoKHR.
 */
#include "wubu_vkd3d_fence.h"

#include <string.h>

void wubu_vkd3d_fence_init(wubu_vkd3d_fence_t *f)
{
    if (!f) return;
    f->value = 0;
    f->completed = 0;
}

int wubu_vkd3d_fence_signal(wubu_vkd3d_fence_t *f, uint64_t value)
{
    if (!f) return -1;
    /* Timeline values must be STRICTLY increasing.  Accepting an equal or
     * lower value would collapse two submissions into one and let the CPU
     * reuse memory the GPU is still reading. */
    if (value <= f->value) return -1;
    f->value = value;
    return 0;
}

int wubu_vkd3d_fence_wait(wubu_vkd3d_fence_t *f, uint64_t value)
{
    if (!f) return -1;
    /* Waiting on a value that was never signalled is a programming error, not
     * a block: the GPU can never reach it. */
    if (value > f->value) return -1;
    return (f->completed >= value) ? 0 : 1;
}

bool wubu_vkd3d_fence_signalled(const wubu_vkd3d_fence_t *f, uint64_t value)
{
    if (!f) return false;
    return f->completed >= value;
}

uint64_t wubu_vkd3d_fence_value(const wubu_vkd3d_fence_t *f)
{
    return f ? f->value : 0;
}

uint64_t wubu_vkd3d_fence_completed(const wubu_vkd3d_fence_t *f)
{
    return f ? f->completed : 0;
}

bool wubu_vkd3d_fence_vk(const wubu_vkd3d_fence_t *f, wubu_vkd3d_fence_vk_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!f) return false;

#ifdef VK_SEMAPHORE_TYPE_TIMELINE_KHR
    out->type = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
#else
    out->type = VK_SEMAPHORE_TYPE_TIMELINE;
#endif
    out->initial_value = 0;
    out->wait_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    out->valid = true;
    out->name = "timeline semaphore";
    return true;
}
