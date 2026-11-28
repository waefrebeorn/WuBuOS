/* wubu_gif_anim_test.c -- does our compositor agree with Pillow, frame by frame?
 *
 * The oracle is tools/make_gif_oracle.py: five real animated GIFs, one per disposal method
 * plus an interlaced one, with Pillow's own composited RGBA reduced to FNV-1a digests. We
 * compare frame COUNT, per-frame DELAY, and per-frame PIXELS. A decoder compared only to
 * itself proves nothing; this one is held to an independent implementation that browsers
 * agree with.
 */
#include "wubu_gif.h"
#include "../../tests/fixtures/gif_anim_expect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures;

#define CHECK(cond, fmt, ...) do { \
    checks++; \
    if (!(cond)) { failures++; printf("  FAIL: " fmt "\n", ##__VA_ARGS__); } \
} while (0)

static uint64_t fnv1a64(const uint8_t *p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h = h * 0x100000001B3ULL; }
    return h;
}

static uint8_t *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

int main(void) {
    for (int c = 0; c < WUBU_GIF_ANIM_CASES; c++) {
        const WubuGifAnimExpect *x = &WUBU_GIF_ANIM_EXPECT[c];
        char path[256];
        snprintf(path, sizeof(path), "tests/fixtures/gif_anim/%s", x->file);
        size_t len = 0;
        uint8_t *data = read_file(path, &len);
        if (!data) { printf("  SKIP: cannot read %s\n", path); failures++; checks++; continue; }

        int err = 0, count = 0;
        int rc = wubu_gif_frame_count(data, len, &count, &err);
        CHECK(rc == 0, "%s: frame_count failed: %s", x->file, wubu_gif_reason(err));
        CHECK(count == x->frames, "%s: %d frames, Pillow says %d", x->file, count, x->frames);

        size_t need = (size_t)x->width * x->height * 4;
        uint8_t *px = (uint8_t *)malloc(need);
        for (int f = 0; f < x->frames && px; f++) {
            memset(px, 0xCD, need);                    /* poison: a frame that writes nothing must not match */
            rc = wubu_gif_frame(data, len, f, px, need, &err);
            CHECK(rc == 0, "%s frame %d: decode failed: %s", x->file, f, wubu_gif_reason(err));
            if (rc != 0) continue;
            uint64_t got = fnv1a64(px, need);
            CHECK(got == x->digest[f],
                  "%s frame %d: digest %016llX, Pillow %016llX", x->file, f,
                  (unsigned long long)got, (unsigned long long)x->digest[f]);
            int delay = wubu_gif_frame_delay(data, len, f, &err);
            CHECK(delay == x->delay_ms[f], "%s frame %d: delay %d ms, Pillow %d ms",
                  x->file, f, delay, x->delay_ms[f]);
        }
        free(px);

        /* A frame past the end must be refused outright -- not wrapped to frame 0 -- with a
         * buffer big enough that OVERFLOW cannot be the reason it is refused. */
        uint8_t *big = (uint8_t *)malloc(need);
        if (big) {
            rc = wubu_gif_frame(data, len, x->frames, big, need, &err);
            CHECK(rc != 0 && err == WUBU_GIF_CORRUPT,
                  "%s: frame %d past the end: rc=%d err=%s (want a CORRUPT refusal)",
                  x->file, x->frames, rc, wubu_gif_reason(err));
            free(big);
        }
        free(data);
    }
    printf("=== wubu_gif_anim: %d passed, %d failed ===\n", checks - failures, failures);
    return failures ? 1 : 0;
}
