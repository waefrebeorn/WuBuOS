/*
 * wubu_vkd3d_descset.h -- VKD3D slice 13: root signature -> pipeline layout.
 *
 * D3D12 binds through a ROOT SIGNATURE: a flat list of parameters, each at a
 * 32-bit register slot (b0, t1, u2, s0).  Vulkan has two different mechanisms
 * and the split is the whole slice:
 *
 *   root constants (b#, 32-bit values)  -> VkPushConstantRange
 *   descriptor tables (CBV/SRV/UAV)     -> VkDescriptorSetLayoutBinding
 *   static samplers (s#)                -> VkDescriptorSetLayoutBinding
 *
 * The rules that are easy to get wrong:
 *
 *   * A D3D12 register is 32 BITS, so root constant register N is byte offset
 *     N*4 in the push constant block.  A translation that packed them densely
 *     (0, 4, 8 for b0, b2, b5) would silently shift every value the shader
 *     reads -- and it would still compile.
 *   * The push constant range must be ONE CONTIGUOUS block covering the HIGHEST
 *     register used, not the number of constants declared.  b5 alone needs a
 *     24-byte range, not 4.
 *   * The range size must be a multiple of 4 (Vulkan requires it).
 *   * Two parameters may not occupy the same register -- D3D12 rejects it, and
 *     a layer that let it through would make the second silently win.
 *   * Descriptor table types map by D3D12's t#/u#/b#/s# prefix, NOT by the
 *     app's intent: a CBV is a uniform/storage buffer, an SRV on a texture is a
 *     sampled image, an UAV on a texture is a storage image.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_DESCSET_H
#define WUBU_VKD3D_DESCSET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#define VKD3D_MAX_ROOT_PARAMS 16

typedef enum {
    VKD3D_ROOT_CONSTANTS = 0,
    VKD3D_ROOT_CBV,
    VKD3D_ROOT_SRV,
    VKD3D_ROOT_UAV,
    VKD3D_ROOT_SAMPLER
} wubu_vkd3d_root_param_type_t;

typedef struct {
    wubu_vkd3d_root_param_type_t type;
    uint32_t          register_num;     /* b0, t1, u2, s0 ... */
    uint32_t          num_32bit_values; /* constants only */
    VkShaderStageFlags stages;          /* visibility */
} wubu_vkd3d_root_param_t;

typedef struct {
    uint32_t               param_count;
    wubu_vkd3d_root_param_t params[VKD3D_MAX_ROOT_PARAMS];
} wubu_vkd3d_root_signature_desc_t;

/* NULL when legal, else a static reason. */
const char *wubu_vkd3d_root_signature_validate(const wubu_vkd3d_root_signature_desc_t *d);

/* The push constant range covering every root constant.  *offset is the byte
 * offset of the first register and *size the total contiguous size. */
bool wubu_vkd3d_root_signature_push_constants(const wubu_vkd3d_root_signature_desc_t *d,
                                              uint32_t *offset, uint32_t *size,
                                              VkShaderStageFlags *stages);

/* The descriptor set layout bindings for every table/sampler parameter.
 * Returns the count, or -1 on an invalid desc. */
int wubu_vkd3d_root_signature_bindings(const wubu_vkd3d_root_signature_desc_t *d,
                                       VkDescriptorSetLayoutBinding *out,
                                       uint32_t max);

/* The byte offset of a root constant register inside the push constant block. */
uint32_t wubu_vkd3d_root_constant_offset(uint32_t register_num);

#endif /* WUBU_VKD3D_DESCSET_H */
