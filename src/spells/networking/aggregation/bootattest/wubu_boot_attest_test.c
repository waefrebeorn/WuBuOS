/*
 * wubu_boot_attest_test.c -- a boot report must be sealed, verifiable, and
 * fail closed.  Every assertion is about the behaviour a peer relies on:
 * the report survives a round trip, a altered byte is caught, and a refused
 * frame leaks nothing about why.
 */
#include "wubu_boot_attest.h"

#include <stdio.h>
#include <string.h>

static int failures, checks;
#define CHECK(c, m) do { checks++; if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static void fill_pcrs(uint8_t pcrs[WUBU_BOOT_ATTEST_PCR_COUNT][32], uint8_t seed)
{
    for (int i = 0; i < WUBU_BOOT_ATTEST_PCR_COUNT; i++)
        for (int j = 0; j < 32; j++)
            pcrs[i][j] = (uint8_t)(seed + i * 7 + j);
}

int main(void)
{
    printf("=== test_wubu_boot_attest ===\n");

    uint8_t pcrs[WUBU_BOOT_ATTEST_PCR_COUNT][32];
    fill_pcrs(pcrs, 11);
    uint8_t pcr_digest[32];
    CHECK(wubu_boot_attest_pcr_digest(pcrs, pcr_digest) == 0,
          "the PCR bank digests");
    uint8_t pcrs2[WUBU_BOOT_ATTEST_PCR_COUNT][32];
    fill_pcrs(pcrs2, 11);
    uint8_t pcr_digest2[32];
    wubu_boot_attest_pcr_digest(pcrs2, pcr_digest2);
    CHECK(memcmp(pcr_digest, pcr_digest2, 32) == 0,
          "the PCR digest is deterministic (peers can re-derive it)");

    uint8_t kernel_digest[32];
    memset(kernel_digest, 0x5A, sizeof(kernel_digest));

    /* -- a full boot report -- */
    wubu_boot_attest_t *b = wubu_boot_attest_create();
    CHECK(b != NULL, "the report exists");
    CHECK(wubu_boot_attest_bind(b, 7, pcr_digest, kernel_digest) == 0,
          "the report binds to the measured boot");
    CHECK(wubu_boot_attest_stage(b, "firmware", 0) == 0, "firmware stage");
    CHECK(wubu_boot_attest_stage(b, "loader", 40) == 0, "loader stage");
    CHECK(wubu_boot_attest_stage(b, "kernel", 120) == 0, "kernel stage");
    CHECK(wubu_boot_attest_stage(b, "gui", 300) == 0, "gui stage");
    CHECK(wubu_boot_attest_seal(b, 1763000000u) == 0, "the report seals");
    CHECK(wubu_boot_attest_stage(b, "late", 999) == 0,
          "a stage may be recorded after sealing (the report is mutable)");

    uint8_t frame[WUBU_BOOT_ATTEST_FRAME_SZ];
    long len = wubu_boot_attest_encode(b, frame, sizeof(frame));
    CHECK(len > 0, "the frame encodes");
    CHECK((size_t)len <= sizeof(frame), "the frame fits the documented size");
    printf("  frame: %ld bytes\n", len);

    /* -- a peer decodes it -- */
    wubu_boot_attest_view_t v;
    memset(&v, 0, sizeof(v));
    CHECK(wubu_boot_attest_decode(frame, (size_t)len, &v) == 0,
          "a peer decodes the report");
    CHECK(v.boot_counter == 7, "the boot counter survives");
    CHECK(v.sealed_at == 1763000000u, "the seal time survives");
    CHECK(v.stage_count == 5, "the stage count survives");
    CHECK((v.flags & WUBU_BOOT_ATTEST_F_BOUND) != 0, "the bound flag survives");
    CHECK(memcmp(v.pcr_digest, pcr_digest, 32) == 0,
          "the PCR digest survives (the peer can compare it)");
    CHECK(memcmp(v.kernel_digest, kernel_digest, 32) == 0,
          "the kernel digest survives");
    CHECK(strcmp(v.stages[0].name, "firmware") == 0, "stage 0 is firmware");
    CHECK(v.stages[1].started_ms == 40, "stage 1 starts at 40ms");
    CHECK(v.stages[0].duration_ms == 40,
          "stage 0 ran 40ms (to the next stage)");
    CHECK(v.stages[2].duration_ms == 180,
          "stage 2 ran 180ms (120 -> 300)");
    CHECK(v.stages[3].duration_ms == 0,
          "the stage that was last at seal time runs to the seal, not past it");
    CHECK(v.stages[4].duration_ms == 0,
          "the stage recorded after sealing gets a zero duration, not a wrap");

    /* -- the signature -- */
    uint8_t sig[32];
    CHECK(wubu_boot_attest_verify(frame, (size_t)len, sig) == 0,
          "the signature verifies independently of decode()");

    /* -- fail closed: every refusal is the same -1 -- */
    uint8_t bad[sizeof(frame)];
    memcpy(bad, frame, (size_t)len);

    bad[30] ^= 0x01;                                  /* a PCR byte */
    CHECK(wubu_boot_attest_decode(bad, (size_t)len, &v) == -1,
          "an altered PCR digest is refused");

    memcpy(bad, frame, (size_t)len);
    bad[len - 1] ^= 0x80;                             /* the signature */
    CHECK(wubu_boot_attest_verify(bad, (size_t)len, sig) == -1,
          "an altered signature is refused");

    memcpy(bad, frame, (size_t)len);
    bad[0] = 'X';                                     /* the magic */
    CHECK(wubu_boot_attest_decode(bad, (size_t)len, &v) == -1,
          "a bad magic is refused");

    memcpy(bad, frame, (size_t)len);
    bad[4] = 99;                                      /* the version */
    CHECK(wubu_boot_attest_decode(bad, (size_t)len, &v) == -1,
          "an unknown version is refused");

    memcpy(bad, frame, (size_t)len);
    bad[6] = WUBU_BOOT_ATTEST_MAX_STAGES + 1;         /* too many stages */
    CHECK(wubu_boot_attest_decode(bad, (size_t)len, &v) == -1,
          "an over-full stage table is refused");

    CHECK(wubu_boot_attest_decode(frame, (size_t)len - 1, &v) == -1,
          "a truncated frame is refused");
    CHECK(wubu_boot_attest_decode(frame, WUBU_BOOT_ATTEST_HDR_SZ, &v) == -1,
          "a header-only frame is refused");
    CHECK(wubu_boot_attest_decode(NULL, (size_t)len, &v) == -1,
          "a NULL frame is refused");

    /* -- a refused frame leaves the caller's view untouched -- */
    wubu_boot_attest_view_t untouched;
    memset(&untouched, 0xA5, sizeof(untouched));
    memcpy(bad, frame, (size_t)len);
    bad[30] ^= 0x02;
    CHECK(wubu_boot_attest_decode(bad, (size_t)len, &untouched) == -1,
          "the tampered frame is refused again");
    {
        int same = 1;
        for (size_t i = 0; i < sizeof(untouched); i++)
            if (((uint8_t *)&untouched)[i] != 0xA5) same = 0;
        CHECK(same, "a refused decode leaks nothing into the caller's view");
    }

    /* -- the stage table and the name shape -- */
    wubu_boot_attest_t *b2 = wubu_boot_attest_create();
    wubu_boot_attest_bind(b2, 1, pcr_digest, kernel_digest);
    CHECK(wubu_boot_attest_stage(b2, "", 0) == -1, "an empty stage name is refused");
    CHECK(wubu_boot_attest_stage(b2, "this-name-is-far-too-long", 0) == -1,
          "an over-long stage name is refused");
    for (int i = 0; i < WUBU_BOOT_ATTEST_MAX_STAGES; i++) {
        char nm[16];
        snprintf(nm, sizeof(nm), "s%d", i);
        CHECK(wubu_boot_attest_stage(b2, nm, (uint32_t)(i * 10)) == 0,
              "the stage table fills exactly");
    }
    CHECK(wubu_boot_attest_stage(b2, "one-more", 0) == -1,
          "the stage table refuses to overflow");

    /* -- a report with nothing to seal -- */
    wubu_boot_attest_t *b3 = wubu_boot_attest_create();
    CHECK(wubu_boot_attest_seal(b3, 1) == -1,
          "an unbound report does not seal");
    wubu_boot_attest_bind(b3, 1, pcr_digest, kernel_digest);
    CHECK(wubu_boot_attest_seal(b3, 1) == -1,
          "a report with no stages does not seal");
    CHECK(wubu_boot_attest_bind(b3, 1, NULL, kernel_digest) == -1,
          "a NULL PCR digest is refused");

    /* -- the encode side -- */
    uint8_t small[16];
    CHECK(wubu_boot_attest_encode(b, small, sizeof(small)) == -1,
          "a buffer too small is refused");
    CHECK(wubu_boot_attest_encode(NULL, frame, sizeof(frame)) == -1,
          "a NULL report is refused");

    wubu_boot_attest_destroy(b);
    wubu_boot_attest_destroy(b2);
    wubu_boot_attest_destroy(b3);

    printf("=== %d checks, %d failed ===\n", checks, failures);
    return failures ? 1 : 0;
}
