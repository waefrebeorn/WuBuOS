/*
 * wubu_vkd3d_precache_test.c -- the shader pre-cache (fossilize).
 *
 * The load-bearing rules: a cache is bound to a DEVICE and a VERSION (so a
 * cache from another GPU is discarded, not reused), a corrupt file is discarded
 * WHOLE, and a save is ATOMIC.
 */
#include "wubu_vkd3d_precache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static const char *PATH = "/tmp/wubu_vkd3d_precache_test.bin";

static void rm_cache(void) { remove(PATH); remove("/tmp/wubu_vkd3d_precache_test.bin.tmp"); }

int main(void)
{
    printf("=== test_vkd3d_precache ===\n");
    wubu_vkd3d_precache_fp_t fp = { 0x10DE, 0x2684, 12345 };
    wubu_vkd3d_precache_fp_t other = { 0x8086, 0x9BC5, 999 };

    rm_cache();

    /* -- a fresh cache: no file means an empty one, NOT an error -- */
    CHECK(wubu_vkd3d_precache_open(PATH, &fp) == NULL,
          "a missing file yields no cache (a cold start, not a failure)");

    /* -- a cold start: an absent file is not an error, and create() gives the
     * caller somewhere to build the cache -- */
    wubu_vkd3d_precache_t *c = wubu_vkd3d_precache_create(&fp);
    CHECK(c != NULL, "a fresh cache is created");
    CHECK(wubu_vkd3d_precache_valid(c), "it is valid");
    CHECK(wubu_vkd3d_precache_size(c) == 0, "it starts empty");
    CHECK(wubu_vkd3d_precache_save(c, PATH) == 0, "the empty cache saves");
    wubu_vkd3d_precache_destroy(c);

    c = wubu_vkd3d_precache_open(PATH, &fp);
    CHECK(c != NULL, "the saved cache re-opens");
    if (!c) { printf("  (cannot continue)\n"); return 1; }
    CHECK(wubu_vkd3d_precache_valid(c), "it is valid");
    CHECK(wubu_vkd3d_precache_size(c) == 0, "it is empty");

    /* -- add entries -- */
    {
        uint8_t blob_a[64], blob_b[128];
        memset(blob_a, 0xAA, sizeof(blob_a));
        memset(blob_b, 0xBB, sizeof(blob_b));
        CHECK(wubu_vkd3d_precache_add(c, 0x1111, blob_a, sizeof(blob_a)) == 0,
              "entry A added");
        CHECK(wubu_vkd3d_precache_add(c, 0x2222, blob_b, sizeof(blob_b)) == 0,
              "entry B added");
        CHECK(wubu_vkd3d_precache_size(c) == 2, "two entries");

        /* a miss is not an error */
        CHECK(wubu_vkd3d_precache_lookup(c, 0x9999) == NULL,
              "a missing key is a miss, not an error");

        /* save and reopen */
        CHECK(wubu_vkd3d_precache_save(c, PATH) == 0, "the cache saves");
    }
    wubu_vkd3d_precache_destroy(c);

    c = wubu_vkd3d_precache_open(PATH, &fp);
    CHECK(c != NULL, "the populated cache re-opens");
    CHECK(wubu_vkd3d_precache_size(c) == 2, "both entries survive the round trip");

    {
        const wubu_vkd3d_precache_entry_t *e =
            wubu_vkd3d_precache_lookup(c, 0x1111);
        CHECK(e != NULL, "entry A is found");
        uint8_t out[256];
        memset(out, 0, sizeof(out));
        CHECK(wubu_vkd3d_precache_read(c, e, out, sizeof(out)) == 64,
              "entry A's blob reads back at its size");
        bool same = true;
        for (size_t i = 0; i < 64; i++) if (out[i] != 0xAA) same = false;
        CHECK(same, "entry A's blob is byte-identical");

        e = wubu_vkd3d_precache_lookup(c, 0x2222);
        CHECK(e != NULL, "entry B is found");
        memset(out, 0, sizeof(out));
        CHECK(wubu_vkd3d_precache_read(c, e, out, sizeof(out)) == 128,
              "entry B's blob reads back at its size");
        same = true;
        for (size_t i = 0; i < 128; i++) if (out[i] != 0xBB) same = false;
        CHECK(same, "entry B's blob is byte-identical");
    }
    wubu_vkd3d_precache_destroy(c);

    /* -- a cache from ANOTHER DEVICE is discarded, not reused -- */
    c = wubu_vkd3d_precache_open(PATH, &other);
    CHECK(c == NULL,
          "a cache built for another GPU/version is DISCARDED (pipelines are device-specific)");

    /* -- a corrupt file is discarded WHOLE -- */
    {
        FILE *f = fopen(PATH, "rb");
        CHECK(f != NULL, "the cache file exists");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            uint8_t *buf = (uint8_t *)malloc((size_t)sz);
            if (buf && fread(buf, 1, (size_t)sz, f) == (size_t)sz) {
                /* truncate to half: a half-written cache must never be used */
                fclose(f);
                f = fopen(PATH, "wb");
                if (f) { fwrite(buf, 1, (size_t)sz / 2, f); fclose(f); }
                c = wubu_vkd3d_precache_open(PATH, &fp);
                CHECK(c == NULL, "a TRUNCATED cache is discarded whole");
            } else {
                fclose(f);
            }
            free(buf);
        }
    }

    /* -- a bad magic is discarded -- */
    {
        FILE *f = fopen(PATH, "wb");
        if (f) { fwrite("NOTVKD3D", 1, 8, f); fclose(f); }
        c = wubu_vkd3d_precache_open(PATH, &fp);
        CHECK(c == NULL, "a bad magic is discarded");
    }

    /* -- the save is atomic: no .tmp is left behind -- */
    /* -- re-adding the same key REFRESHES it rather than duplicating it -- */
    rm_cache();
    c = wubu_vkd3d_precache_create(&fp);
    CHECK(c != NULL, "fresh cache");
    uint8_t v1[16], v2[32];
    memset(v1, 0x11, sizeof(v1));
    memset(v2, 0x22, sizeof(v2));
    CHECK(wubu_vkd3d_precache_add(c, 0xABCD, v1, sizeof(v1)) == 0, "first add");
    CHECK(wubu_vkd3d_precache_size(c) == 1, "one entry");
    CHECK(wubu_vkd3d_precache_add(c, 0xABCD, v2, sizeof(v2)) == 0, "re-add");
    CHECK(wubu_vkd3d_precache_size(c) == 1, "still ONE entry (refreshed, not duplicated)");
    {
        const wubu_vkd3d_precache_entry_t *e = wubu_vkd3d_precache_lookup(c, 0xABCD);
        uint8_t out[64];
        CHECK(e != NULL && wubu_vkd3d_precache_read(c, e, out, sizeof(out)) == 32,
              "the refreshed blob is the NEW one");
    }
    wubu_vkd3d_precache_destroy(c);

    /* -- the file has no leftover .tmp after a save -- */
    {
        FILE *probe = fopen("/tmp/wubu_vkd3d_precache_test.bin.tmp", "rb");
        CHECK(probe == NULL, "no .tmp file is left behind (the save is atomic)");
        if (probe) fclose(probe);
    }

    /* -- validation of bad arguments -- */
    CHECK(wubu_vkd3d_precache_open(NULL, &fp) == NULL, "a null path is rejected");
    CHECK(wubu_vkd3d_precache_open(PATH, NULL) == NULL, "a null fingerprint is rejected");
    CHECK(wubu_vkd3d_precache_lookup(NULL, 1) == NULL, "a null cache looks up nothing");
    CHECK(wubu_vkd3d_precache_read(NULL, NULL, NULL, 0) == -1, "a null cache reads nothing");
    CHECK(wubu_vkd3d_precache_size(NULL) == 0, "a null cache has no entries");
    CHECK(!wubu_vkd3d_precache_valid(NULL), "a null cache is not valid");
    CHECK(wubu_vkd3d_precache_save(NULL, PATH) == -1, "a null cache cannot save");
    wubu_vkd3d_precache_destroy(NULL);

    rm_cache();

    if (failures == 0) printf("ALL VKD3D_PRECACHE TESTS PASSED\n");
    else printf("%d VKD3D_PRECACHE FAILURES\n", failures);
    return failures ? 1 : 0;
}
