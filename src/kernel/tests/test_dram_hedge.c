/* src/kernel/tests/test_dram_hedge.c
 *
 * Selftest for the WuBuOS DRAM-refresh hedge (wubu_dram_hedge.c):
 * proves the channel-stride addressing lands replicas 256 bytes apart and
 * that replicated put/get round-trips correctly for several element sizes.
 */
#include <stdio.h>
#include <string.h>
#include "wubu_dram_hedge.h"

static int checks = 0;
static int fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

static void test_channel_stride(void) {
    printf("== channel stride ==\n");
    wdh_hedge_t *h = wdh_create(8, 2);
    CHECK(h != NULL, "create 2 replicas");
    if (!h) return;
    CHECK(wdh_replicas(h) == 2, "2 replicas");
    char *r0 = (char *)wdh_replica_base(h, 0);
    char *r1 = (char *)wdh_replica_base(h, 1);
    CHECK(r0 && r1, "replica bases non-null");
    if (r0 && r1)
        CHECK((size_t)(r1 - r0) == 256, "replicas 256B apart (channel scramble)");
    CHECK(wdh_slots(h) == 65536, "slot count");
    CHECK(wdh_elem_size(h) == 8, "elem size 8");
    wdh_destroy(h);
}

static void test_roundtrip_8(void) {
    printf("== put/get roundtrip, elem=8 ==\n");
    wdh_hedge_t *h = wdh_create(8, 2);
    if (!h) { printf("  create failed\n"); return; }
    unsigned char in[8] = {1,2,3,4,5,6,7,8};
    unsigned char out[8] = {0};
    /* slots spread across chunks to exercise the stride math */
    for (size_t idx = 0; idx < 400; idx += 7) {
        in[0] = (unsigned char)(idx & 0xFF);
        in[7] = (unsigned char)((idx >> 8) & 0xFF);
        CHECK(wdh_put(h, idx, in) == 0, "put");
        memset(out, 0, sizeof out);
        CHECK(wdh_get(h, idx, out) == 0, "get");
        CHECK(memcmp(in, out, 8) == 0, "roundtrip match");
    }
    /* a boundary slot near the chunk edge */
    size_t per_chunk = 256 / 8; /* 32 */
    for (size_t idx = 0; idx < 3; idx++) {
        size_t i = per_chunk * idx;  /* 0, 32, 64 = chunk starts */
        in[0] = 0xAA; in[1] = 0xBB;
        wdh_put(h, i, in);
        memset(out, 0, sizeof out);
        wdh_get(h, i, out);
        CHECK(out[0] == 0xAA && out[1] == 0xBB, "chunk-boundary roundtrip");
    }
    wdh_destroy(h);
}

static void test_roundtrip_4(void) {
    printf("== put/get roundtrip, elem=4 ==\n");
    wdh_hedge_t *h = wdh_create(4, 3);
    if (!h) { printf("  create failed\n"); return; }
    CHECK(wdh_replicas(h) == 3, "3 replicas");
    unsigned char in[4] = {9,8,7,6};
    unsigned char out[4] = {0};
    for (size_t idx = 0; idx < 300; idx += 3) {
        in[0] = (unsigned char)idx;
        wdh_put(h, idx, in);
        memset(out, 0, sizeof out);
        wdh_get(h, idx, out);
        CHECK(memcmp(in, out, 4) == 0, "roundtrip match elem4");
    }
    wdh_destroy(h);
}

static void test_replicated_writes(void) {
    printf("== replicas hold identical data (the hedge precondition) ==\n");
    wdh_hedge_t *h = wdh_create(8, 2);
    if (!h) return;
    unsigned char in[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
    CHECK(wdh_put(h, 5, in) == 0, "put slot 5");
    /* both replicas' 256B-apart addresses must hold the bytes */
    char *r0 = (char *)wdh_replica_base(h, 0);
    char *r1 = (char *)wdh_replica_base(h, 1);
    CHECK(memcmp(r0 + 5*8, in, 8) == 0, "replica0 written");
    CHECK(memcmp(r1 + 5*8, in, 8) == 0, "replica1 written (256B away)");
    wdh_destroy(h);
}

static void test_probe(void) {
    printf("== trefi probe ==\n");
    double median = 0, spike = 0;
    int periodic = wdh_probe_trefi(&median, &spike);
    printf("  periodic=%d median=%.0f cyc spike_pct=%.3f\n",
           periodic, median, spike);
    /* probe must not crash; median must be a sane load latency (50-5000 cyc) */
    CHECK(median > 0 && median < 5000, "median in sane range");
}

static void test_reader_pool(void) {
    printf("== hedged reader worker-pool (race-to-completion) ==\n");
    wdh_hedge_t *h = wdh_create(8, 2);
    if (!h) { printf("  hedge create failed\n"); return; }
    /* pin the two workers to distinct cores (0 and 2) so they don't share
     * a physical core's execution ports */
    int cores[2] = {0, 2};
    wdh_reader_t *r = wdh_reader_create(h, cores, 2);
    if (!r) {
        printf("  reader pool unavailable on this build (no pthread) — skip\n");
        wdh_destroy(h);
        return;
    }
    unsigned char in[8] = {0xDE,0xAD,0xBE,0xEF,0x11,0x22,0x33,0x44};
    unsigned char out[8] = {0};
    int ok = 1;
    for (size_t idx = 0; idx < 200; idx += 5) {
        in[0] = (unsigned char)(idx & 0xFF);
        in[1] = (unsigned char)((idx >> 8) & 0xFF);
        wdh_put(h, idx, in);
        memset(out, 0, sizeof out);
        if (wdh_reader_read(r, idx, out) != 0) { ok = 0; break; }
        if (memcmp(in, out, 8) != 0) {
            printf("  mismatch idx=%zu want=%02x%02x.. got=%02x%02x..\n",
                   idx, in[0], in[1], out[0], out[1]);
            ok = 0; break;
        }
    }
    CHECK(ok, "reader pool round-trips match across race");
    wdh_reader_destroy(r);
    wdh_destroy(h);
}

int main(void) {
    printf("=== WuBuOS DRAM-refresh hedge selftest ===\n");
    test_channel_stride();
    test_roundtrip_8();
    test_roundtrip_4();
    test_replicated_writes();
    test_probe();
    test_reader_pool();
    printf("\nResults: %d/%d passed, %d failed\n", checks - fails, checks, fails);
    return fails ? 1 : 0;
}
