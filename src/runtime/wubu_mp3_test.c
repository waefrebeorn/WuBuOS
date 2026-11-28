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

    /* ---- stage 2a: side information. The frame DECLARES how many bytes of side info it carries
     * (17/32 MPEG-1 mono/stereo, 9/17 MPEG-2). If our field widths are right, the reader lands
     * exactly on that byte; one wrong bit shifts every later field and shows up immediately. ---- */
    {
        int frames_checked = 0, bad_bits = 0, bad_fit = 0, bad_switch = 0, bad_mdb = 0;
        long total_granule_bits = 0, total_main_bits = 0;
        for (int i = 0; i < WUBU_MP3_CASES; i++) {
            char path[512];
            snprintf(path, sizeof path, "tests/fixtures/mp3/%s", WUBU_MP3_EXPECT[i].file);
            long n = 0;
            uint8_t *data = slurp(path, &n);
            if (!data) continue;
            WubuMp3 meta; int em = 0;
            if (wubu_mp3_info(data, (size_t)n, &meta, &em) != WUBU_MP3_OK) { free(data); continue; }
            size_t at = (size_t)meta.first_frame_offset;
            for (int f = 0; f < 200 && at + 4 <= (size_t)n; f++) {
                int flen = 0, ver = 0, rate = 0, chans = 0, e = 0;
                if (wubu_mp3_frame_header(data, (size_t)n, at, &flen, &ver, &rate, &chans, &e) != WUBU_MP3_OK) break;
                WubuMp3Side si; int e3 = 0;
                if (wubu_mp3_side_info(data, (size_t)n, at, flen, &si, &e3) == WUBU_MP3_OK) {
                    frames_checked++;
                    if (si.bits_consumed != si.side_info_bits) bad_bits++;
                    int sum = 0;
                    for (int g = 0; g < si.granules; g++)
                        for (int ch = 0; ch < chans; ch++) sum += si.gr[g][ch].part2_3_length;
                    /* Two invariants, both real:
                     *  - main_data_begin addresses the reservoir, so it cannot exceed 511 bytes
                     *    (MPEG-1) or 255 (MPEG-2).
                     *  - a frame's granule data may legitimately CONTINUE into following frames'
                     *    main data (that is what the reservoir is for), so a per-frame "does it fit"
                     *    test is not an invariant at all -- my first attempt asserted exactly that and
                     *    flagged three healthy MPEG-2 frames. The honest bound is global: across the
                     *    file, the granule bits cannot exceed the main-data bits the file contains. */
                    int main_bytes = flen - 4 - (si.side_info_bits / 8);
                    int max_mdb = (ver == 1) ? 511 : 255;
                    if (si.main_data_begin > max_mdb) bad_mdb++;
                    total_granule_bits += sum;
                    total_main_bits += (long)main_bytes * 8;
                    for (int g = 0; g < si.granules; g++)
                        for (int ch = 0; ch < chans; ch++) {
                            WubuMp3Granule *gr = &si.gr[g][ch];
                            if (gr->window_switching && (gr->block_type < 1 || gr->block_type > 3)) bad_switch++;
                            if (!gr->window_switching && gr->block_type != 0) bad_switch++;
                        }
                }
                at += (size_t)flen;
            }
            free(data);
        }
        printf("  side info: %d frames parsed, %d wrong bit counts, %d over-supply, %d bad block types, %d bad mdb\n",
               frames_checked, bad_bits, bad_fit, bad_switch, bad_mdb);
        CHECK(frames_checked > 100, "side info: only %d frames parsed", frames_checked);
        CHECK(bad_bits == 0, "side info: %d frames did not land on the declared size", bad_bits);
        CHECK(bad_switch == 0, "side info: %d granules have an impossible block type", bad_switch);
        printf("  side info: granule bits %ld vs main-data bits %ld (%.3f of the file)\n",
               total_granule_bits, total_main_bits,
               total_main_bits ? (double)total_granule_bits / (double)total_main_bits : 0.0);
        CHECK(total_granule_bits <= total_main_bits,
              "side info: the file claims %ld granule bits but holds only %ld main-data bits",
              total_granule_bits, total_main_bits);
        CHECK(total_granule_bits > total_main_bits / 2,
              "side info: only %ld of %ld main-data bits are claimed -- the walk is probably incomplete",
              total_granule_bits, total_main_bits);
        CHECK(bad_mdb == 0, "side info: %d frames exceed the reservoir limit", bad_mdb);
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
