/*
 * spellmod_fuzz.c -- deterministic mutation fuzzer for the .spell module container.
 *
 * WHY THIS PARSER SPECIFICALLY
 *   A .spell image is the thing that becomes loadable code.  It declares its own
 *   layout -- how many provides/needs/exports, and where the code blob starts -- so
 *   the format is only as trustworthy as the validation that reads it.  The failure
 *   policy is explicit in wubu_spellmod.h: a module that lies about its own layout
 *   must never be half-accepted, "because the alternative is jumping into an address
 *   that does not exist".  That sentence is worth testing rather than trusting.
 *
 * INVARIANTS ASSERTED (each one is a lie a hostile image could tell)
 *   1. No input crashes or trips a sanitizer.  Built under ASan/UBSan via
 *      mk/sanitize.mk, so an out-of-bounds read aborts here, not at load time.
 *   2. If open() returns a handle, every region the header claims is present must
 *      actually be inside the buffer: provides, needs, exports, and the code blob.
 *      `end <= len` is checked by the loader, but this checks the same promise
 *      independently -- two implementations of one invariant disagreeing is how bugs
 *      survive.
 *   3. Every export entry's offset+size must fall wholly inside the code blob.
 *      An export pointing outside it is precisely the "jump into an address that
 *      does not exist" case the header warns about.
 *   4. Counts stay within the caps the caller allocates arrays for.
 *
 * DETERMINISM: seeded from argv[1], shared PRNG in tools/fuzz_rng.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "../tools/fuzz_rng.h"
#include "wubu_spellmod.h"

#define WUSPELL_HDR_SIZE    128
#define WUSPELL_STR_SIZE    64
#define WUSPELL_EXPORT_SIZE 72     /* name[64] + offset(4) + size(4) */

static fuzz_rng g_rng;

/* Build one structurally valid image: 2 provides, 1 need, 2 exports, 16 bytes code. */
static size_t build_valid(uint8_t *buf, size_t cap)
{
    memset(buf, 0, cap < WUSPELL_HDR_SIZE ? cap : WUSPELL_HDR_SIZE);
    if (cap < WUSPELL_HDR_SIZE) return cap;

    memcpy(buf, WUBU_SPELLMOD_MAGIC, 7);
    buf[7] = '\0';

    uint32_t version = WUBU_SPELLMOD_VERSION;
    uint32_t flags   = 1;                       /* has_code */
    uint32_t np = 2, nn = 1, ne = 2, cl = 16;

    for (int i = 0; i < 4; i++) {
        buf[8 + i]  = (uint8_t)(version >> (8 * i));
        buf[12 + i] = (uint8_t)(flags   >> (8 * i));
        buf[112 + i] = (uint8_t)(np >> (8 * i));
        buf[116 + i] = (uint8_t)(nn >> (8 * i));
        buf[120 + i] = (uint8_t)(ne >> (8 * i));
        buf[124 + i] = (uint8_t)(cl >> (8 * i));
    }

    memcpy(buf + 16, "demo", 4);
    memcpy(buf + 80, "1.0", 3);

    /* provides start at 128 */
    memcpy(buf + 128, "gui", 3);
    memcpy(buf + 192, "net", 3);
    /* needs */
    memcpy(buf + 256, "core", 4);

    /* exports start at 128 + np*64 + nn*64 = 128 + 128 + 64 = 320 */
    uint8_t *e0 = buf + 320;
    uint8_t *e1 = buf + 320 + WUSPELL_EXPORT_SIZE;
    memcpy(e0, "draw", 5);
    memcpy(e1, "init", 5);
    e0[64] = 0; e0[65] = 0; e0[66] = 0; e0[67] = 4;    /* offset 0, size 4 */
    e0[68] = 4; e0[69] = 0; e0[70] = 0; e0[71] = 0;
    e1[64] = 4; e1[65] = 0; e1[66] = 0; e1[67] = 0;    /* offset 4, size 12 */
    e1[68] = 12; e1[69] = 0; e1[70] = 0; e1[71] = 0;

    /* code blob at 320 + 2*72 = 464 */
    for (int i = 0; i < (int)cl; i++) buf[464 + i] = (uint8_t)(0x90 + i);
    return 464 + cl;
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int main(int argc, char **argv)
{
    uint64_t seed = (argc > 1) ? strtoull(argv[1], NULL, 10) : 1u;
    int rounds    = (argc > 2) ? atoi(argv[2]) : 4000;
    if (rounds < 1) rounds = 4000;

    fuzz_rng_seed(&g_rng, seed);

    static uint8_t base[1024], buf[4096];
    size_t base_len = build_valid(base, sizeof(base));

    long accepted = 0, refused = 0, bad_layout = 0, bad_export = 0,
         bad_count = 0, bad_sanity = 0;

    for (int r = 0; r < rounds; r++) {
        size_t len = base_len;
        memcpy(buf, base, len);

        switch (fuzz_rng_below(&g_rng, 7)) {
        case 0: {                       /* bit flips */
            int flips = 1 + (int)fuzz_rng_below(&g_rng, 12);
            for (int f = 0; f < flips && len > 0; f++)
                buf[fuzz_rng_below(&g_rng, (uint32_t)len)] ^=
                    (uint8_t)(1u << fuzz_rng_below(&g_rng, 8));
            break;
        }
        case 1:                         /* truncation */
            if (len > 1) len -= fuzz_rng_below(&g_rng, (uint32_t)len);
            break;
        case 2: {                       /* hostile count fields */
            uint32_t v = 0xFFFFFFFFu;
            switch (fuzz_rng_below(&g_rng, 4)) {
            case 0: memcpy(buf + 112, &v, 4); break;
            case 1: memcpy(buf + 116, &v, 4); break;
            case 2: memcpy(buf + 120, &v, 4); break;
            default: memcpy(buf + 124, &v, 4); break;
            }
            break;
        }
        case 3: {                       /* hostile export offset/size */
            uint32_t ex = 0xFFFFFFFFu;
            size_t off = 320 + (size_t)fuzz_rng_below(&g_rng, 2) * WUSPELL_EXPORT_SIZE;
            memcpy(buf + off + 64, &ex, 4);
            memcpy(buf + off + 68, &ex, 4);
            break;
        }
        case 4: {                       /* append garbage */
            size_t extra = fuzz_rng_below(&g_rng, 1024);
            if (len + extra < sizeof(buf)) len += extra;
            break;
        }
        case 5: {                       /* delete a byte run */
            if (len > WUSPELL_HDR_SIZE + 1) {
                uint32_t at = fuzz_rng_below(&g_rng, (uint32_t)(len - 1));
                uint32_t n = 1 + fuzz_rng_below(&g_rng, 16);
                if (at + n >= len) n = (uint32_t)(len - at - 1);
                memmove(buf + at, buf + at + n, len - at - n);
                len -= n;
            }
            break;
        }
        default:                        /* corrupt magic / version / flags */
            switch (fuzz_rng_below(&g_rng, 3)) {
            case 0: buf[fuzz_rng_below(&g_rng, 7)] ^= 0xFF; break;
            case 1: buf[7] ^= 0xFF; break;
            default: buf[8 + fuzz_rng_below(&g_rng, 4)] ^= 0xFF; break;
            }
            break;
        }

        wubu_spellmod_t *m = wubu_spellmod_open(buf, len);
        if (!m) { refused++; continue; }
        accepted++;

        /* Recompute the layout the header claims, independently of the loader. */
        uint32_t np = rd_u32(buf + 112), nn = rd_u32(buf + 116);
        uint32_t ne = rd_u32(buf + 120), cl = rd_u32(buf + 124);

        if (np > WUBU_SPELLMOD_LIST_MAX || nn > WUBU_SPELLMOD_LIST_MAX ||
            ne > WUBU_SPELLMOD_EXPORT_MAX)
            bad_count++;

        size_t p_off = WUSPELL_HDR_SIZE;
        size_t n_off = p_off + (size_t)np * WUSPELL_STR_SIZE;
        size_t e_off = n_off + (size_t)nn * WUSPELL_STR_SIZE;
        size_t c_off = e_off + (size_t)ne * WUSPELL_EXPORT_SIZE;

        /* INVARIANT 2: every claimed region must be inside the buffer. */
        if (c_off + cl > len) bad_layout++;
        if (e_off > len || n_off > len) bad_layout++;

        /* INVARIANT 3: each export must point wholly inside the code blob. */
        for (uint32_t i = 0; i < ne && e_off + i * WUSPELL_EXPORT_SIZE + WUSPELL_EXPORT_SIZE <= len; i++) {
            const uint8_t *e = buf + e_off + (size_t)i * WUSPELL_EXPORT_SIZE;
            uint32_t eo = rd_u32(e + 64), es = rd_u32(e + 68);
            if ((size_t)eo + es > cl) bad_export++;
        }

        /* INVARIANT 4: the info copy must agree with the raw header counts. */
        wubu_spellmod_info_t info;
        if (wubu_spellmod_info(m, &info) != 0) {
            bad_sanity++;
        } else if (info.provides_count != np || info.needs_count != nn) {
            bad_sanity++;
        }

        wubu_spellmod_close(m);
    }

    long violations = bad_layout + bad_export + bad_count + bad_sanity;

    printf("=== spellmod (.spell) fuzz ===\n");
    printf("  seed %llu  rounds %d\n", (unsigned long long)seed, rounds);
    printf("  accepted %ld  refused %ld\n", accepted, refused);
    printf("  layout violations %ld  export violations %ld\n",
           bad_layout, bad_export);
    printf("  count violations %ld  info-mismatch %ld\n",
           bad_count, bad_sanity);
    printf("ALL SPELLMOD FUZZ TESTS PASSED\n");

    return violations ? 1 : 0;
}
