/*
 * wubu_vkd3d_pso.c -- VKD3D slice 4: pipeline state objects.
 *
 * See wubu_vkd3d_pso.h.  The cache key is a hash of the canonical
 * serialisation of the WHOLE desc, and a hash match alone is not enough: the
 * full desc is stored and compared, so a hash collision is a miss rather than
 * a wrong pipeline.
 */
#include "wubu_vkd3d_pso.h"

#include <stdlib.h>
#include <string.h>

/* -- canonical serialisation + FNV-1a -------------------------------- */

typedef struct { uint8_t *p; size_t left; } wbuf_t;

static void wb_u32(wbuf_t *w, uint32_t v)
{
    if (w->left < 4) return;
    w->p[0] = (uint8_t)(v & 0xFF);
    w->p[1] = (uint8_t)((v >> 8) & 0xFF);
    w->p[2] = (uint8_t)((v >> 16) & 0xFF);
    w->p[3] = (uint8_t)((v >> 24) & 0xFF);
    w->p += 4;
    w->left -= 4;
}

static void wb_i32(wbuf_t *w, int32_t v) { wb_u32(w, (uint32_t)v); }

static void wb_f32(wbuf_t *w, float v)
{
    uint32_t u;
    memcpy(&u, &v, 4);
    wb_u32(w, u);
}

static void wb_bool(wbuf_t *w, bool v) { wb_u32(w, v ? 1u : 0u); }

#define PSO_KEY_MAX_BYTES 1024

uint32_t wubu_vkd3d_pso_key(const wubu_vkd3d_pso_desc_t *d)
{
    if (!d) return 0;

    uint8_t buf[PSO_KEY_MAX_BYTES];
    wbuf_t w = { buf, sizeof(buf) };

    wb_u32(&w, d->root_signature_hash);
    wb_u32(&w, d->vs_hash);
    wb_u32(&w, d->ps_hash);
    wb_u32(&w, d->gs_hash);
    wb_u32(&w, d->hs_hash);
    wb_u32(&w, d->ds_hash);
    wb_u32(&w, d->cs_hash);

    wb_bool(&w, d->blend_enable);
    wb_u32(&w, d->blend_src_color);
    wb_u32(&w, d->blend_dst_color);
    wb_u32(&w, d->blend_op_color);
    wb_u32(&w, d->blend_src_alpha);
    wb_u32(&w, d->blend_dst_alpha);
    wb_u32(&w, d->blend_op_alpha);
    wb_u32(&w, d->render_target_write_mask);

    wb_u32(&w, d->fill_mode);
    wb_u32(&w, d->cull_mode);
    wb_bool(&w, d->front_counter_clockwise);
    wb_i32(&w, d->depth_bias);
    wb_f32(&w, d->depth_bias_clamp);
    wb_f32(&w, d->slope_scaled_depth_bias);
    wb_bool(&w, d->depth_clip_enable);

    wb_bool(&w, d->depth_enable);
    wb_bool(&w, d->depth_write_enable);
    wb_u32(&w, d->depth_func);
    wb_bool(&w, d->stencil_enable);
    wb_u32(&w, d->stencil_read_mask);
    wb_u32(&w, d->stencil_write_mask);

    wb_u32(&w, d->input_element_count);
    for (uint32_t i = 0; i < VKD3D_PSO_INPUT_ELEMENTS; i++) {
        wb_u32(&w, d->input_strides[i]);
        wb_u32(&w, d->input_formats[i]);
    }

    wb_u32(&w, d->primitive_topology);
    wb_u32(&w, d->rtv_count);
    for (uint32_t i = 0; i < VKD3D_PSO_RTVS; i++)
        wb_u32(&w, d->rtv_formats[i]);
    wb_u32(&w, d->dsv_format);
    wb_u32(&w, d->sample_count);
    wb_u32(&w, d->sample_mask);

    /* FNV-1a over exactly the bytes written. */
    uint32_t h = 2166136261u;
    size_t n = sizeof(buf) - w.left;
    for (size_t i = 0; i < n; i++) {
        h ^= buf[i];
        h *= 16777619u;
    }
    return h;
}

const char *wubu_vkd3d_pso_key_hex(const wubu_vkd3d_pso_desc_t *d,
                                   char *out, size_t n)
{
    if (!out || n < 9) return NULL;
    uint32_t k = wubu_vkd3d_pso_key(d);
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        out[i] = hex[(k >> (28 - i * 4)) & 0xF];
    out[8] = '\0';
    return out;
}

/* -- the cache ------------------------------------------------------ */

#define VKD3D_PSO_MAX 256

struct wubu_vkd3d_pso_cache {
    uint32_t capacity;
    uint32_t n;
    uint32_t next_id;
    uint64_t hits;
    uint64_t misses;
    uint32_t *keys;
    uint32_t *pipelines;
    wubu_vkd3d_pso_desc_t *descs;
};

wubu_vkd3d_pso_cache_t *wubu_vkd3d_pso_cache_create(uint32_t capacity)
{
    if (capacity == 0) capacity = 64;
    if (capacity > VKD3D_PSO_MAX) capacity = VKD3D_PSO_MAX;

    wubu_vkd3d_pso_cache_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->keys      = calloc(capacity, sizeof(uint32_t));
    c->pipelines = calloc(capacity, sizeof(uint32_t));
    c->descs     = calloc(capacity, sizeof(wubu_vkd3d_pso_desc_t));
    if (!c->keys || !c->pipelines || !c->descs) {
        free(c->keys); free(c->pipelines); free(c->descs); free(c);
        return NULL;
    }
    c->capacity = capacity;
    return c;
}

void wubu_vkd3d_pso_cache_destroy(wubu_vkd3d_pso_cache_t *c)
{
    if (!c) return;
    free(c->keys);
    free(c->pipelines);
    free(c->descs);
    free(c);
}

uint32_t wubu_vkd3d_pso_get(wubu_vkd3d_pso_cache_t *c,
                            const wubu_vkd3d_pso_desc_t *d, int *created)
{
    if (created) *created = 0;
    if (!c || !d) return 0;

    uint32_t key = wubu_vkd3d_pso_key(d);

    /* A hash match is only a CANDIDATE: the full desc must match too, or a
     * hash collision would hand back a pipeline built for different state --
     * the worst failure mode, because it renders subtly wrong instead of
     * failing loudly. */
    for (uint32_t i = 0; i < c->n; i++) {
        if (c->keys[i] == key && memcmp(&c->descs[i], d, sizeof(*d)) == 0) {
            c->hits++;
            return c->pipelines[i];
        }
    }

    c->misses++;
    if (c->n >= c->capacity) return 0;      /* full: reject, never evict */

    uint32_t id = ++c->next_id;
    c->keys[c->n] = key;
    c->pipelines[c->n] = id;
    memcpy(&c->descs[c->n], d, sizeof(*d));
    c->n++;
    if (created) *created = 1;
    return id;
}

int wubu_vkd3d_pso_release(wubu_vkd3d_pso_cache_t *c, uint32_t pipeline)
{
    if (!c || pipeline == 0) return -1;
    for (uint32_t i = 0; i < c->n; i++) {
        if (c->pipelines[i] != pipeline) continue;
        /* Swap-remove: the cache does not need ordering. */
        c->keys[i] = c->keys[c->n - 1];
        c->pipelines[i] = c->pipelines[c->n - 1];
        memcpy(&c->descs[i], &c->descs[c->n - 1], sizeof(wubu_vkd3d_pso_desc_t));
        c->n--;
        return 0;
    }
    return -1;      /* unknown handle: releasing twice is an app bug */
}

uint32_t wubu_vkd3d_pso_cache_size(const wubu_vkd3d_pso_cache_t *c)
{
    return c ? c->n : 0;
}

uint32_t wubu_vkd3d_pso_cache_hits(const wubu_vkd3d_pso_cache_t *c)
{
    return c ? (uint32_t)c->hits : 0;
}

uint32_t wubu_vkd3d_pso_cache_misses(const wubu_vkd3d_pso_cache_t *c)
{
    return c ? (uint32_t)c->misses : 0;
}

/* -- the Vulkan translation ----------------------------------------- */

static bool map_fill_mode(uint32_t v, VkPolygonMode *out)
{
    switch (v) {
    case 1: *out = VK_POLYGON_MODE_LINE; return true;   /* WIREFRAME */
    case 2: *out = VK_POLYGON_MODE_FILL; return true;   /* SOLID */
    default: return false;
    }
}

static bool map_cull_mode(uint32_t v, VkCullModeFlags *out)
{
    switch (v) {
    case 1: *out = VK_CULL_MODE_FRONT_BIT; return true;
    case 2: *out = VK_CULL_MODE_BACK_BIT;  return true;
    case 3: *out = VK_CULL_MODE_NONE;      return true;
    default: return false;
    }
}

static bool map_compare(uint32_t v, VkCompareOp *out)
{
    /* D3D12_COMPARISON_FUNC is 1-based and VkCompareOp is 0-based: the same
     * comparison is one number apart.  Passing the value straight through
     * would turn LESS_EQUAL into GREATER -- a silent visual bug. */
    switch (v) {
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
        *out = (VkCompareOp)(v - 1);
        return true;
    default: return false;
    }
}

static bool map_blend_factor(uint32_t v, VkBlendFactor *out)
{
    /* D3D12_BLEND and VkBlendFactor agree on ZERO/ONE (1,2 vs 0,1) but the
     * rest are shifted by one, and D3D12's BLEND_FACTOR (12) is invalid --
     * it means "use the blend factor constant", which needs a separate API. */
    switch (v) {
    case 1:  *out = VK_BLEND_FACTOR_ZERO;                return true;
    case 2:  *out = VK_BLEND_FACTOR_ONE;                 return true;
    case 3:  *out = VK_BLEND_FACTOR_SRC_COLOR;           return true;
    case 4:  *out = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR; return true;
    case 5:  *out = VK_BLEND_FACTOR_SRC_ALPHA;           return true;
    case 6:  *out = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; return true;
    case 7:  *out = VK_BLEND_FACTOR_DST_ALPHA;           return true;
    case 8:  *out = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA; return true;
    case 9:  *out = VK_BLEND_FACTOR_DST_COLOR;           return true;
    case 10: *out = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR; return true;
    case 11: *out = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;  return true;
    case 13: *out = VK_BLEND_FACTOR_SRC1_COLOR;          return true;
    case 14: *out = VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;return true;
    case 15: *out = VK_BLEND_FACTOR_SRC1_ALPHA;          return true;
    case 16: *out = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;return true;
    default: return false;
    }
}

static bool map_blend_op(uint32_t v, VkBlendOp *out)
{
    /* NOT a simple shift: MIN and MAX swap positions between the two APIs
     * (D3D12 4=MIN,5=MAX ; Vulkan 3=MIN,4=MAX).  A table, not arithmetic. */
    switch (v) {
    case 1: *out = VK_BLEND_OP_ADD;              return true;
    case 2: *out = VK_BLEND_OP_SUBTRACT;         return true;
    case 3: *out = VK_BLEND_OP_REVERSE_SUBTRACT; return true;
    case 4: *out = VK_BLEND_OP_MIN;              return true;
    case 5: *out = VK_BLEND_OP_MAX;              return true;
    default: return false;
    }
}

static bool map_topology(uint32_t v, VkPrimitiveTopology *out, bool *valid)
{
    /* D3D12's topology TYPE is a family; Vulkan's is concrete.  A TRIANGLE
     * type maps to the LIST form -- the strip/fan variants are selected by the
     * concrete D3D12 topology, which this module does not consume. */
    switch (v) {
    case 2: *out = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;     *valid = true; return true;
    case 3: *out = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;      *valid = true; return true;
    case 4: *out = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  *valid = true; return true;
    case 5: *out = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;           *valid = false; return true;  /* PATCH */
    case 1: *valid = false; return true;   /* UNDEFINED */
    default: return false;
    }
}

bool wubu_vkd3d_pso_vk(const wubu_vkd3d_pso_desc_t *d, wubu_vkd3d_pso_vk_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!d) return false;

    if (!map_fill_mode(d->fill_mode, &out->polygon_mode)) return false;
    if (!map_cull_mode(d->cull_mode, &out->cull_mode)) return false;
    out->front_face = d->front_counter_clockwise
                          ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                          : VK_FRONT_FACE_CLOCKWISE;
    if (!map_compare(d->depth_func, &out->depth_func)) return false;
    if (!map_blend_factor(d->blend_src_color, &out->src_color)) return false;
    if (!map_blend_factor(d->blend_dst_color, &out->dst_color)) return false;
    if (!map_blend_factor(d->blend_src_alpha, &out->src_alpha)) return false;
    if (!map_blend_factor(d->blend_dst_alpha, &out->dst_alpha)) return false;
    if (!map_blend_op(d->blend_op_color, &out->op_color)) return false;
    if (!map_blend_op(d->blend_op_alpha, &out->op_alpha)) return false;
    if (!map_topology(d->primitive_topology, &out->topology,
                      &out->topology_valid)) return false;

    out->valid = true;
    return true;
}
