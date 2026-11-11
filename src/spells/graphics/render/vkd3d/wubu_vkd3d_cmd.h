/*
 * wubu_vkd3d_cmd.h -- VKD3D slice 3: D3D12 command allocators and lists.
 *
 * D3D12's command model has four states and one hard safety rule:
 *
 *   CREATED --recording--> CLOSED --execute--> EXECUTING --fence fires--> CLOSED
 *      ^                                                              |
 *      +------------------------- reset ------------------------------+
 *
 * The rule: a list's memory must NOT be reused while the GPU may still be
 * reading it.  D3D12 makes this the app's job via fences, and getting it wrong
 * is a use-after-free that shows up as corrupted frames or a crash several
 * frames later -- the worst kind of bug.  Vulkan has the same rule
 * (VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, and the pool may not be
 * reset while any buffer from it is in flight), so the state machine here maps
 * onto it directly.
 *
 * Also modelled: D3D12 has DIRECT/BUNDLE/COMPUTE/COPY list types.  Vulkan has
 * only PRIMARY/SECONDARY buffer levels -- the compute/copy distinction is a
 * QUEUE FAMILY, not a buffer level, and a D3D12 bundle is a secondary buffer.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_CMD_H
#define WUBU_VKD3D_CMD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

typedef enum {
    VKD3D_CMD_DIRECT = 0,   /* graphics */
    VKD3D_CMD_BUNDLE,       /* a secondary buffer, replayed by a DIRECT list */
    VKD3D_CMD_COMPUTE,
    VKD3D_CMD_COPY,
    VKD3D_CMD_COUNT
} wubu_vkd3d_cmd_type_t;

typedef enum {
    VKD3D_LIST_CREATED = 0,   /* recording is allowed */
    VKD3D_LIST_CLOSED,        /* ready to execute */
    VKD3D_LIST_EXECUTING,     /* in flight; memory must not be reused */
    VKD3D_LIST_INVALID        /* its allocator was reset; must be re-created */
} wubu_vkd3d_list_state_t;

typedef struct wubu_vkd3d_cmd_alloc wubu_vkd3d_cmd_alloc_t;
typedef struct wubu_vkd3d_cmd_list  wubu_vkd3d_cmd_list_t;

/* An allocator owns the command memory for the lists created from it. */
wubu_vkd3d_cmd_alloc_t *wubu_vkd3d_cmd_alloc_create(wubu_vkd3d_cmd_type_t type);
void wubu_vkd3d_cmd_alloc_destroy(wubu_vkd3d_cmd_alloc_t *a);

wubu_vkd3d_cmd_type_t wubu_vkd3d_cmd_alloc_type(const wubu_vkd3d_cmd_alloc_t *a);
int  wubu_vkd3d_cmd_alloc_lists(const wubu_vkd3d_cmd_alloc_t *a);
/* The allocator's next fence value (monotonic; the GPU signals it). */
uint64_t wubu_vkd3d_cmd_alloc_next_fence(const wubu_vkd3d_cmd_alloc_t *a);

/* Reset an allocator.  Fails while ANY list created from it is EXECUTING --
 * reusing command memory the GPU may still be reading is a use-after-free.
 * On success every list from this allocator becomes INVALID. */
int wubu_vkd3d_cmd_alloc_reset(wubu_vkd3d_cmd_alloc_t *a);

/* Create a list against an allocator of the SAME type.  The list starts in
 * CREATED (recording). */
wubu_vkd3d_cmd_list_t *wubu_vkd3d_cmd_list_create(wubu_vkd3d_cmd_alloc_t *a);
void wubu_vkd3d_cmd_list_destroy(wubu_vkd3d_cmd_list_t *l);

wubu_vkd3d_list_state_t wubu_vkd3d_cmd_list_state(const wubu_vkd3d_cmd_list_t *l);
wubu_vkd3d_cmd_type_t   wubu_vkd3d_cmd_list_type(const wubu_vkd3d_cmd_list_t *l);
uint64_t wubu_vkd3d_cmd_list_fence(const wubu_vkd3d_cmd_list_t *l);

/* CREATED -> CLOSED.  Closing an already-closed list is an error: D3D12
 * requires a reset before a list can be recorded into again. */
int wubu_vkd3d_cmd_list_close(wubu_vkd3d_cmd_list_t *l);

/* CLOSED -> CREATED.  Rejected while EXECUTING. */
int wubu_vkd3d_cmd_list_reset(wubu_vkd3d_cmd_list_t *l);

/* CLOSED -> EXECUTING.  A list must be closed before it can be executed. */
int wubu_vkd3d_cmd_list_execute(wubu_vkd3d_cmd_list_t *l);

/* EXECUTING -> CLOSED: the fence for this list's execution has fired. */
int wubu_vkd3d_cmd_list_complete(wubu_vkd3d_cmd_list_t *l);

/* -- the Vulkan translation ----------------------------------------- */

typedef struct {
    VkCommandBufferLevel     level;         /* PRIMARY or SECONDARY */
    VkCommandPoolCreateFlags pool_flags;
    VkQueueFlags             queue_flags;    /* which queue family it runs on */
    bool                     valid;
    const char              *name;
} wubu_vkd3d_cmd_vk_t;

bool wubu_vkd3d_cmd_vk(wubu_vkd3d_cmd_type_t type, wubu_vkd3d_cmd_vk_t *out);

/* The pool flags every D3D12 allocator needs: D3D12 lets the app reset a
 * SINGLE list, which Vulkan only permits with this flag. */
VkCommandPoolCreateFlags wubu_vkd3d_cmd_pool_flags(void);

#endif /* WUBU_VKD3D_CMD_H */
