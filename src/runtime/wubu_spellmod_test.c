/*
 * wubu_spellmod_test.c -- the .spell container contract.
 *
 * Builds real images in memory and checks both directions: a well-formed module
 * is accepted and every accessor agrees with what was written, and a malformed
 * one is refused WITHOUT touching the caller's output.  A half-accepted module
 * is the failure this format cannot allow -- it would put a caller at an
 * address that does not exist.
 */

#include "wubu_spellmod.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks = 0, g_fails = 0;
#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

/* ---- builder ---------------------------------------------------------- */

typedef struct {
    uint32_t provides;
    uint32_t needs;
    uint32_t exports;
    uint32_t code_len;
    const char *name;
} build_spec_t;

static size_t build(uint8_t *buf, size_t cap, const build_spec_t *sp)
{
    size_t off = 128;
    memset(buf, 0, off);
    memcpy(buf, WUBU_SPELLMOD_MAGIC, 7);
    buf[7] = '\0';
    buf[8] = (uint8_t)(WUBU_SPELLMOD_VERSION & 0xff);
    buf[9] = buf[10] = buf[11] = 0;
    buf[12] = 1;                       /* flags: has_code */

    if (sp->name) {
        size_t n = strlen(sp->name);
        if (n > 63) n = 63;
        memcpy(buf + 16, sp->name, n);
    }
    memcpy(buf + 80, "0.1.0", 5);

    buf[112] = (uint8_t)sp->provides;
    buf[116] = (uint8_t)sp->needs;
    buf[120] = (uint8_t)sp->exports;
    buf[124] = (uint8_t)sp->code_len;

    static const char *pv[] = { "boot-attestation", "measurement-binding" };
    for (uint32_t i = 0; i < sp->provides && i < 2; i++) {
        memcpy(buf + off, pv[i], strlen(pv[i]));
        off += 64;
    }
    off += (size_t)(sp->provides > 2 ? sp->provides - 2 : 0) * 64;

    memcpy(buf + off, "sdr-transport", 13);   /* one need */
    off += (size_t)sp->needs * 64;

    /* export "measure" -> offset 4, size 8 */
    memcpy(buf + off, "measure", 7);
    buf[off + 64] = 4;
    buf[off + 68] = 8;
    off += (size_t)sp->exports * 72;

    static const uint8_t code[] = { 'A','B','C','D', 1,2,3,4, 5,6,7,8 };
    if (sp->code_len <= sizeof(code)) {
        memcpy(buf + off, code, sp->code_len);
    }
    off += sp->code_len;

    (void)cap;
    return off;
}

int main(void)
{
    static uint8_t img[1024];

    /* ---- 1. a well-formed module ------------------------------------- */
    build_spec_t sp = { 2, 1, 1, 12, "bootattest" };
    size_t len = build(img, sizeof(img), &sp);

    wubu_spellmod_t *m = wubu_spellmod_open(img, len);
    TEST(m != NULL, "a well-formed module opens");

    wubu_spellmod_info_t info;
    memset(&info, 0xAA, sizeof(info));          /* detect untouched fields */
    TEST(wubu_spellmod_info(m, &info) == 0, "info() succeeds");
    TEST(strcmp(info.name, "bootattest") == 0, "name round-trips");
    TEST(strcmp(info.version, "0.1.0") == 0, "version round-trips");
    TEST(info.provides_count == 2, "two provides");
    TEST(strcmp(info.provides[0], "boot-attestation") == 0, "provide[0]");
    TEST(info.needs_count == 1, "one need");
    TEST(strcmp(info.needs[0], "sdr-transport") == 0, "need[0]");

    uint32_t eoff = 0xFFFFFFFFu, esz = 0xFFFFFFFFu;
    TEST(wubu_spellmod_export(m, "measure", &eoff, &esz) == 0, "export resolves");
    TEST(eoff == 4, "export offset correct");
    TEST(esz == 8, "export size correct");

    uint32_t clen = 0;
    const uint8_t *code = wubu_spellmod_code(m, &clen);
    TEST(code != NULL && clen == 12, "code blob returned with its length");
    TEST(memcmp(code, "ABCD", 4) == 0, "code bytes intact");

    TEST(wubu_spellmod_provides(m, "boot-attestation"), "provides() true");
    TEST(!wubu_spellmod_provides(m, "nonexistent"), "provides() false");
    TEST(wubu_spellmod_export(m, "nope", &eoff, &esz) == -1,
         "missing export refused");
    wubu_spellmod_close(m);

    /* ---- 2. refusal must leave outputs untouched --------------------- */
    uint32_t keep_off = 0xDEAD, keep_sz = 0xBEEF;
    m = wubu_spellmod_open(img, len);
    TEST(m != NULL, "re-open for refusal test");
    TEST(wubu_spellmod_export(m, "", &keep_off, &keep_sz) == -1,
         "empty name refused");
    TEST(keep_off == 0xDEAD && keep_sz == 0xBEEF,
         "refusal leaves the caller's outputs untouched");
    wubu_spellmod_close(m);

    /* ---- 3. malformed images ----------------------------------------- */
    static uint8_t bad[1024];

    /* bad magic */
    memcpy(bad, img, len);
    bad[0] = 'X';
    TEST(wubu_spellmod_open(bad, len) == NULL, "bad magic refused");

    /* unknown version */
    memcpy(bad, img, len);
    bad[8] = 9;
    TEST(wubu_spellmod_open(bad, len) == NULL, "unknown version refused");

    /* undefined flag bit */
    memcpy(bad, img, len);
    bad[12] = 0x80;
    TEST(wubu_spellmod_open(bad, len) == NULL, "unknown flag refused");

    /* truncated header */
    TEST(wubu_spellmod_open(img, 100) == NULL, "truncated header refused");

    /* absurd counts */
    memcpy(bad, img, len);
    bad[112] = 200;                                  /* provides > cap */
    TEST(wubu_spellmod_open(bad, len) == NULL, "absurd provide count refused");

    /* payload shorter than the declared regions */
    memcpy(bad, img, len);
    bad[124] = 200;                                  /* code_len too large */
    TEST(wubu_spellmod_open(bad, len) == NULL, "over-long code region refused");

    /* export pointing outside the code blob */
    memcpy(bad, img, len);
    bad[128 + 2*64 + 64 + 64] = 200;                 /* export offset past code */
    TEST(wubu_spellmod_open(bad, len) != NULL, "image still parses");
    {
        wubu_spellmod_t *bm = wubu_spellmod_open(bad, len);
        TEST(wubu_spellmod_export(bm, "measure", &eoff, &esz) == -1,
             "an export outside the code blob is refused");
        wubu_spellmod_close(bm);
    }

    /* NULL / empty inputs */
    TEST(wubu_spellmod_open(NULL, 0) == NULL, "NULL buffer refused");
    TEST(wubu_spellmod_export(NULL, "x", &eoff, &esz) == -1, "NULL module refused");
    TEST(wubu_spellmod_info(NULL, &info) == -1, "NULL module refused by info()");
    TEST(wubu_spellmod_code(NULL, &clen) == NULL, "NULL module refused by code()");

    /* ---- 4. a module with no code is legal --------------------------- */
    build_spec_t nc = { 0, 0, 0, 0, "pure-manifest" };
    len = build(img, sizeof(img), &nc);
    m = wubu_spellmod_open(img, len);
    TEST(m != NULL, "a manifest-only module opens");
    TEST(wubu_spellmod_code(m, &clen) == NULL && clen == 0,
         "no code means no code pointer");
    wubu_spellmod_close(m);

    printf("=== spellmod: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
