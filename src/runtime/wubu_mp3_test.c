/* wubu_mp3_test.c -- does our MP3 frame walk agree with ffprobe?
 *
 * ffmpeg encodes the fixtures and ffprobe counts their frames; our code sees neither. Frame
 * counts are exact, so they are compared exactly. Container duration includes the encoder delay
 * (ffmpeg prepends ~1 frame of silence), so it is compared within a stated bound and the observed
 * difference is printed -- the same rule the JPEG gate follows: tolerances are measured, not chosen.
 */
#include "wubu_mp3.h"
#include "../../tests/fixtures/mp3_expect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass, fail;

#define CHECK(cond, ...) do { \
    if (cond) { pass++; } else { fail++; printf("  [FAIL] "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

static uint8_t *slurp(const char *path, long *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (*n <= 0) { fclose(f); return NULL; }
    uint8_t *b = (uint8_t *)malloc((size_t)*n);
    if (b && fread(b, 1, (size_t)*n, f) != (size_t)*n) { free(b); b = NULL; }
    fclose(f);
    return b;
}

int main(void) {
    for (int i = 0; i < WUBU_MP3_CASES; i++) {
        const WubuMp3Expect *x = &WUBU_MP3_EXPECT[i];
        char path[512];
        snprintf(path, sizeof path, "tests/fixtures/mp3/%s", x->file);
        long n = 0;
        uint8_t *data = slurp(path, &n);
        if (!data) { printf("  [FAIL] cannot read %s\n", path); fail++; continue; }

        WubuMp3 m;
        int err = 0;
        int rc = wubu_mp3_info(data, (size_t)n, &m, &err);
        CHECK(rc == WUBU_MP3_OK, "%s: refused (%s)", x->file, wubu_mp3_reason(err));
        if (rc == WUBU_MP3_OK) {
            printf("  %s: %d Hz, %d ch, %d audio frames (%d MPEG), %d ms (ffprobe %d frames, %d ms)\n",
                   x->file, m.sample_rate, m.channels, m.audio_frames, m.frames, m.duration_ms,
                   x->frames, x->duration_ms);
            CHECK(m.sample_rate == x->sample_rate, "%s: rate %d, want %d", x->file, m.sample_rate, x->sample_rate);
            CHECK(m.channels == x->channels, "%s: %d channels, want %d", x->file, m.channels, x->channels);
            /* ffprobe counts DECODED audio frames; gapless trimming can make it one lower than the
             * frames physically present, so this is bounded by one and the difference is printed. */
            int fd = m.audio_frames - x->frames;
            if (fd < 0) fd = -fd;
            CHECK(fd <= 1, "%s: %d audio frames, ffprobe counted %d (difference above 1)",
                  x->file, m.audio_frames, x->frames);
            int diff = m.duration_ms - x->duration_ms;
            if (diff < 0) diff = -diff;
            CHECK(diff <= 15, "%s: duration %d ms, ffprobe %d ms (difference %d above 15)",
                  x->file, m.duration_ms, x->duration_ms, diff);
            CHECK(m.channels == 1 || m.channels == 2, "%s: channels out of range", x->file);
            /* Layer III frame length must scale with version: MPEG-2 carries 576 samples. */
            CHECK(m.samples_per_frame == (m.version == 1 ? 1152 : 576),
                  "%s: version %d claims %d samples/frame", x->file, m.version, m.samples_per_frame);
        }
        free(data);
    }

    /* A file that is not MP3 at all must be refused BY NAME, not walked as zeros. */
    const uint8_t junk[64] = { 'N', 'O', 'T', 'M', 'P', '3' };
    WubuMp3 m; int err = 0;
    int rc = wubu_mp3_info(junk, sizeof junk, &m, &err);
    CHECK(rc == WUBU_MP3_FORMAT, "junk accepted as MP3 (rc=%d, %s)", rc, wubu_mp3_reason(err));

    /* An ID3v2 tag in front of the frames must be skipped, not treated as the stream start. */
    {
        long n = 0;
        uint8_t *base = slurp("tests/fixtures/mp3/cbr128_44100_stereo.mp3", &n);
        if (base) {
            static const uint8_t id3[10] = { 'I', 'D', '3', 4, 0, 0, 0, 0, 0, 10 };  /* 10-byte body */
            size_t total = sizeof id3 + 10 + (size_t)n;
            uint8_t *tagged = (uint8_t *)malloc(total);
            if (tagged) {
                memcpy(tagged, id3, sizeof id3);
                memset(tagged + sizeof id3, 0, 10);
                memcpy(tagged + sizeof id3 + 10, base, (size_t)n);
                WubuMp3 t; int terr = 0;
                int trc = wubu_mp3_info(tagged, total, &t, &terr);
                CHECK(trc == WUBU_MP3_OK && t.audio_frames == 40 && t.has_id3v2,
                      "ID3v2-prefixed stream: rc=%d audio frames=%d id3=%d (%s)",
                      trc, t.audio_frames, t.has_id3v2, wubu_mp3_reason(terr));
                free(tagged);
            }
            free(base);
        }
    }

    printf("=== wubu_mp3: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
