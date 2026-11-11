/*
 * wubu_vkd3d_cmd.c -- VKD3D slice 3: D3D12 command allocators and lists.
 *
 * See wubu_vkd3d_cmd.h.  The state machine IS the module: the transitions are
 * the safety rule, and every rejected transition is a use-after-free prevented.
 */
#include "wubu_vkd3d_cmd.h"

#include <stdlib.h>
#include <string.h>

#define VKD3D_MAX_LISTS 64

struct wubu_vkd3d_cmd_alloc {
    uint32_t               id;
    wubu_vkd3d_cmd_type_t  type;
    uint64_t               next_fence;
    wubu_vkd3d_cmd_list_t *lists[VKD3D_MAX_LISTS];
    int                    n_lists;
};

struct wubu_vkd3d_cmd_list {
    uint32_t                 id;
    wubu_vkd3d_cmd_alloc_t  *alloc;
    wubu_vkd3d_cmd_type_t    type;
    wubu_vkd3d_list_state_t  state;
    uint64_t                 fence;
};

static uint32_t g_next_alloc_id = 1;
static uint32_t g_next_list_id  = 1;

/* -- allocator ------------------------------------------------------ */

wubu_vkd3d_cmd_alloc_t *wubu_vkd3d_cmd_alloc_create(wubu_vkd3d_cmd_type_t type)
{
    if (type >= VKD3D_CMD_COUNT) return NULL;
    wubu_vkd3d_cmd_alloc_t *a = calloc(1, sizeof(*a));
    if (!a) return NULL;
    a->id = g_next_alloc_id++;
    a->type = type;
    a->next_fence = 1;         /* fence 0 means "never executed" */
    return a;
}

static void alloc_forget(wubu_vkd3d_cmd_alloc_t *a, const wubu_vkd3d_cmd_list_t *l)
{
    for (int i = 0; i < a->n_lists; i++) {
        if (a->lists[i] == l) {
            a->lists[i] = a->lists[a->n_lists - 1];
            a->n_lists--;
            return;
        }
    }
}

void wubu_vkd3d_cmd_alloc_destroy(wubu_vkd3d_cmd_alloc_t *a)
{
    if (!a) return;
    /* Destroy the lists this allocator owns, oldest first.  Their type is the
     * allocator's own, which cannot have changed, so re-creating them is the
     * app's job -- exactly as in D3D12. */
    while (a->n_lists > 0) {
        wubu_vkd3d_cmd_list_t *l = a->lists[a->n_lists - 1];
        a->n_lists--;
        free(l);
    }
    free(a);
}

wubu_vkd3d_cmd_type_t wubu_vkd3d_cmd_alloc_type(const wubu_vkd3d_cmd_alloc_t *a)
{
    return a ? a->type : VKD3D_CMD_COUNT;
}

int wubu_vkd3d_cmd_alloc_lists(const wubu_vkd3d_cmd_alloc_t *a)
{
    return a ? a->n_lists : 0;
}

uint64_t wubu_vkd3d_cmd_alloc_next_fence(const wubu_vkd3d_cmd_alloc_t *a)
{
    return a ? a->next_fence : 0;
}

int wubu_vkd3d_cmd_alloc_reset(wubu_vkd3d_cmd_alloc_t *a)
{
    if (!a) return -1;
    /* THE SAFETY RULE.  Resetting the allocator would hand its command memory
     * back for reuse while the GPU may still be reading it. */
    for (int i = 0; i < a->n_lists; i++)
        if (a->lists[i]->state == VKD3D_LIST_EXECUTING) return -1;

    for (int i = 0; i < a->n_lists; i++)
        a->lists[i]->state = VKD3D_LIST_INVALID;
    return 0;
}

/* -- list ----------------------------------------------------------- */

wubu_vkd3d_cmd_list_t *wubu_vkd3d_cmd_list_create(wubu_vkd3d_cmd_alloc_t *a)
{
    if (!a) return NULL;
    if (a->n_lists >= VKD3D_MAX_LISTS) return NULL;

    wubu_vkd3d_cmd_list_t *l = calloc(1, sizeof(*l));
    if (!l) return NULL;
    l->id = g_next_list_id++;
    l->alloc = a;
    l->type = a->type;              /* a list's type is its allocator's */
    l->state = VKD3D_LIST_CREATED;
    l->fence = 0;
    a->lists[a->n_lists++] = l;
    return l;
}

void wubu_vkd3d_cmd_list_destroy(wubu_vkd3d_cmd_list_t *l)
{
    if (!l) return;
    if (l->alloc) alloc_forget(l->alloc, l);
    free(l);
}

wubu_vkd3d_list_state_t wubu_vkd3d_cmd_list_state(const wubu_vkd3d_cmd_list_t *l)
{
    return l ? l->state : VKD3D_LIST_INVALID;
}

wubu_vkd3d_cmd_type_t wubu_vkd3d_cmd_list_type(const wubu_vkd3d_cmd_list_t *l)
{
    return l ? l->type : VKD3D_CMD_COUNT;
}

uint64_t wubu_vkd3d_cmd_list_fence(const wubu_vkd3d_cmd_list_t *l)
{
    return l ? l->fence : 0;
}

int wubu_vkd3d_cmd_list_close(wubu_vkd3d_cmd_list_t *l)
{
    if (!l) return -1;
    /* An already-closed list cannot be closed again: D3D12 requires a reset
     * before the list can be recorded into again. */
    if (l->state != VKD3D_LIST_CREATED) return -1;
    l->state = VKD3D_LIST_CLOSED;
    return 0;
}

int wubu_vkd3d_cmd_list_reset(wubu_vkd3d_cmd_list_t *l)
{
    if (!l) return -1;
    /* Rejected while EXECUTING: that would reuse command memory the GPU may
     * still be reading.  An INVALID list (its allocator was reset) must be
     * re-created, not reset. */
    if (l->state == VKD3D_LIST_EXECUTING) return -1;
    if (l->state != VKD3D_LIST_CLOSED) return -1;
    l->state = VKD3D_LIST_CREATED;
    l->fence = 0;
    return 0;
}

int wubu_vkd3d_cmd_list_execute(wubu_vkd3d_cmd_list_t *l)
{
    if (!l || !l->alloc) return -1;
    /* A list must be closed before it can be executed. */
    if (l->state != VKD3D_LIST_CLOSED) return -1;
    l->fence = l->alloc->next_fence++;
    l->state = VKD3D_LIST_EXECUTING;
    return 0;
}

int wubu_vkd3d_cmd_list_complete(wubu_vkd3d_cmd_list_t *l)
{
    if (!l) return -1;
    if (l->state != VKD3D_LIST_EXECUTING) return -1;
    l->state = VKD3D_LIST_CLOSED;
    return 0;
}

/* -- the Vulkan translation ----------------------------------------- */

bool wubu_vkd3d_cmd_vk(wubu_vkd3d_cmd_type_t type, wubu_vkd3d_cmd_vk_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));

    switch (type) {
    case VKD3D_CMD_DIRECT:
        out->level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        out->queue_flags = VK_QUEUE_GRAPHICS_BIT;
        out->name = "DIRECT";
        break;
    case VKD3D_CMD_BUNDLE:
        /* A D3D12 bundle is a SECONDARY buffer, replayed by a DIRECT list. */
        out->level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        out->queue_flags = VK_QUEUE_GRAPHICS_BIT;
        out->name = "BUNDLE";
        break;
    case VKD3D_CMD_COMPUTE:
        /* Vulkan has no compute command buffer level -- the compute/copy
         * distinction is a QUEUE FAMILY, not a buffer level. */
        out->level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        out->queue_flags = VK_QUEUE_COMPUTE_BIT;
        out->name = "COMPUTE";
        break;
    case VKD3D_CMD_COPY:
        out->level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        out->queue_flags = VK_QUEUE_TRANSFER_BIT;
        out->name = "COPY";
        break;
    default:
        return false;
    }

    out->pool_flags = wubu_vkd3d_cmd_pool_flags();
    out->valid = true;
    return true;
}

VkCommandPoolCreateFlags wubu_vkd3d_cmd_pool_flags(void)
{
    /* D3D12 lets the app reset a SINGLE list.  Vulkan only permits that when
     * the pool was created with this flag -- so every D3D12 allocator needs
     * it. */
    return VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
}
