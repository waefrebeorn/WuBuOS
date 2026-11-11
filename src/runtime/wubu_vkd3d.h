/*
 * wubu_vkd3d.h -- VKD3D: the D3D12 personality over the Vulkan leg.
 *
 * Slice 1: the RESOURCE STATE translation core.
 *
 * D3D12 tracks a resource's state with a bit set and makes the app issue an
 * explicit barrier for every transition.  Vulkan has no such state field -- it
 * infers the transition from pipeline stage + access masks and an image layout.
 * So the entire correctness question of a D3D12-on-Vulkan layer is: does every
 * D3D12 state map to the right (stage, access, layout) triple, and does every
 * barrier get emitted at the right point?
 *
 * This module owns that translation.  It is pure C11 + the Vulkan headers; it
 * opens no device, so the whole mapping table is unit-testable without a GPU.
 * The device/command layer comes in a later slice.
 *
 * Opaque structs, minimal includes, C11.
 */
#ifndef WUBU_VKD3D_H
#define WUBU_VKD3D_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

/* -- D3D12_RESOURCE_STATES (the real d3d12.h bit values) ------------- */
typedef enum {
    D3D12_RESOURCE_STATE_COMMON                    = 0x00000,
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x00001,
    D3D12_RESOURCE_STATE_INDEX_BUFFER               = 0x00002,
    D3D12_RESOURCE_STATE_RENDER_TARGET              = 0x00004,
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS           = 0x00008,
    D3D12_RESOURCE_STATE_DEPTH_WRITE                = 0x00010,
    D3D12_RESOURCE_STATE_DEPTH_READ                 = 0x00020,
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE  = 0x00040,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE      = 0x00080,
    D3D12_RESOURCE_STATE_STREAM_OUT                 = 0x00100,
    D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT          = 0x00200,
    D3D12_RESOURCE_STATE_COPY_DEST                  = 0x00400,
    D3D12_RESOURCE_STATE_COPY_SOURCE                = 0x00800,
    D3D12_RESOURCE_STATE_RESOLVE_DEST               = 0x01000,
    D3D12_RESOURCE_STATE_RESOLVE_SOURCE             = 0x02000,
    D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE        = 0x100000,
    D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE = 0x400000,
    /* composites (d3d12.h defines these as the OR of the above) */
    D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE = 0x000C0,
    D3D12_RESOURCE_STATE_GENERIC_READ =
        0x00001 | 0x00002 | 0x00040 | 0x00080 | 0x00200 | 0x00800,
    /* D3D12_RESOURCE_STATE_PRESENT is an alias of COMMON */
    D3D12_RESOURCE_STATE_PRESENT = 0x00000
} D3D12_RESOURCE_STATES;

/* -- D3D12 feature levels (the real d3d12.h values) ------------------ */
typedef enum {
    D3D12_FEATURE_LEVEL_1_0_CORE = 0x1000,
    D3D12_FEATURE_LEVEL_9_1      = 0x9100,
    D3D12_FEATURE_LEVEL_9_2      = 0x9200,
    D3D12_FEATURE_LEVEL_9_3      = 0x9300,
    D3D12_FEATURE_LEVEL_10_0     = 0xa000,
    D3D12_FEATURE_LEVEL_10_1     = 0xa100,
    D3D12_FEATURE_LEVEL_11_0     = 0xb000,
    D3D12_FEATURE_LEVEL_11_1     = 0xb100,
    D3D12_FEATURE_LEVEL_12_0     = 0xc000,
    D3D12_FEATURE_LEVEL_12_1     = 0xc100,
    D3D12_FEATURE_LEVEL_12_2     = 0xc200
} D3D12_FEATURE_LEVEL;

/* -- the translated state ------------------------------------------- */

typedef struct {
    VkPipelineStageFlags stage;       /* VK_PIPELINE_STAGE_* bits */
    VkAccessFlags        access;      /* VK_ACCESS_* bits */
    VkImageLayout        image_layout;/* valid only when image_layout_valid */
    bool                 image_layout_valid;
    bool                 is_buffer;   /* true: buffer, false: image */
    const char          *name;        /* for the log / descriptor dump */
} wubu_vkd3d_state_t;

/* Map a D3D12 resource state (a bit set, so composites are allowed) to its
 * Vulkan equivalent.  Unknown bits are REJECTED rather than guessed at --
 * silently accepting a state we did not map is how a D3D12 layer corrupts
 * rendering.  Returns false when any bit is unknown. */
bool wubu_vkd3d_state_map(uint32_t d3d12_state, wubu_vkd3d_state_t *out);

/* The mask of every state bit this module knows. */
uint32_t wubu_vkd3d_known_states(void);

/* Human-readable name for a single-bit state, or NULL. */
const char *wubu_vkd3d_state_name(uint32_t d3d12_state);

/* -- barriers ------------------------------------------------------- */

/* A D3D12-style barrier, keyed by a caller-assigned resource id so this
 * module stays pointer-free (and therefore testable). */
typedef struct {
    uint32_t resource_id;
    uint32_t before;
    uint32_t after;
    bool     is_buffer;
} wubu_vkd3d_barrier_t;

/* Translate a barrier list into Vulkan barriers.
 * The emitted VkImageMemoryBarrier/VkBufferMemoryBarrier carry
 * VK_NULL_HANDLE: the caller binds the real resource, exactly as a
 * D3D12-on-Vulkan layer must (the barrier is built before the resource handle
 * is looked up, and patched in afterwards).
 * Returns the number of barriers written, or -1 on an unmappable state. */
int wubu_vkd3d_barriers(const wubu_vkd3d_barrier_t *bars, uint32_t n,
                        VkBufferMemoryBarrier *bmb, uint32_t *n_bmb,
                        VkImageMemoryBarrier *imb, uint32_t *n_imb);

/* -- per-resource state tracking ------------------------------------ */

typedef struct wubu_vkd3d_tracker wubu_vkd3d_tracker_t;

wubu_vkd3d_tracker_t *wubu_vkd3d_tracker_create(void);
void wubu_vkd3d_tracker_destroy(wubu_vkd3d_tracker_t *t);

/* Track a transition.  Returns 0 on success, -1 when `from` does not match
 * the state actually tracked for that resource (an ILLEGAL transition -- D3D12
 * requires the app to declare the current state, so a mismatch means the app's
 * own bookkeeping is wrong), and 1 for a no-op (from == to). */
int  wubu_vkd3d_tracker_transition(wubu_vkd3d_tracker_t *t, uint32_t resource_id,
                                   uint32_t from, uint32_t to);
uint32_t wubu_vkd3d_tracker_state(const wubu_vkd3d_tracker_t *t, uint32_t resource_id);
int  wubu_vkd3d_tracker_count(const wubu_vkd3d_tracker_t *t);

/* -- feature level ladder ------------------------------------------ */

/* Is this feature level at or below the given one? */
bool wubu_vkd3d_feature_level_le(uint32_t level, uint32_t ceiling);
/* The feature-level name, or "unknown". */
const char *wubu_vkd3d_feature_level_name(uint32_t level);

#endif /* WUBU_VKD3D_H */
