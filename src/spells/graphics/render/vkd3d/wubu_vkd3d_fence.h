/*
 * wubu_vkd3d_fence.h -- VKD3D slice 6: fences as Vulkan timeline semaphores.
 *
 * A D3D12 fence is a 64-bit monotonic counter the CPU signals and the GPU
 * signals, and both sides wait on.  Vulkan's binary semaphore cannot express
 * that -- it has no value -- but the TIMELINE semaphore can, with a 1:1
 * behavioural match: vkSignalSemaphoreKHR / vkWaitSemaphoresKHR carry exactly
 * the value this module tracks.
 *
 * The rule that matters: timeline values must be STRICTLY INCREASING.  An equal
 * or lower signal is invalid, and silently accepting it would make two
 * submissions look like one -- the GPU could still be executing the first while
 * the CPU believes it is done, which is a use-after-free that surfaces frames
 * later.
 *
 * Opaque struct, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_FENCE_H
#define WUBU_VKD3D_FENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

typedef struct {
    uint64_t value;       /* the next value that will be signalled */
    uint64_t completed;   /* the highest value the GPU has reached */
} wubu_vkd3d_fence_t;

void wubu_vkd3d_fence_init(wubu_vkd3d_fence_t *f);

/* Signal the fence to `value`.  Returns 0 on success, -1 when the value is not
 * strictly greater than the current one. */
int  wubu_vkd3d_fence_signal(wubu_vkd3d_fence_t *f, uint64_t value);

/* Wait until the fence has reached `value`.  Returns 0 when it already has,
 * 1 when the caller must block, -1 on a bad value. */
int  wubu_vkd3d_fence_wait(wubu_vkd3d_fence_t *f, uint64_t value);

/* Has the fence reached `value`? */
bool wubu_vkd3d_fence_signalled(const wubu_vkd3d_fence_t *f, uint64_t value);

uint64_t wubu_vkd3d_fence_value(const wubu_vkd3d_fence_t *f);
uint64_t wubu_vkd3d_fence_completed(const wubu_vkd3d_fence_t *f);

/* -- the Vulkan translation ----------------------------------------- */

typedef struct {
    VkSemaphoreTypeKHR      type;
    uint64_t                initial_value;
    VkPipelineStageFlags    wait_stage_mask;   /* the stage that waits */
    bool                    valid;
    const char             *name;
} wubu_vkd3d_fence_vk_t;

bool wubu_vkd3d_fence_vk(const wubu_vkd3d_fence_t *f, wubu_vkd3d_fence_vk_t *out);

#endif /* WUBU_VKD3D_FENCE_H */
