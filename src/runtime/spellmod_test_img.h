/*
 * spellmod_test_img.h -- builds valid .spell images in memory for tests.
 *
 * WHY A HEADER
 *   Two test targets construct synthetic modules (the resolver unit gate and the
 *   directory-scan gate).  A second copy of the layout maths would drift from the
 *   real format the moment somebody changes a field width in wubu_spellmod.c, and a
 *   drifted builder produces tests that validate a format nobody parses.  One
 *   header-only implementation keeps both honest.
 *
 * Header-only and dependency-free (string.h only), same reasoning as fuzz_rng.h.
 *
 * LIFETIME WARNING: wubu_spellmod_open() BORROWS the buffer -- it does not copy the
 * image.  Give every module its own storage that outlives the handle.
 */
#ifndef WUBU_SPELLMOD_TEST_IMG_H
#define WUBU_SPELLMOD_TEST_IMG_H

#include <stdint.h>
#include <string.h>

#include "../runtime/wubu_spellmod.h"

#define WUSPELL_TEST_HDR_SIZE 128
#define WUSPELL_TEST_STR_SIZE 64
#define WUSPELL_TEST_POOL     8
#define WUSPELL_TEST_BUF_SIZE 512

static inline void spell_test_put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

/*
 * Minimal valid image: np provides, nn needs, no exports, no code.
 * Layout fixed by wubu_spellmod.c -- 128-byte header, then 64-byte provides and
 * needs strings, then exports, then code. `buf` must hold at least the header plus
 * the lists.
 *
 * Returns the image length actually written (0 if the buffer is too small for even
 * the header).
 */
static inline size_t spell_test_build(uint8_t *buf, size_t cap,
                                      const char *name,
                                      const char *provides[], int np,
                                      const char *needs[], int nn)
{
    if (!buf || cap < WUSPELL_TEST_HDR_SIZE) return 0;
    memset(buf, 0, WUSPELL_TEST_HDR_SIZE);

    memcpy(buf, WUBU_SPELLMOD_MAGIC, 7);
    buf[7] = '\0';
    spell_test_put_u32(buf + 8, 1);      /* version */
    spell_test_put_u32(buf + 12, 0);     /* flags: no code */

    np = np > WUBU_SPELLMOD_LIST_MAX ? WUBU_SPELLMOD_LIST_MAX : np;
    nn = nn > WUBU_SPELLMOD_LIST_MAX ? WUBU_SPELLMOD_LIST_MAX : nn;
    spell_test_put_u32(buf + 112, (uint32_t)np);
    spell_test_put_u32(buf + 116, (uint32_t)nn);
    spell_test_put_u32(buf + 120, 0);    /* exports */
    spell_test_put_u32(buf + 124, 0);    /* code_len */

    strncpy((char *)buf + 16, name, WUBU_SPELLMOD_NAME_MAX - 1);
    strcpy((char *)buf + 80, "1.0");

    size_t off = WUSPELL_TEST_HDR_SIZE;
    for (int i = 0; i < np && off + WUSPELL_TEST_STR_SIZE <= cap;
         i++, off += WUSPELL_TEST_STR_SIZE) {
        memset(buf + off, 0, WUSPELL_TEST_STR_SIZE);
        strncpy((char *)buf + off, provides[i], WUSPELL_TEST_STR_SIZE - 1);
    }
    for (int i = 0; i < nn && off + WUSPELL_TEST_STR_SIZE <= cap;
         i++, off += WUSPELL_TEST_STR_SIZE) {
        memset(buf + off, 0, WUSPELL_TEST_STR_SIZE);
        strncpy((char *)buf + off, needs[i], WUSPELL_TEST_STR_SIZE - 1);
    }
    return off;
}

/*
 * Per-module buffer pool.  wubu_spellmod_open() borrows the image, so all eight
 * handles must not share one static buffer -- a single buffer leaves every handle
 * pointing at whichever image was built last.
 */
typedef struct {
    uint8_t bufs[WUSPELL_TEST_POOL][WUSPELL_TEST_BUF_SIZE];
    int next;
} spell_test_pool;

static inline uint8_t *spell_test_slot(spell_test_pool *pool)
{
    if (!pool || pool->next >= WUSPELL_TEST_POOL) return NULL;
    return pool->bufs[pool->next++];
}

#endif /* WUBU_SPELLMOD_TEST_IMG_H */
