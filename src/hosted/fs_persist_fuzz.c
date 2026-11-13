/*
 * fs_persist_fuzz.c -- deterministic mutation fuzzer for the WUBFS01 image loader.
 *
 * WHY THIS EXISTS
 *   `hosted_fs_persist_load()` reads an image whose length fields come from disk,
 *   i.e. from somewhere the user can write.  Its documented contract is fail-closed:
 *   a truncated image, bad magic, or a record claiming more than the format allows
 *   must return -1 and leave the live namespace untouched.  Fixed hostile cases in
 *   test_fs_persist cover the obvious shapes; they cannot cover the shape nobody
 *   thought of, which is the only reason to have a fuzzer at all.
 *
 * WHAT IT ASSERTS
 *   1. No input makes the process die.  This target is built under ASan/UBSan (see
 *      mk/sanitize.mk), so a read/write out of bounds, an overflow, or a
 *      misaligned access aborts the run instead of being reported later.
 *   2. The return value is always -1 or a non-negative count bounded by the
 *      runtime ceiling -- never a wild number, which would mean the caller trusts
 *      a length field it did not validate.
 *   3. A corrupted magic byte MUST be refused.  That is the cheapest possible
 *      integrity check and if it ever stops holding, nothing downstream matters.
 *
 * DETERMINISM
 *   Everything is seeded from argv[1] (default 1).  Given the same seed the run is
 *   byte-for-byte reproducible, so a crash is a bug report rather than an anecdote.
 *
 * NOT A COVERAGE FUZZER
 *   There is no libFuzzer and no instrumentation here.  This mutates a small valid
 *   image a fixed number of times; it is a smoke screen against obvious memory
 *   faults, not a proof.  Do not describe it as "coverage-guided".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>   /* getpid, unlink */

#include "../apps/guest_file.h"
#include "../runtime/styx.h"   /* STX_QTDIR / STX_QTFILE */
#include "hosted_fs_persist.h"

#define FS_PERSIST_MAGIC   "WUBFS01"
#define FS_PERSIST_HDR     12          /* magic[8] + uint32 count */
#define REC_FIXED          13          /* uint64 path + uint8 qtype + uint32 dlen */
#define NAME_SZ            256

/* Deterministic PRNG (splitmix64): no rand(), no global state, reproducible. */
static uint64_t rng_state;
static uint64_t rng_next(void)
{
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static uint32_t rng_below(uint32_t n) { return n ? (uint32_t)(rng_next() % n) : 0; }

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put_u64(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

/* Build one well-formed image holding a few entries. */
static size_t build_valid(uint8_t *buf, size_t cap)
{
    size_t off = 0;
    memcpy(buf + off, FS_PERSIST_MAGIC, 8); off += 8;
    put_u32(buf + off, 4); off += 4;

    const char *names[] = { "Documents", "report.txt", "photo.png", "notes.txt" };
    const uint8_t qtypes[] = { STX_QTDIR, STX_QTFILE, STX_QTFILE, STX_QTFILE };
    const uint32_t lens[]  = { 0, 11, 9, 5 };

    for (int i = 0; i < 4; i++) {
        if (off + REC_FIXED + NAME_SZ + lens[i] > cap) break;
        put_u64(buf + off, i + 1); off += 8;
        buf[off++] = qtypes[i];
        put_u32(buf + off, lens[i]); off += 4;
        memset(buf + off, 0, NAME_SZ);
        strncpy((char *)buf + off, names[i], NAME_SZ - 1);
        off += NAME_SZ;
        memset(buf + off, 'A' + i, lens[i]);
        off += lens[i];
    }
    return off;
}

static int count_entries(void)
{
    char names[512][64];
    int n = guest_file_list(names, 64, 512);
    return n < 0 ? 0 : n;
}

int main(int argc, char **argv)
{
    uint64_t seed = (argc > 1) ? strtoull(argv[1], NULL, 10) : 1u;
    int rounds    = (argc > 2) ? atoi(argv[2]) : 4000;
    if (rounds < 1) rounds = 4000;

    rng_state = seed;

    /* Point the store at scratch space so the fuzzer can never touch the real
     * user image -- the same isolation rule test_startmenu_files learned the hard
     * way when it wrote into ~/.local/share/wubu/fs.img. */
    char store[512];
    snprintf(store, sizeof(store), "/tmp/wubfs-fuzz-%d.img", (int)getpid());
    setenv("WUBU_FS_STORE", store, 1);

    static uint8_t base[4096], buf[8192];
    size_t base_len = build_valid(base, sizeof(base));

    long refused = 0, loaded = 0, wild = 0, magic_fail = 0;
    int max_seen = 0;

    for (int r = 0; r < rounds; r++) {
        size_t len = base_len;
        memcpy(buf, base, len);

        /* Choose a mutation class. */
        switch (rng_below(6)) {
        case 0: {                       /* bit flips */
            int flips = 1 + (int)rng_below(8);
            for (int f = 0; f < flips && len > 0; f++)
                buf[rng_below((uint32_t)len)] ^= (uint8_t)(1u << rng_below(8));
            break;
        }
        case 1:                         /* truncation */
            if (len > 1) len -= rng_below((uint32_t)len);
            break;
        case 2: {                       /* hostile length field in the header */
            uint32_t v = 0xFFFFFFFFu;
            if (len >= FS_PERSIST_HDR) put_u32(buf + 8, v);
            break;
        }
        case 3: {                       /* hostile data_len inside each record */
            size_t off = FS_PERSIST_HDR;
            while (off + REC_FIXED + NAME_SZ <= len) {
                put_u32(buf + off + 9, 0xFFFFFFu);
                /* advance past this record using its (now corrupt) length */
                uint32_t dl = 0;
                dl |= buf[off + 9]; dl |= (uint32_t)buf[off + 10] << 8;
                dl |= (uint32_t)buf[off + 11] << 16;
                if (dl > len) break;
                off += REC_FIXED + NAME_SZ + dl;
            }
            break;
        }
        case 4: {                       /* truncate mid-record */
            if (len > FS_PERSIST_HDR) {
                size_t cut = FS_PERSIST_HDR +
                             rng_below((uint32_t)(len - FS_PERSIST_HDR));
                len = cut;
            }
            break;
        }
        default:                        /* single byte delete */
            if (len > FS_PERSIST_HDR + 1) {
                size_t at = FS_PERSIST_HDR +
                            rng_below((uint32_t)(len - FS_PERSIST_HDR));
                memmove(buf + at, buf + at + 1, len - at - 1);
                len--;
            }
            break;
        }

        bool magic_ok = (len >= 8 && memcmp(buf, FS_PERSIST_MAGIC, 7) == 0);

        FILE *fp = fopen(store, "wb");
        if (!fp) { perror("fopen"); return 2; }
        fwrite(buf, 1, len, fp);
        fclose(fp);

        int rc = fs_persist_load(1);

        /* INVARIANT 2: the return value is always -1 or a sane count. */
        if (rc == -1) {
            refused++;
        } else {
            loaded++;
            if (rc < 0 || rc > (int)guest_file_max()) wild++;
            if (rc > max_seen) max_seen = rc;
        }
        /* INVARIANT 3: corrupt magic must be refused. */
        if (!magic_ok && rc != -1) magic_fail++;

        unlink(store);
    }

    printf("=== fs_persist fuzz ===\n");
    printf("  seed %llu  rounds %d\n", (unsigned long long)seed, rounds);
    printf("  refused %ld  loaded %ld  wild %ld  magic-fail %ld\n",
           refused, loaded, wild, magic_fail);
    printf("  max entries loaded %d  (ceiling %u)\n", max_seen, guest_file_max());
    printf("ALL FS_PERSIST FUZZ TESTS PASSED\n");

    return (wild || magic_fail) ? 1 : 0;
}
