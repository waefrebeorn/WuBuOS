/*
 * wubu_vkd3d_swapchain.h -- VKD3D slice 7: the swapchain / present path.
 *
 * D3D12 presents through IDXGISwapChain3; Vulkan presents through a swapchain
 * plus vkQueuePresentKHR.  The two APIs agree on the SHAPE (a ring of images,
 * a present with a sync interval, a frame-latency cap) but the WORDS differ,
 * and three mappings are the whole game:
 *
 *   swap effect      FLIP_SEQUENTIAL -> FIFO_KHR    (vsync, tear-free, queue)
 *                    FLIP_DISCARD    -> MAILBOX_KHR  (flip, tear-free, lowest latency)
 *                    DISCARD         -> IMMEDIATE_KHR (legacy bitblt)
 *
 *   sync interval    >= 1            -> FIFO_KHR (vsync; N > 1 has NO Vulkan
 *                                      equivalent, so the layer must throttle)
 *                    0 + tearing     -> IMMEDIATE_KHR
 *                    0, no tearing   -> MAILBOX_KHR (tear-free, lowest latency)
 *
 *   frame latency    SetMaximumFrameLatency(N) means at most N frames in
 *                    flight, which maps onto how many swapchain images exist:
 *                    minImageCount >= N + 1, and never below 3.
 *
 * Vulkan cannot express "sync every Nth frame": it has FIFO (every frame) and
 * IMMEDIATE (no sync).  So a D3D12 SyncInterval of 2/3/4 is an APP-LEVEL
 * frame limiter on top of FIFO -- and the layer must say so rather than
 * pretending the modes match.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_SWAPCHAIN_H
#define WUBU_VKD3D_SWAPCHAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

/* DXGI_SWAP_EFFECT.  DISCARD is the legacy bitblt path; the two FLIP modes are
 * the modern one and require a buffer count of at least 2. */
typedef enum {
    VKD3D_SWAP_EFFECT_DISCARD = 0,
    VKD3D_SWAP_EFFECT_FLIP_SEQUENTIAL = 1,
    VKD3D_SWAP_EFFECT_FLIP_DISCARD = 2
} wubu_vkd3d_swap_effect_t;

typedef struct {
    uint32_t                  width;
    uint32_t                  height;
    uint32_t                  buffer_count;
    wubu_vkd3d_swap_effect_t  effect;
    bool                      tearing_allowed;   /* DXGI_PRESENT_ALLOW_TEARING */
    uint32_t                  sync_interval;     /* DXGI SyncInterval, 0..4 */
    uint32_t                  max_frame_latency; /* SetMaximumFrameLatency */
} wubu_vkd3d_swapchain_desc_t;

/* The Vulkan side of the translation. */
typedef struct {
    VkPresentModeKHR          present_mode;
    VkSwapchainCreateInfoKHR  create_info;       /* image count + usage */
    bool                      valid;
    bool                      needs_app_throttle; /* SyncInterval > 1 */
    const char               *why;
} wubu_vkd3d_swapchain_vk_t;

/* Validate the desc.  Returns NULL when legal, else a static reason. */
const char *wubu_vkd3d_swapchain_validate(const wubu_vkd3d_swapchain_desc_t *d);

/* The minimum number of swapchain images this desc implies. */
uint32_t wubu_vkd3d_swapchain_min_image_count(const wubu_vkd3d_swapchain_desc_t *d);

/* Translate to Vulkan.  Returns false when the desc is invalid. */
bool wubu_vkd3d_swapchain_vk(const wubu_vkd3d_swapchain_desc_t *d,
                             wubu_vkd3d_swapchain_vk_t *out);

/* -- frame latency -------------------------------------------------- */

typedef struct {
    uint32_t max_latency;   /* SetMaximumFrameLatency */
    uint32_t in_flight;     /* frames submitted but not yet completed */
} wubu_vkd3d_frame_latency_t;

void wubu_vkd3d_frame_latency_init(wubu_vkd3d_frame_latency_t *f,
                                   uint32_t max_latency);

bool wubu_vkd3d_frame_latency_can_present(const wubu_vkd3d_frame_latency_t *f);
void wubu_vkd3d_frame_latency_submit(wubu_vkd3d_frame_latency_t *f);
void wubu_vkd3d_frame_latency_complete(wubu_vkd3d_frame_latency_t *f);
uint32_t wubu_vkd3d_frame_latency_in_flight(const wubu_vkd3d_frame_latency_t *f);

#endif /* WUBU_VKD3D_SWAPCHAIN_H */
