/*
 * app_bundle_test.c -- the .app container contract.
 *
 * A bundle that lies about where its entry point lives must never reach the exec
 * layer, so most checks are negative: build a good one, then break it in every
 * way the parser could be fooled and require refusal each time.
 */

#include "app_bundle.h"
#include "wubu_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The real exec layer pulls in containers, Wine prefixes and the VSL. This test
 * covers the bundle contract and the REFUSAL paths -- it must prove that a bad
 * bundle never REACHES the exec layer, so a no-op stand-in is exactly right.
 * Same weak-glue convention as wm_test_stub.c and apps_test_stubs.c. */
__attribute__((weak)) int64_t wubu_exec(const void *data, size_t size,
                                        const char *filename)
{
    (void)data; (void)size; (void)filename;
    return WUBU_EXEC_OK;
}

static int g_checks = 0, g_fails = 0;
#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

typedef struct {
    uint32_t personality;
    uint32_t needs;
    uint32_t entry_len;
    const char *name;
} bspec_t;

int main(void)
{
    static uint8_t img[512];
    static uint8_t bad[512];

    /* Build by hand rather than trusting a shared builder helper: this test is
     * about the exact bytes at exact offsets, and an off-by-one in a helper
     * would hide precisely the bug we are looking for. */
    bspec_t sp = { 0x11 /* windows-pe */, 2, 8, "demo" };
    memset(img, 0, sizeof(img));
    memcpy(img, WUBU_APP_MAGIC, 7);
    img[7] = '\0';
    img[8] = (uint8_t)WUBU_APP_VERSION;
    img[12] = 1;
    img[16] = (uint8_t)sp.personality;
    memcpy(img + 20, "demo", 4);
    memcpy(img + 84, "1.0", 3);
    img[116] = (uint8_t)sp.needs;
    memcpy(img + 128, "boot-attestation", 16);
    memcpy(img + 192, "measurement-binding", 20);
    size_t eoff = 128 + (size_t)sp.needs * 64;
    put_u32(img + 120, (uint32_t)eoff);          /* full width: 256 does not fit
                                                  * in one byte, and truncating it
                                                  * silently points at offset 0 */
    put_u32(img + 124, sp.entry_len);
    memcpy(img + eoff, "MZDEMO..", sp.entry_len);
    size_t len = eoff + sp.entry_len;

    /* ---- 1. a well-formed bundle ------------------------------------ */
    wubu_app_bundle_t *a = wubu_app_open(img, len);
    TEST(a != NULL, "a well-formed bundle opens");

    wubu_app_info_t info;
    memset(&info, 0xAA, sizeof(info));
    TEST(wubu_app_info(a, &info) == 0, "info() succeeds");
    TEST(strcmp(info.name, "demo") == 0, "name round-trips");
    TEST(strcmp(info.version, "1.0") == 0, "version round-trips");
    TEST(info.personality == 0x11, "personality round-trips");

    char needs[WUBU_APP_NEEDS_MAX][WUBU_APP_NAME_MAX];
    int nn = wubu_app_needs(a, needs, WUBU_APP_NEEDS_MAX);
    TEST(nn == 2, "two spell dependencies");
    TEST(strcmp(needs[0], "boot-attestation") == 0, "dependency[0]");
    TEST(strcmp(needs[1], "measurement-binding") == 0, "dependency[1]");

    uint32_t elen = 0;
    const uint8_t *entry = wubu_app_entry(a, &elen);
    TEST(entry != NULL && elen == 8, "entry payload returned with length");
    TEST(memcmp(entry, "MZDEMO..", 8) == 0, "entry bytes intact");

    TEST(strcmp(wubu_app_personality_name(0x11), "windows-pe") == 0,
         "personality name resolves");
    TEST(strcmp(wubu_app_personality_name(0x77), "unknown") == 0,
         "unknown personality named honestly");
    wubu_app_close(a);

    /* ---- 2. launch refuses what it cannot run ---------------------- */
    /* A real PE is needed to get past detection, so here we only prove the
     * refusal paths: no entry, and an unsupported personality. Both must fail
     * WITHOUT reaching the exec layer. */
    bspec_t noent = { 0x11, 0, 0, "empty" };
    memset(img, 0, sizeof(img));
    memcpy(img, WUBU_APP_MAGIC, 7);
    img[8] = (uint8_t)WUBU_APP_VERSION;
    img[12] = 1;
    img[16] = (uint8_t)noent.personality;
    memcpy(img + 20, "empty", 5);
    a = wubu_app_open(img, 128);
    TEST(a != NULL, "bundle with no entry still opens (data-only is legal)");
    TEST(wubu_app_launch(a) == WUBU_EXEC_ERR_FMT,
         "launching a bundle with no entry is refused");
    wubu_app_close(a);

    memset(img, 0, sizeof(img));
    memcpy(img, WUBU_APP_MAGIC, 7);
    img[8] = (uint8_t)WUBU_APP_VERSION;
    img[12] = 1;
    img[16] = 0x77;                       /* a personality we cannot run */
    memcpy(img + 20, "weird", 5);
    put_u32(img + 120, (uint32_t)eoff);
    put_u32(img + 124, 8);
    memcpy(img + eoff, "XYZXYZXY", 8);
    a = wubu_app_open(img, len);
    TEST(a != NULL, "unsupported-personality bundle parses");
    TEST(wubu_app_launch(a) == WUBU_EXEC_ERR_FMT,
         "an unsupported personality is refused before the exec layer");
    wubu_app_close(a);

    /* ---- 3. malformed bundles -------------------------------------- */
    memcpy(bad, img, len);

    memcpy(bad, img, len); bad[0] = 'X';
    TEST(wubu_app_open(bad, len) == NULL, "bad magic refused");

    memcpy(bad, img, len); bad[8] = 9;
    TEST(wubu_app_open(bad, len) == NULL, "unknown version refused");

    memcpy(bad, img, len); bad[12] = 0x40;
    TEST(wubu_app_open(bad, len) == NULL, "undefined flag refused");

    TEST(wubu_app_open(img, 100) == NULL, "truncated header refused");

    memcpy(bad, img, len); bad[116] = 200;      /* needs over cap */
    TEST(wubu_app_open(bad, len) == NULL, "absurd dependency count refused");

    memcpy(bad, img, len);
    put_u32(bad + 124, 200);                    /* entry_len past EOF */
    TEST(wubu_app_open(bad, len) == NULL, "entry running past the end refused");

    memcpy(bad, img, len);
    put_u32(bad + 120, 0xFFFF);                 /* entry offset past EOF */
    TEST(wubu_app_open(bad, len) == NULL, "entry offset past EOF refused");

    /* ---- 4. NULL handling ------------------------------------------ */
    TEST(wubu_app_open(NULL, 0) == NULL, "NULL buffer refused");
    TEST(wubu_app_info(NULL, &info) == -1, "info() refuses NULL");
    TEST(wubu_app_needs(NULL, needs, 4) == -1, "needs() refuses NULL");
    TEST(wubu_app_entry(NULL, &elen) == NULL, "entry() refuses NULL");
    TEST(wubu_app_launch(NULL) == WUBU_EXEC_ERR_FMT, "launch() refuses NULL");

    printf("=== app_bundle: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
