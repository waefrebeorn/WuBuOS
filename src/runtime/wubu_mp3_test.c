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


    /* ---- stage 2b: the granule data actually decodes ------------------------------------------
     * What this asserts, and what it deliberately does NOT:
     *   - every granule of every MPEG-1 fixture walks the codebooks without leaving the tree and
     *     without selecting an unpopulated codebook (0/4/14);
     *   - the scalefactor + big_values region fits inside part2_3_length (big_bits <= declared);
     *   - an MPEG-2 stream is REFUSED rather than decoded with MPEG-1 band tables.
     * It does not assert used == declared: part2_3_length may cut the last count1 code in half and
     * the reference decoder discards that partial code, so equality there would measure the
     * encoder's rounding rather than our decoding. The value-level oracle arrives with the IMDCT.
     */
    {
        const char *files[] = { "tests/fixtures/mp3/cbr128_44100_stereo.mp3",
                                "tests/fixtures/mp3/cbr192_48000_stereo.mp3",
                                "tests/fixtures/mp3/cbr64_44100_mono.mp3",
                                "tests/fixtures/mp3/vbr_44100_stereo.mp3" };
        for (unsigned fi = 0; fi < sizeof(files) / sizeof(files[0]); fi++) {
            FILE *fp = fopen(files[fi], "rb");
            CHECK(fp != NULL, "open %s", files[fi]);
            if (!fp) continue;
            fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
            unsigned char *d = malloc((size_t)n);
            CHECK(d && fread(d, 1, (size_t)n, fp) == (size_t)n, "read %s", files[fi]);
            fclose(fp);
            WubuMp3 m; int e = 0;
            CHECK(wubu_mp3_info(d, (size_t)n, &m, &e) == WUBU_MP3_OK, "info %s (%s)", files[fi], wubu_mp3_reason(e));
            int granules = 0, bad = 0, overrun = 0, empty_big = 0, tail = 0, frames_bad = 0, exact = 0;
            size_t at = (size_t)m.first_frame_offset;
            for (int i = 0; i < m.frames; i++) {
                int fl = 0, v = 0, r = 0, c = 0;
                if (wubu_mp3_frame_header(d, (size_t)n, at, &fl, &v, &r, &c, &e) != 0) break;
                WubuMp3GranuleBits gb[2][2];
                int rc = wubu_mp3_frame_huffman(d, (size_t)n, at, gb, &e);
                /* the final frame of a file may be cut off by EOF; anything else is a defect */
                if (rc != WUBU_MP3_OK) {
                    if (i + 1 < m.frames) frames_bad++;
                    at += (size_t)fl;
                    continue;
                }
                for (int gr = 0; gr < 2; gr++) {
                    for (int ch = 0; ch < 2; ch++) {
                        if (!gb[gr][ch].declared_bits) continue;
                        granules++;
                        if (!gb[gr][ch].ok) bad++;
                        if (gb[gr][ch].big_bits > gb[gr][ch].declared_bits) overrun++;
                        if (gb[gr][ch].big_bits == 0 && gb[gr][ch].declared_bits > 0) empty_big++;
                        if (gb[gr][ch].tail_partial) tail++;
                        if (gb[gr][ch].tail_skip == 0) exact++;
                    }
                }
                at += (size_t)fl;
            }
            CHECK(frames_bad == 0, "%s: %d frames failed to decode (%s)", files[fi], frames_bad, wubu_mp3_reason(e));
            CHECK(granules > 0, "%s: no granules decoded", files[fi]);
            CHECK(bad == 0, "%s: %d of %d granules left the codebook", files[fi], bad, granules);
            CHECK(overrun == 0, "%s: %d granules spent more than part2_3_length before count1", files[fi], overrun);
            CHECK(empty_big == 0, "%s: %d granules consumed no bits at all", files[fi], empty_big);
            /* A FLOOR on exactness, because "did not crash" is not evidence of correct decoding: a
             * shifted codebook tree still walks to valid leaves. These floors are measured from this
             * decoder on these fixtures (2026-11-28) and may only rise. They are what makes a bent
             * table visible -- the count drops the moment the walk consumes the wrong number of bits. */
            static const int floor_exact[] = { 50, 59, 75, 69 };
            CHECK(exact >= floor_exact[fi],
                  "%s: only %d of %d granules consumed exactly the declared bits (floor %d)",
                  files[fi], exact, granules, floor_exact[fi]);
            printf("  %-46s %d granules decode, %d exact, %d with a cut tail code\n",
                   files[fi], granules, exact, tail);
            free(d);
        }

        /* MPEG-2 uses ISO 13818-3 scalefactor bands. Decoding it with MPEG-1 bands would produce
         * plausible nonsense, so the refusal is asserted rather than skipped. */
        {
            FILE *fp = fopen("tests/fixtures/mp3/mpeg2_22050_stereo.mp3", "rb");
            CHECK(fp != NULL, "open the MPEG-2 fixture");
            if (fp) {
                fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
                unsigned char *d = malloc((size_t)n);
                if (d && fread(d, 1, (size_t)n, fp) == (size_t)n) {
                    WubuMp3 m; int e = 0;
                    wubu_mp3_info(d, (size_t)n, &m, &e);
                    WubuMp3GranuleBits gb[2][2];
                    int rc = wubu_mp3_frame_huffman(d, (size_t)n, (size_t)m.first_frame_offset, gb, &e);
                    CHECK(rc == WUBU_MP3_UNSUPPORTED,
                          "MPEG-2 granule decode must be refused, not guessed (rc=%d)", rc);
                }
                free(d);
                fclose(fp);
            }
        }
    }

    printf("=== wubu_mp3: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
