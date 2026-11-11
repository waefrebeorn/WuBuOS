/*
 * wubu_vkd3d.c -- VKD3D slice 1: the D3D12 resource-state translation core.
 *
 * See wubu_vkd3d.h for the contract.  Every mapping below is the real Vulkan
 * equivalent of the D3D12 state, and every unknown bit is rejected: a D3D12
 * layer that silently guesses at an unmapped state corrupts rendering.
 */
#include "wubu_vkd3d.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t      bit;
    VkPipelineStageFlags stage;
    VkAccessFlags access;
    VkImageLayout layout;       /* used only when has_layout */
    bool          has_layout;
    bool          is_buffer;
    const char   *name;
} state_entry_t;

/* The D3D12 -> Vulkan state table.
 *
 * The stage/access masks are deliberately the WIDEST set that is still correct
 * for the state: D3D12's states are coarser than Vulkan's, so a state maps to
 * every stage that can legally touch the resource in that state.  Overlapping
 * masks are safe (Vulkan just inserts a wider barrier); a too-narrow mask is a
 * race. */
static const state_entry_t k_states[] = {
    { D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
      VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
      VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
      VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
      VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, false, false, "VERTEX_AND_CONSTANT_BUFFER" },

    { D3D12_RESOURCE_STATE_INDEX_BUFFER,
      VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
      VK_ACCESS_INDEX_READ_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, false, false, "INDEX_BUFFER" },

    { D3D12_RESOURCE_STATE_RENDER_TARGET,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true, false, "RENDER_TARGET" },

    { D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
      VK_IMAGE_LAYOUT_GENERAL, true, false, "UNORDERED_ACCESS" },

    { D3D12_RESOURCE_STATE_DEPTH_WRITE,
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, true, false, "DEPTH_WRITE" },

    { D3D12_RESOURCE_STATE_DEPTH_READ,
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, true, false, "DEPTH_READ" },

    { D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
      VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
      VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
      VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
      VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true, false, "NON_PIXEL_SHADER_RESOURCE" },

    { D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true, false, "PIXEL_SHADER_RESOURCE" },

    { D3D12_RESOURCE_STATE_STREAM_OUT,
      VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
      VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
      VK_IMAGE_LAYOUT_UNDEFINED, false, true, "STREAM_OUT" },

    { D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
      VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, false, true, "INDIRECT_ARGUMENT" },

    { D3D12_RESOURCE_STATE_COPY_DEST,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true, false, "COPY_DEST" },

    { D3D12_RESOURCE_STATE_COPY_SOURCE,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, true, false, "COPY_SOURCE" },

    { D3D12_RESOURCE_STATE_RESOLVE_DEST,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true, false, "RESOLVE_DEST" },

    { D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, true, false, "RESOLVE_SOURCE" },

#ifdef VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR
    { D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE,
      VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
      VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
      VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, true, false,
      "SHADING_RATE_SOURCE" },
#endif

#ifdef VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
    { D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
#ifdef VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
      VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
#endif
#ifdef VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV
      VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV |
#endif
      0,
      VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR |
      VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
      VK_IMAGE_LAYOUT_UNDEFINED, false, true,
      "RAYTRACING_ACCELERATION_STRUCTURE" },
#endif
};

uint32_t wubu_vkd3d_known_states(void)
{
    uint32_t m = D3D12_RESOURCE_STATE_COMMON;   /* 0 is always legal */
    for (size_t i = 0; i < sizeof(k_states) / sizeof(k_states[0]); i++)
        m |= k_states[i].bit;
    return m;
}

const char *wubu_vkd3d_state_name(uint32_t d3d12_state)
{
    for (size_t i = 0; i < sizeof(k_states) / sizeof(k_states[0]); i++)
        if (k_states[i].bit == d3d12_state)
            return k_states[i].name;
    if (d3d12_state == D3D12_RESOURCE_STATE_COMMON) return "COMMON/PRESENT";
    if (d3d12_state == D3D12_RESOURCE_STATE_GENERIC_READ) return "GENERIC_READ";
    if (d3d12_state == D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) return "ALL_SHADER_RESOURCE";
    return NULL;
}

bool wubu_vkd3d_state_map(uint32_t d3d12_state, wubu_vkd3d_state_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->name = NULL;

    /* COMMON / PRESENT (0) is a legal state on its own: the resource is idle.
     * Vulkan has no "idle" layout for an image, so it maps to GENERAL. */
    if (d3d12_state == D3D12_RESOURCE_STATE_COMMON) {
        out->stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        out->access = 0;
        out->image_layout = VK_IMAGE_LAYOUT_GENERAL;
        out->image_layout_valid = true;
        out->name = "COMMON/PRESENT";
        return true;
    }

    if (d3d12_state & ~wubu_vkd3d_known_states()) return false;  /* unknown bit */

    int matched = 0;
    for (size_t i = 0; i < sizeof(k_states) / sizeof(k_states[0]); i++) {
        const state_entry_t *e = &k_states[i];
        if (!(d3d12_state & e->bit)) continue;
        out->stage |= e->stage;
        out->access |= e->access;
        if (e->has_layout) {
            /* The layout is the most specific one named by the state set; a
             * composite never carries two different layouts, because a
             * resource can only be in one layout at a time. */
            out->image_layout = e->layout;
            out->image_layout_valid = true;
        }
        if (e->is_buffer) out->is_buffer = true;
        if (!out->name) out->name = e->name;
        matched++;
    }
    return matched > 0;
}

/* -- barriers ------------------------------------------------------- */

int wubu_vkd3d_barriers(const wubu_vkd3d_barrier_t *bars, uint32_t n,
                        VkMemoryBarrier *mb, uint32_t *n_mb,
                        VkBufferMemoryBarrier *bmb, uint32_t *n_bmb,
                        VkImageMemoryBarrier *imb, uint32_t *n_imb)
{
    if (n_mb) *n_mb = 0;
    if (n_bmb) *n_bmb = 0;
    if (n_imb) *n_imb = 0;
    if (!bars && n > 0) return -1;

    int written = 0;
    for (uint32_t i = 0; i < n; i++) {
        const wubu_vkd3d_barrier_t *b = &bars[i];

        /* A UAV barrier is NOT a resource barrier: it orders every UAV access
         * against every other one, so it is a GLOBAL memory barrier -- and it
         * must NOT touch the layout, because a layout change here would cost a
         * render-target resolve on some drivers for no benefit. */
        if (b->type == VKD3D_BARRIER_UAV) {
            if (!mb || !n_mb || *n_mb >= 4096) return -1;
            VkMemoryBarrier *m = &mb[(*n_mb)++];
            memset(m, 0, sizeof(*m));
            m->sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            m->srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            m->dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            written++;
            continue;
        }

        wubu_vkd3d_state_t before, after;
        if (!wubu_vkd3d_state_map(b->before, &before)) return -1;
        if (!wubu_vkd3d_state_map(b->after, &after)) return -1;

        /* ALIASING keeps the layout: the resource is reused at an address
         * another resource occupied, so only the access masks matter. */
        VkImageLayout keep = before.image_layout_valid ? before.image_layout
                                                      : VK_IMAGE_LAYOUT_UNDEFINED;

        if (b->is_buffer) {
            if (!bmb || !n_bmb || *n_bmb >= 4096) return -1;
            VkBufferMemoryBarrier *o = &bmb[(*n_bmb)++];
            memset(o, 0, sizeof(*o));
            o->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            o->srcAccessMask = before.access;
            o->dstAccessMask = after.access;
            o->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            o->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            o->buffer = VK_NULL_HANDLE;      /* the caller binds the resource */
        } else {
            if (!imb || !n_imb || *n_imb >= 4096) return -1;
            VkImageMemoryBarrier *o = &imb[(*n_imb)++];
            memset(o, 0, sizeof(*o));
            o->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            o->srcAccessMask = before.access;
            o->dstAccessMask = after.access;
            o->oldLayout = (b->type == VKD3D_BARRIER_ALIASING)
                               ? keep
                               : (before.image_layout_valid ? before.image_layout
                                                            : VK_IMAGE_LAYOUT_UNDEFINED);
            o->newLayout = (b->type == VKD3D_BARRIER_ALIASING)
                               ? keep
                               : (after.image_layout_valid ? after.image_layout
                                                           : VK_IMAGE_LAYOUT_UNDEFINED);
            o->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            o->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            o->image = VK_NULL_HANDLE;       /* the caller binds the resource */
        }
        written++;
    }
    return written;
}

/* -- tracker -------------------------------------------------------- */

#define VKD3D_TRACKER_MAX 256

struct wubu_vkd3d_tracker {
    uint32_t id[VKD3D_TRACKER_MAX];
    uint32_t state[VKD3D_TRACKER_MAX];
    int      n;
};

wubu_vkd3d_tracker_t *wubu_vkd3d_tracker_create(void)
{
    wubu_vkd3d_tracker_t *t = calloc(1, sizeof(*t));
    return t;
}

void wubu_vkd3d_tracker_destroy(wubu_vkd3d_tracker_t *t)
{
    free(t);
}

static int tracker_slot(wubu_vkd3d_tracker_t *t, uint32_t id)
{
    for (int i = 0; i < t->n; i++)
        if (t->id[i] == id) return i;
    return -1;
}

int wubu_vkd3d_tracker_transition(wubu_vkd3d_tracker_t *t, uint32_t resource_id,
                                  uint32_t from, uint32_t to)
{
    if (!t) return -1;
    int slot = tracker_slot(t, resource_id);
    uint32_t cur = (slot >= 0) ? t->state[slot] : D3D12_RESOURCE_STATE_COMMON;

    /* D3D12 requires the app to declare the CURRENT state in the barrier.  If
     * that does not match what we tracked, the app's bookkeeping is wrong and
     * the transition is illegal -- silently accepting it would hide the bug
     * until it showed up as corrupted output. */
    if (from != cur) return -1;
    if (from == to) return 1;                    /* no-op */

    if (slot < 0) {
        if (t->n >= VKD3D_TRACKER_MAX) return -1;
        slot = t->n++;
        t->id[slot] = resource_id;
    }
    t->state[slot] = to;
    return 0;
}

uint32_t wubu_vkd3d_tracker_state(const wubu_vkd3d_tracker_t *t, uint32_t resource_id)
{
    if (!t) return D3D12_RESOURCE_STATE_COMMON;
    int slot = tracker_slot((wubu_vkd3d_tracker_t *)t, resource_id);
    return (slot >= 0) ? t->state[slot] : D3D12_RESOURCE_STATE_COMMON;
}

int wubu_vkd3d_tracker_count(const wubu_vkd3d_tracker_t *t)
{
    return t ? t->n : 0;
}

/* -- feature level -------------------------------------------------- */

bool wubu_vkd3d_feature_level_le(uint32_t level, uint32_t ceiling)
{
    return level <= ceiling;
}

const char *wubu_vkd3d_feature_level_name(uint32_t level)
{
    switch (level) {
    case D3D12_FEATURE_LEVEL_1_0_CORE: return "1.0 CORE";
    case D3D12_FEATURE_LEVEL_9_1:  return "9.1";
    case D3D12_FEATURE_LEVEL_9_2:  return "9.2";
    case D3D12_FEATURE_LEVEL_9_3:  return "9.3";
    case D3D12_FEATURE_LEVEL_10_0: return "10.0";
    case D3D12_FEATURE_LEVEL_10_1: return "10.1";
    case D3D12_FEATURE_LEVEL_11_0: return "11.0";
    case D3D12_FEATURE_LEVEL_11_1: return "11.1";
    case D3D12_FEATURE_LEVEL_12_0: return "12.0";
    case D3D12_FEATURE_LEVEL_12_1: return "12.1";
    case D3D12_FEATURE_LEVEL_12_2: return "12.2";
    default: return "unknown";
    }
}
