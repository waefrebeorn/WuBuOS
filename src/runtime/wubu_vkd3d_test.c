/*
 * wubu_vkd3d_test.c -- the state-translation core, proven without a GPU.
 *
 * The whole point of this module is that a D3D12 state either maps to the right
 * Vulkan (stage, access, layout) triple or it is REJECTED -- a layer that
 * guesses corrupts rendering.  So the test asserts both halves.
 */
#include "wubu_vkd3d.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_vkd3d ===\n");
    wubu_vkd3d_state_t s;

    /* -- every single-bit state maps, and nothing is silently dropped -- */
    static const uint32_t states[] = {
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        D3D12_RESOURCE_STATE_INDEX_BUFFER,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_DEPTH_READ,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_STREAM_OUT,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_RESOLVE_DEST,
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
    };
    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        CHECK(wubu_vkd3d_state_map(states[i], &s), "state maps");
        CHECK(s.stage != 0, "state carries a stage mask");
        CHECK(s.name != NULL, "state has a name");
    }

    /* -- COMMON is legal on its own and maps to GENERAL -- */
    CHECK(wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_COMMON, &s), "COMMON maps");
    CHECK(s.image_layout_valid && s.image_layout == VK_IMAGE_LAYOUT_GENERAL,
          "COMMON -> VK_IMAGE_LAYOUT_GENERAL");

    /* -- a composite maps to the OR of its parts -- */
    CHECK(wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_GENERIC_READ, &s),
          "GENERIC_READ maps");
    wubu_vkd3d_state_t vb, ib;
    wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &vb);
    wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_INDEX_BUFFER, &ib);
    CHECK((s.stage & vb.stage) == vb.stage && (s.stage & ib.stage) == ib.stage,
          "GENERIC_READ's stage covers VB and IB");
    CHECK((s.access & vb.access) == vb.access && (s.access & ib.access) == ib.access,
          "GENERIC_READ's access covers VB and IB");

    /* -- the layout is the specific one, not a generic fallback -- */
    CHECK(wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_RENDER_TARGET, &s),
          "RENDER_TARGET maps");
    CHECK(s.image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          "RENDER_TARGET -> COLOR_ATTACHMENT_OPTIMAL");
    CHECK(wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_COPY_SOURCE, &s),
          "COPY_SOURCE maps");
    CHECK(s.image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          "COPY_SOURCE -> TRANSFER_SRC_OPTIMAL");

    /* -- depth read vs depth write use different layouts -- */
    wubu_vkd3d_state_t dw, dr;
    wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_DEPTH_WRITE, &dw);
    wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_DEPTH_READ, &dr);
    CHECK(dw.image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          "DEPTH_WRITE -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL");
    CHECK(dr.image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
          "DEPTH_READ -> DEPTH_STENCIL_READ_ONLY_OPTIMAL");
    CHECK((dw.access & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) != 0,
          "DEPTH_WRITE can write");
    CHECK((dr.access & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) == 0,
          "DEPTH_READ cannot write");

    /* -- unknown bits are REJECTED, never guessed -- */
    CHECK(!wubu_vkd3d_state_map(0x80000000u, &s), "an unknown high bit is rejected");
    CHECK(!wubu_vkd3d_state_map(D3D12_RESOURCE_STATE_COPY_DEST | 0x08000000u, &s),
          "a known state OR an unknown bit is still rejected");
    CHECK(wubu_vkd3d_state_map(0, &s), "0 (COMMON) is never rejected");
    CHECK(wubu_vkd3d_known_states() & D3D12_RESOURCE_STATE_COPY_SOURCE,
          "the known mask includes COPY_SOURCE");

    /* -- names -- */
    CHECK(strcmp(wubu_vkd3d_state_name(D3D12_RESOURCE_STATE_COPY_DEST),
                 "COPY_DEST") == 0, "state name");
    CHECK(strcmp(wubu_vkd3d_state_name(D3D12_RESOURCE_STATE_GENERIC_READ),
                 "GENERIC_READ") == 0, "composite name");
    CHECK(wubu_vkd3d_state_name(0x80000000u) == NULL, "unknown state has no name");

    /* -- barriers -- */
    {
        wubu_vkd3d_barrier_t bars[2] = {
            { 1, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE, false,
              VKD3D_BARRIER_TRANSITION },
            { 2, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE, false,
              VKD3D_BARRIER_TRANSITION },
        };
        VkImageMemoryBarrier imb[4]; uint32_t n_imb = 0;
        VkBufferMemoryBarrier bmb[4]; uint32_t n_bmb = 0;
        VkMemoryBarrier mb[4]; uint32_t n_mb = 0;
        int w = wubu_vkd3d_barriers(bars, 2, mb, &n_mb, bmb, &n_bmb, imb, &n_imb);
        CHECK(w == 2, "both barriers were emitted");
        CHECK(n_imb == 2 && n_bmb == 0, "image barriers went to the image array");
        CHECK(imb[0].oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
              imb[0].newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
              "the first barrier carries the copy-dest -> copy-src layout change");
        CHECK(imb[0].srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT,
              "the first barrier's src access is TRANSFER_WRITE");
        CHECK(imb[0].image == VK_NULL_HANDLE,
              "the caller binds the resource, not this module");

        /* a buffer barrier lands in the buffer array */
        wubu_vkd3d_barrier_t bb[1] = {
            { 3, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                 D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, true,
                 VKD3D_BARRIER_TRANSITION }
        };
        n_imb = 0; n_bmb = 0; n_mb = 0;
        w = wubu_vkd3d_barriers(bb, 1, mb, &n_mb, bmb, &n_bmb, imb, &n_imb);
        CHECK(w == 1 && n_bmb == 1 && n_imb == 0, "a buffer barrier goes to the buffer array");
        CHECK(bmb[0].dstAccessMask == VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
              "the buffer barrier carries the indirect-read access");

        /* an unmappable barrier fails the whole batch, not just the bad one */
        wubu_vkd3d_barrier_t bad[2] = {
            { 4, D3D12_RESOURCE_STATE_COPY_SOURCE,
                 D3D12_RESOURCE_STATE_COPY_DEST, false, VKD3D_BARRIER_TRANSITION },
            { 5, 0x80000000u, D3D12_RESOURCE_STATE_COPY_DEST, false, VKD3D_BARRIER_TRANSITION },
        };
        n_imb = 0; n_bmb = 0; n_mb = 0;
        CHECK(wubu_vkd3d_barriers(bad, 2, mb, &n_mb, bmb, &n_bmb, imb, &n_imb) == -1,
              "an unmappable barrier is an error");

        /* -- a UAV barrier is a GLOBAL memory barrier, and never a layout
         * change: emitting a layout change here would cost a resolve on some
         * drivers for no benefit. */
        {
            wubu_vkd3d_barrier_t uav[1] = {
                { 9, 0, 0, false, VKD3D_BARRIER_UAV }
            };
            n_imb = 0; n_bmb = 0; n_mb = 0;
            CHECK(wubu_vkd3d_barriers(uav, 1, mb, &n_mb, bmb, &n_bmb, imb, &n_imb) == 1,
                  "a UAV barrier is emitted");
            CHECK(n_mb == 1 && n_imb == 0 && n_bmb == 0,
                  "a UAV barrier is a global memory barrier, not a resource one");
            CHECK(mb[0].srcAccessMask == (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
                  "a UAV barrier orders both UAV reads and writes");
        }

        /* -- an aliasing barrier keeps the layout -- */
        {
            wubu_vkd3d_barrier_t ali[1] = {
                { 10, D3D12_RESOURCE_STATE_COPY_SOURCE,
                       D3D12_RESOURCE_STATE_COPY_DEST, false, VKD3D_BARRIER_ALIASING }
            };
            n_imb = 0; n_bmb = 0; n_mb = 0;
            CHECK(wubu_vkd3d_barriers(ali, 1, mb, &n_mb, bmb, &n_bmb, imb, &n_imb) == 1,
                  "an aliasing barrier is emitted");
            CHECK(imb[0].oldLayout == imb[0].newLayout,
                  "an aliasing barrier does NOT change the layout");
            CHECK(imb[0].oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  "the layout kept is the resource's current one");
        }
    }

    /* -- the tracker -- */
    {
        wubu_vkd3d_tracker_t *t = wubu_vkd3d_tracker_create();
        CHECK(t != NULL, "tracker created");
        CHECK(wubu_vkd3d_tracker_count(t) == 0, "tracker starts empty");
        CHECK(wubu_vkd3d_tracker_state(t, 7) == D3D12_RESOURCE_STATE_COMMON,
              "an unseen resource is COMMON");

        /* a first transition FROM common is legal */
        CHECK(wubu_vkd3d_tracker_transition(t, 7, D3D12_RESOURCE_STATE_COMMON,
                                            D3D12_RESOURCE_STATE_COPY_DEST) == 0,
              "common -> copy_dest is legal");
        CHECK(wubu_vkd3d_tracker_state(t, 7) == D3D12_RESOURCE_STATE_COPY_DEST,
              "the tracker records the new state");

        /* lying about the current state is an ILLEGAL transition */
        CHECK(wubu_vkd3d_tracker_transition(t, 7, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_COPY_SOURCE) == -1,
              "a wrong 'from' is rejected");
        CHECK(wubu_vkd3d_tracker_state(t, 7) == D3D12_RESOURCE_STATE_COPY_DEST,
              "a rejected transition does not move the state");

        /* the honest transition succeeds */
        CHECK(wubu_vkd3d_tracker_transition(t, 7, D3D12_RESOURCE_STATE_COPY_DEST,
                                            D3D12_RESOURCE_STATE_COPY_SOURCE) == 0,
              "the honest transition succeeds");

        /* a no-op is reported as such, not as an error */
        CHECK(wubu_vkd3d_tracker_transition(t, 7, D3D12_RESOURCE_STATE_COPY_SOURCE,
                                            D3D12_RESOURCE_STATE_COPY_SOURCE) == 1,
              "from == to is a no-op");
        CHECK(wubu_vkd3d_tracker_count(t) == 1, "the tracker holds one resource");

        wubu_vkd3d_tracker_destroy(t);
    }

    /* -- feature level ladder -- */
    CHECK(wubu_vkd3d_feature_level_le(D3D12_FEATURE_LEVEL_11_0,
                                      D3D12_FEATURE_LEVEL_12_0),
          "11.0 <= 12.0");
    CHECK(!wubu_vkd3d_feature_level_le(D3D12_FEATURE_LEVEL_12_2,
                                       D3D12_FEATURE_LEVEL_12_0),
          "12.2 > 12.0");
    CHECK(strcmp(wubu_vkd3d_feature_level_name(D3D12_FEATURE_LEVEL_12_2), "12.2") == 0,
          "feature level name");
    CHECK(strcmp(wubu_vkd3d_feature_level_name(0x9999), "unknown") == 0,
          "unknown feature level");

    if (failures == 0) printf("ALL VKD3D TESTS PASSED\n");
    else printf("%d VKD3D FAILURES\n", failures);
    return failures ? 1 : 0;
}
