/*
 * wubu_vkd3d_descset_test.c -- root signature -> pipeline layout.
 *
 * The load-bearing rule: a D3D12 register is 32 BITS, so register N is byte
 * N*4.  Packing root constants densely would shift every value the shader
 * reads -- and it would still compile.
 */
#include "wubu_vkd3d_descset.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void one(wubu_vkd3d_root_signature_desc_t *d,
                wubu_vkd3d_root_param_type_t t, uint32_t reg,
                uint32_t vals, VkShaderStageFlags stages)
{
    wubu_vkd3d_root_param_t *p = &d->params[d->param_count++];
    memset(p, 0, sizeof(*p));
    p->type = t;
    p->register_num = reg;
    p->num_32bit_values = vals;
    p->stages = stages;
}

int main(void)
{
    printf("=== test_vkd3d_descset ===\n");
    wubu_vkd3d_root_signature_desc_t d;
    uint32_t off = 0, size = 0;
    VkShaderStageFlags st = 0;
    VkDescriptorSetLayoutBinding b[16];

    /* -- the register -> byte offset rule -- */
    CHECK(wubu_vkd3d_root_constant_offset(0) == 0, "b0 is byte 0");
    CHECK(wubu_vkd3d_root_constant_offset(1) == 4, "b1 is byte 4");
    CHECK(wubu_vkd3d_root_constant_offset(5) == 20, "b5 is byte 20 (NOT packed)");
    CHECK(wubu_vkd3d_root_constant_offset(16) == 64, "b16 is byte 64");

    /* -- a single root constant -- */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CONSTANTS, 0, 1, VK_SHADER_STAGE_VERTEX_BIT);
    CHECK(wubu_vkd3d_root_signature_validate(&d) == NULL, "one root constant is legal");
    CHECK(wubu_vkd3d_root_signature_push_constants(&d, &off, &size, &st) &&
          off == 0 && size == 4, "b0 -> offset 0, size 4");
    CHECK(st == VK_SHADER_STAGE_VERTEX_BIT, "the stage visibility is carried");

    /* -- b0..b3 -> 16 bytes -- */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CONSTANTS, 0, 4, VK_SHADER_STAGE_FRAGMENT_BIT);
    CHECK(wubu_vkd3d_root_signature_push_constants(&d, &off, &size, &st) &&
          off == 0 && size == 16, "b0 with 4 values -> 16 bytes");

    /* -- b5 alone needs a 24-byte range, not 4 -- */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CONSTANTS, 5, 1, VK_SHADER_STAGE_VERTEX_BIT);
    CHECK(wubu_vkd3d_root_signature_push_constants(&d, &off, &size, &st) &&
          off == 20 && size == 4, "b5 -> offset 20, size 4");

    /* -- b0 and b5 together: ONE contiguous 24-byte block -- */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CONSTANTS, 0, 1, VK_SHADER_STAGE_VERTEX_BIT);
    one(&d, VKD3D_ROOT_CONSTANTS, 5, 1, VK_SHADER_STAGE_VERTEX_BIT);
    CHECK(wubu_vkd3d_root_signature_push_constants(&d, &off, &size, &st) &&
          off == 0 && size == 24,
          "b0+b5 -> one contiguous 24-byte block (b1..b4 unused but covered)");

    /* -- stages are the UNION of the parameters -- */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CONSTANTS, 0, 1, VK_SHADER_STAGE_VERTEX_BIT);
    one(&d, VKD3D_ROOT_CONSTANTS, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    CHECK(wubu_vkd3d_root_signature_push_constants(&d, &off, &size, &st) &&
          st == (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
          "the push constant range is visible to both stages");

    /* -- descriptor table types -- */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CBV, 0, 0, VK_SHADER_STAGE_ALL);
    one(&d, VKD3D_ROOT_SRV, 0, 0, VK_SHADER_STAGE_ALL);
    one(&d, VKD3D_ROOT_UAV, 0, 0, VK_SHADER_STAGE_ALL);
    one(&d, VKD3D_ROOT_SAMPLER, 0, 0, VK_SHADER_STAGE_ALL);
    CHECK(wubu_vkd3d_root_signature_validate(&d) == NULL, "one of each table is legal");
    {
        int n = wubu_vkd3d_root_signature_bindings(&d, b, 16);
        CHECK(n == 4, "four bindings");
        CHECK(b[0].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              "CBV -> UNIFORM_BUFFER");
        CHECK(b[1].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              "SRV -> SAMPLED_IMAGE");
        CHECK(b[2].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              "UAV -> STORAGE_IMAGE");
        CHECK(b[3].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER,
              "sampler -> SAMPLER");
        CHECK(b[0].binding == 0 && b[3].binding == 3,
              "the binding numbers are sequential");
    }

    /* -- root constants are NOT bindings -- */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CONSTANTS, 0, 4, VK_SHADER_STAGE_ALL);
    CHECK(wubu_vkd3d_root_signature_bindings(&d, b, 16) == 0,
          "root constants produce no descriptor bindings");
    CHECK(wubu_vkd3d_root_signature_push_constants(&d, NULL, NULL, NULL),
          "push constants are still available (and NULL out pointers are safe)");

    /* -- a mixed signature -- */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CONSTANTS, 0, 4, VK_SHADER_STAGE_VERTEX_BIT);
    one(&d, VKD3D_ROOT_CBV, 0, 0, VK_SHADER_STAGE_ALL);
    one(&d, VKD3D_ROOT_SRV, 0, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    CHECK(wubu_vkd3d_root_signature_validate(&d) == NULL, "a mixed signature is legal");
    CHECK(wubu_vkd3d_root_signature_push_constants(&d, &off, &size, &st) &&
          off == 0 && size == 16, "its constants translate");
    CHECK(wubu_vkd3d_root_signature_bindings(&d, b, 16) == 2,
          "its two tables become two bindings");

    /* -- validation -- */
    memset(&d, 0, sizeof(d));
    CHECK(strcmp(wubu_vkd3d_root_signature_validate(&d),
                 "an empty root signature binds nothing") == 0,
          "an empty root signature is reported");
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CONSTANTS, 0, 0, VK_SHADER_STAGE_ALL);
    CHECK(wubu_vkd3d_root_signature_validate(&d) != NULL,
          "a root constant with no values is reported");
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CBV, 0, 0, 0);
    CHECK(strcmp(wubu_vkd3d_root_signature_validate(&d),
                 "a root parameter names no shader stage") == 0,
          "a parameter with no shader stage is reported");
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CBV, 0, 0, VK_SHADER_STAGE_ALL);
    one(&d, VKD3D_ROOT_CBV, 0, 0, VK_SHADER_STAGE_ALL);
    CHECK(strcmp(wubu_vkd3d_root_signature_validate(&d),
                 "two parameters use the same register") == 0,
          "two parameters on the same register is reported");
    /* different TYPES on the same register number is fine (b0 vs t0) */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CBV, 0, 0, VK_SHADER_STAGE_ALL);
    one(&d, VKD3D_ROOT_SRV, 0, 0, VK_SHADER_STAGE_ALL);
    CHECK(wubu_vkd3d_root_signature_validate(&d) == NULL,
          "b0 and t0 are different namespaces and both legal");
    CHECK(wubu_vkd3d_root_signature_validate(NULL) != NULL, "a null desc is rejected");

    /* -- an invalid desc does not translate -- */
    memset(&d, 0, sizeof(d));
    one(&d, VKD3D_ROOT_CBV, 0, 0, 0);
    CHECK(!wubu_vkd3d_root_signature_push_constants(&d, &off, &size, &st),
          "an invalid desc yields no push constant range");
    CHECK(wubu_vkd3d_root_signature_bindings(&d, b, 16) == -1,
          "an invalid desc yields no bindings");

    if (failures == 0) printf("ALL VKD3D_DESCSET TESTS PASSED\n");
    else printf("%d VKD3D_DESCSET FAILURES\n", failures);
    return failures ? 1 : 0;
}
