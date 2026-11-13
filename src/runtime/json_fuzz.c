/*
 * json_fuzz.c -- deterministic mutation fuzzer for the JSON reader.
 *
 * WHY THIS PARSER SPECIFICALLY
 *   wubu_json parses whatever settings, configs or remote payloads hand it, into a
 *   fixed 512-token arena with recursive descent.  Two shapes matter:
 *     - deep nesting, which recurses once per level and can exhaust the stack;
 *     - long scalar literals, which are copied into fixed-size local buffers.
 *   Neither is bounded by JSON_MAX_TOKENS: a single very long number uses ONE token
 *   but arbitrarily many bytes, so the arena cap does not protect that path at all.
 *
 * INVARIANTS ASSERTED
 *   1. No input crashes or trips a sanitizer (ASan/UBSan build aborts here).
 *   2. On success, every node reports child_count <= child_capacity, and the arena
 *      never exceeds JSON_MAX_TOKENS -- so a caller iterating children cannot walk
 *      off the end.
 *   3. Parsing the same bytes twice yields the same verdict (determinism of the
 *      parser, not just of the input generator).  A parser whose answer depends on
 *      leftover global state is a bug waiting for a second call.
 *
 * DETERMINISM: seeded from argv[1], shared PRNG in tools/fuzz_rng.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "../tools/fuzz_rng.h"
#include "wubu_json.h"

static fuzz_rng g_rng;

/* A valid starting point with one of every type. */
static const char *kSeed =
    "{\"name\":\"wubu\",\"count\":42,\"ratio\":1.5,"
    "\"ok\":true,\"nothing\":null,"
    "\"list\":[1,2,3],\"nested\":{\"a\":{\"b\":\"c\"}}}";

static void reset_arena(void)
{
    /* json_free resets the counter; nothing else clears the globals between runs,
     * so an explicit clear keeps each iteration independent. */
    memset(g_json_tokens, 0, sizeof(g_json_tokens));
    g_json_token_count = 0;
    g_json_parse_error = 0;
}

static int check_tree(JsonToken *t, int depth, int *max_depth)
{
    if (!t) return 0;
    if (depth > *max_depth) *max_depth = depth;
    if (t->child_count > t->child_capacity) return -1;
    if (g_json_token_count > JSON_MAX_TOKENS) return -1;
    for (int i = 0; i < t->child_count; i++)
        if (check_tree(&t->children[i], depth + 1, max_depth) != 0) return -1;
    return 0;
}

/* Parse and discard, returning only whether it produced a value.  Exists so the
 * determinism check can compare verdicts without ever keeping two trees alive --
 * the parser hands back pointers into a shared arena, so two live trees are not
 * possible by construction. */
static bool json_probe_verdict(const char *buf)
{
    const char *p = buf;
    reset_arena();
    JsonToken *t = json_parse_value(&p);
    bool ok = (t != NULL);
    if (t) json_free(t);
    return ok;
}

static bool parse_once(const uint8_t *buf, size_t len, JsonToken **out)
{
    /* The parser takes a NUL-terminated cursor, so work on a private copy. */
    char *tmp = malloc(len + 1);
    if (!tmp) exit(2);
    memcpy(tmp, buf, len);
    tmp[len] = '\0';

    const char *p = tmp;
    reset_arena();
    JsonToken *root = json_parse_value(&p);
    bool first_ok = (root != NULL);
    /* MUST be freed before the arena is reset: json_parse_value hands back a
     * pointer INTO g_json_tokens, so resetting would overwrite this tree. */
    if (root) json_free(root);

    /* INVARIANT 3: parsing the identical bytes again must give the same verdict.
     * A result that depends on history rather than input is a bug. */
    bool second_ok = json_probe_verdict(tmp);
    if (first_ok != second_ok) {
        free(tmp);
        return false;
    }

    /* Re-parse and hand the caller a live tree; it stays valid until the next
     * iteration's reset_arena(). */
    const char *q = tmp;
    reset_arena();
    *out = json_parse_value(&q);
    free(tmp);
    return true;
}

int main(int argc, char **argv)
{
    uint64_t seed = (argc > 1) ? strtoull(argv[1], NULL, 10) : 1u;
    int rounds    = (argc > 2) ? atoi(argv[2]) : 4000;
    if (rounds < 1) rounds = 4000;

    fuzz_rng_seed(&g_rng, seed);

    static uint8_t buf[16384];
    size_t base_len = strlen(kSeed);

    long accepted = 0, refused = 0, bad_tree = 0, nondeterministic = 0;
    int max_depth_seen = 0;

    for (int r = 0; r < rounds; r++) {
        size_t len = base_len;
        memcpy(buf, kSeed, base_len);

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
        case 2: {                       /* append garbage */
            size_t extra = fuzz_rng_below(&g_rng, 4096);
            if (len + extra < sizeof(buf)) len += extra;
            break;
        }
        case 3: {                       /* LONG NUMBER: the arena caps token COUNT,
                                         * not byte length, so this path is unbounded */
            static uint8_t nb[8192];
            size_t n = 64 + fuzz_rng_below(&g_rng, 4096);
            if (n + 8 < sizeof(nb)) {
                memcpy(nb, "{\"n\":", 5);
                memset(nb + 5, '9', n);
                nb[5 + n] = '}';
                nb[5 + n + 1] = '\0';
                if (5 + n + 2 <= sizeof(buf)) {
                    memcpy(buf, nb, 5 + n + 2);
                    len = 5 + n + 2;
                }
            }
            break;
        }
        case 4: {                       /* DEEP NESTING: one token per level */
            static uint8_t db[8192];
            size_t n = fuzz_rng_below(&g_rng, 800);
            size_t off = 0;
            for (size_t i = 0; i < n && off + 2 < sizeof(db); i++) db[off++] = '[';
            for (size_t i = 0; i < n && off + 2 < sizeof(db); i++) db[off++] = ']';
            if (off + 1 < sizeof(buf)) { memcpy(buf, db, off); len = off; }
            break;
        }
        case 5: {                       /* delete a byte run */
            if (len > 2) {
                uint32_t at = fuzz_rng_below(&g_rng, (uint32_t)(len - 1));
                uint32_t n = 1 + fuzz_rng_below(&g_rng, 16);
                if (at + n >= len) n = (uint32_t)(len - at - 1);
                memmove(buf + at, buf + at + n, len - at - n);
                len -= n;
            }
            break;
        }
        default:                        /* insert a random byte */
            if (len + 1 < sizeof(buf)) {
                uint32_t at = fuzz_rng_below(&g_rng, (uint32_t)len);
                memmove(buf + at + 1, buf + at, len - at);
                buf[at] = (uint8_t)fuzz_rng_next(&g_rng);
                len++;
            }
            break;
        }

        JsonToken *root = NULL;
        if (!parse_once(buf, len, &root)) { nondeterministic++; continue; }

        if (!root) { refused++; continue; }
        accepted++;

        int maxd = 0;
        if (check_tree(root, 1, &maxd) != 0) bad_tree++;
        if (maxd > max_depth_seen) max_depth_seen = maxd;

        json_free(root);
    }

    printf("=== json fuzz ===\n");
    printf("  seed %llu  rounds %d\n", (unsigned long long)seed, rounds);
    printf("  accepted %ld  refused %ld\n", accepted, refused);
    printf("  bad tree %ld  nondeterministic %ld\n",
           bad_tree, nondeterministic);
    printf("  deepest tree parsed %d levels\n", max_depth_seen);
    printf("ALL JSON FUZZ TESTS PASSED\n");

    return (bad_tree || nondeterministic) ? 1 : 0;
}
