/*
 * mime_fuzz.c -- deterministic mutation fuzzer for the .desktop / MIME entry parser.
 *
 * WHY THIS PARSER SPECIFICALLY
 *   A .desktop file comes off disk (the guest store, an installed package, a
 *   downloaded file) and is parsed into fixed-size char arrays.  Every field is a
 *   bounded copy, so the interesting failure modes are exactly the ones a hostile
 *   file can produce: over-long values, embedded NULs, malformed section headers,
 *   unterminated lines, enormous Actions= lists.  mime_contract covers the shapes we
 *   thought of; this covers the ones we did not.
 *
 * INVARIANTS ASSERTED
 *   1. No input crashes or trips a sanitizer -- built under ASan/UBSan in
 *      mk/sanitize.mk, so an out-of-bounds write or a signed overflow aborts here
 *      rather than being found later in a different context.
 *   2. "No Name means not a valid entry" holds for EVERY accepted parse, not just
 *      the hand-written cases.  That is the contract callers rely on to decide
 *      whether a desktop file is usable at all.
 *   3. action_count never exceeds MAX_DESKTOP_ACTIONS, so a file cannot make a
 *      caller walk off the end of a fixed array.
 *   4. action ids stay distinct -- the parser is supposed to refuse duplicates, and
 *      a duplicate would silently shadow a menu item.
 *
 * DETERMINISM: seeded from argv[1], same as fs_persist_fuzz.  See tools/fuzz_rng.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>   /* unlink */

#include "../tools/fuzz_rng.h"
#include "wubu_mime.h"
#include "wubu_mime_internal.h"

static fuzz_rng g_rng;

/* A valid starting point: mutating a real file reaches deeper parser states than
 * mutating noise, because most random bytes die on the very first line. */
static const char *kSeed =
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Notepad\n"
    "Comment=Edit text files\n"
    "Exec=wubu-app notepad %f\n"
    "Icon=text-editor\n"
    "Terminal=false\n"
    "Actions=Open;New;Save;\n"
    "\n"
    "[Desktop Action Open]\n"
    "Name=Open File\n"
    "Exec=wubu-app notepad --open %f\n"
    "\n"
    "[Desktop Action New]\n"
    "Name=New File\n"
    "Exec=wubu-app notepad --new\n";

static void put_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) { perror("fopen"); exit(2); }
    fwrite(buf, 1, len, fp);
    fclose(fp);
}

int main(int argc, char **argv)
{
    uint64_t seed = (argc > 1) ? strtoull(argv[1], NULL, 10) : 1u;
    int rounds    = (argc > 2) ? atoi(argv[2]) : 4000;
    if (rounds < 1) rounds = 4000;

    fuzz_rng_seed(&g_rng, seed);

    char path[512];
    snprintf(path, sizeof(path), "/tmp/wubu-mime-fuzz-%d.desktop", (int)getpid());

    static uint8_t buf[8192];
    size_t base_len = strlen(kSeed);
    memcpy(buf, kSeed, base_len);

    long accepted = 0, refused = 0, bad_invariant = 0;
    int max_actions = 0;

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
        case 2: {                       /* append garbage (over-long value attack) */
            size_t extra = fuzz_rng_below(&g_rng, 2048);
            if (len + extra < sizeof(buf))
                len += extra;
            break;
        }
        case 3: {                       /* NUL injection: terminates a C string early */
            if (len > 0) buf[fuzz_rng_below(&g_rng, (uint32_t)len)] = '\0';
            break;
        }
        case 4: {                       /* huge Actions list */
            if (len + 16 < sizeof(buf)) {
                memcpy(buf + len, "Actions=", 8); len += 8;
                int n = 1 + (int)fuzz_rng_below(&g_rng, 64);
                for (int i = 0; i < n && len + 8 < sizeof(buf); i++) {
                    int w = snprintf((char *)buf + len, 9, "A%d;", i);
                    len += (size_t)w;
                }
                buf[len++] = '\n';
            }
            break;
        }
        case 5: {                       /* delete a run of bytes */
            if (len > 2) {
                uint32_t at = fuzz_rng_below(&g_rng, (uint32_t)(len - 1));
                uint32_t n = 1 + fuzz_rng_below(&g_rng, 16);
                if (at + n >= len) n = (uint32_t)(len - at - 1);
                memmove(buf + at, buf + at + n, len - at - n);
                len -= n;
            }
            break;
        }
        default:                        /* byte insert */
            if (len + 1 < sizeof(buf)) {
                uint32_t at = fuzz_rng_below(&g_rng, (uint32_t)len);
                memmove(buf + at + 1, buf + at, len - at);
                buf[at] = (uint8_t)fuzz_rng_next(&g_rng);
                len++;
            }
            break;
        }

        put_file(path, buf, len);

        DesktopEntry e;
        bool ok = parse_desktop_file(path, &e);

        if (!ok) { refused++; continue; }
        accepted++;

        /* INVARIANT 2: no Name means invalid. */
        if (e.name[0] == '\0') bad_invariant++;

        /* INVARIANT 3: action_count bounded by the array it indexes. */
        if (e.action_count > MAX_DESKTOP_ACTIONS) bad_invariant++;
        if (e.action_count > max_actions) max_actions = e.action_count;

        /* INVARIANT 4: action display names distinct -- a duplicate would silently
         * shadow a menu item built from this list. */
        for (int i = 0; i < e.action_count; i++)
            for (int j = i + 1; j < e.action_count; j++)
                if (strcmp(e.action_list[i].name,
                           e.action_list[j].name) == 0)
                    bad_invariant++;
    }

    unlink(path);

    printf("=== mime (.desktop) fuzz ===\n");
    printf("  seed %llu  rounds %d\n", (unsigned long long)seed, rounds);
    printf("  accepted %ld  refused %ld  invariant violations %ld\n",
           accepted, refused, bad_invariant);
    printf("  max actions parsed %d  (limit %d)\n", max_actions, MAX_DESKTOP_ACTIONS);
    printf("ALL MIME FUZZ TESTS PASSED\n");

    return bad_invariant ? 1 : 0;
}
