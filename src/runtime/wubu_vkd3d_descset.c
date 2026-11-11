/*
 * wubu_vkd3d_descset.c -- VKD3D slice 13: root signature -> pipeline layout.
 *
 * See wubu_vkd3d_descset.h.
 */
#include "wubu_vkd3d_descset.h"

#include <string.h>

uint32_t wubu_vkd3d_root_constant_offset(uint32_t register_num)
{
    /* A D3D12 root constant register is 32 bits, so register N is byte N*4.
     * Packing them densely instead would shift every value the shader reads
     * and still compile. */
    return register_num * 4u;
}

const char *wubu_vkd3d_root_signature_validate(const wubu_vkd3d_root_signature_desc_t *d)
{
    if (!d) return "no root signature";
    if (d->param_count == 0) return "an empty root signature binds nothing";
    if (d->param_count > VKD3D_MAX_ROOT_PARAMS) return "too many root parameters";

    for (uint32_t i = 0; i < d->param_count; i++) {
        const wubu_vkd3d_root_param_t *p = &d->params[i];
        if (p->stages == 0)
            return "a root parameter names no shader stage";

        /* Two parameters may not occupy the same register: D3D12 rejects it,
         * and a layer that let it through would make the second silently win. */
        for (uint32_t j = i + 1; j < d->param_count; j++) {
            if (d->params[j].type == p->type &&
                d->params[j].register_num == p->register_num &&
                d->params[j].type != VKD3D_ROOT_CONSTANTS)
                return "two parameters use the same register";
        }
        if (p->type == VKD3D_ROOT_CONSTANTS) {
            if (p->num_32bit_values == 0)
                return "a root constant parameter declares no values";
            /* The register block must not overflow a 32-bit offset. */
            if (p->register_num > 1023)
                return "a root constant register is out of range";
        }
    }
    return NULL;
}

bool wubu_vkd3d_root_signature_push_constants(const wubu_vkd3d_root_signature_desc_t *d,
                                              uint32_t *offset, uint32_t *size,
                                              VkShaderStageFlags *stages)
{
    if (offset) *offset = 0;
    if (size) *size = 0;
    if (stages) *stages = 0;
    if (wubu_vkd3d_root_signature_validate(d) != NULL) return false;

    uint32_t lo = 0xFFFFFFFFu, hi = 0;
    bool any = false;
    for (uint32_t i = 0; i < d->param_count; i++) {
        const wubu_vkd3d_root_param_t *p = &d->params[i];
        if (p->type != VKD3D_ROOT_CONSTANTS) continue;
        uint32_t o = wubu_vkd3d_root_constant_offset(p->register_num);
        uint32_t e = o + p->num_32bit_values * 4u;
        if (o < lo) lo = o;
        if (e > hi) hi = e;
        if (stages) *stages |= p->stages;
        any = true;
    }
    if (!any) return false;   /* no root constants: no push constant range */

    if (offset) *offset = lo;
    if (size) *size = hi - lo;   /* contiguous, covering the HIGHEST register */
    return true;
}

int wubu_vkd3d_root_signature_bindings(const wubu_vkd3d_root_signature_desc_t *d,
                                       VkDescriptorSetLayoutBinding *out,
                                       uint32_t max)
{
    if (wubu_vkd3d_root_signature_validate(d) != NULL) return -1;

    uint32_t n = 0;
    for (uint32_t i = 0; i < d->param_count; i++) {
        const wubu_vkd3d_root_param_t *p = &d->params[i];
        VkDescriptorType dt;

        switch (p->type) {
        case VKD3D_ROOT_CBV:     dt = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;  break;
        case VKD3D_ROOT_SRV:     dt = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;   break;
        case VKD3D_ROOT_UAV:     dt = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;    break;
        case VKD3D_ROOT_SAMPLER: dt = VK_DESCRIPTOR_TYPE_SAMPLER;          break;
        default:                 continue;   /* root constants are not bindings */
        }
        if (!out || n >= max) return -1;
        out[n].binding = n;                 /* sequential: tables are ordered */
        out[n].descriptorType = dt;
        out[n].descriptorCount = 1;
        out[n].stageFlags = p->stages;
        out[n].pImmutableSamplers = NULL;
        n++;
    }
    return (int)n;
}
