/*
 * test_glott_speak.c -- the WuBuGLOTT product facade gate.
 *
 * Speaks every cardinal vowel through the device sink (forced to the wav
 * backend) and checks that each one produced real audio in the file.
 * Not assert-nothing: it measures the file's RMS and spectral peak.
 *
 * Run with BIGMAC_BACKEND=wav so the output lands in a file:
 *   BIGMAC_BACKEND=wav make test_glott_speak
 */
#include "../../egojar.h"

#include "../../audio/wubu_audio.h"
#include "../../audio/glott/wubuglott_voice.h"
#include "../../audio/glott/wubuglott_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); g_fail++; } } while (0)

static double goertzel(const int16_t *x, int n, double hz, double fs)
{
    double k = 2.0 * cos(2.0 * WG_PI * hz / fs);
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < n; i++) {
        double v = (double)x[i] + k * s1 - s2;
        s2 = s1;
        s1 = v;
    }
    return s1 * s1 + s2 * s2 - k * s1 * s2;
}

/* Read back the wav the sink wrote. */
static int read_wav(const char *path, int16_t **out, int *n_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char hdr[44];
    if (fread(hdr, 1, 44, f) != 44) { fclose(f); return -1; }
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        fclose(f);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f) - 44;
    fseek(f, 44, SEEK_SET);
    int n = (int)(sz / 2);
    int16_t *buf = malloc((size_t)n * sizeof(int16_t));
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 2, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return -1; }
    fclose(f);
    *out = buf;
    *n_out = n;
    return 0;
}

int main(void)
{
    const char *wav = wubu_audio_sink_path();
    if (!wav) wav = "/tmp/wubu_sound.wav";
    remove(wav);

    wubu_audio_sink_open();

    int nv = wubu_glott_vowel_count();
    CHECK(nv == 5, "there are five cardinal vowels");
    printf("  backend: %s\n", wubu_audio_sink_backend() ? wubu_audio_sink_backend() : "none");

    /* Speak each vowel, truncating the file between them. */
    for (int i = 0; i < nv; i++) {
        /* The sink holds the FILE* open, so the file must be closed before
         * it is removed -- deleting it underneath would send the audio into
         * an unlinked inode. Close, remove, reopen. */
        wubu_audio_sink_close();
        remove(wav);
        wubu_audio_sink_open();
        int rc = wubu_glott_speak(i, 300, 110.0);
        CHECK(rc == 0, "the vowel speaks without error");
        wubu_audio_sink_flush();

        int16_t *pcm = NULL;
        int n = 0;
        CHECK(read_wav(wav, &pcm, &n) == 0, "the sink wrote a readable wav");
        if (!pcm) continue;

        double rms = 0.0;
        for (int k = 0; k < n; k++) rms += (double)pcm[k] * (double)pcm[k];
        rms = sqrt(rms / (double)(n ? n : 1));

        /* the fundamental must be present (f0 = 110 Hz) */
        double p110 = goertzel(pcm, n, 110.0, 48000.0);
        double p4k  = goertzel(pcm, n, 4000.0, 48000.0);

        printf("  vowel %s: %d samples, rms=%.0f, f0_bin=%.3g, 4k_bin=%.3g\n",
               wubu_glott_vowel_name(i) ? wubu_glott_vowel_name(i) : "?",
               n, rms, p110 / (p110 + 1.0), p4k / (p110 + 1.0));

        CHECK(n > 4000, "the wav has real length (300 ms at 48 kHz)");
        CHECK(rms > 100.0, "the vowel is audible, not silence");
        CHECK(p110 > p4k, "the spectrum is pitched, not just noise");

        free(pcm);
    }

    /* The vowel index is bounds-checked. */
    CHECK(wubu_glott_speak(-1, 100, 110.0) == -1, "an unknown vowel is refused");
    CHECK(wubu_glott_speak(99, 100, 110.0) == -1, "an out-of-range vowel is refused");
    CHECK(wubu_glott_vowel_name(99) == NULL, "an out-of-range name is NULL");

    wubu_audio_sink_close();
    CHECK(1, "close did not crash");

    if (g_fail == 0) {
        printf("test_glott_speak: all checks passed\n");
        return 0;
    }
    printf("test_glott_speak: %d FAILED\n", g_fail);
    return 1;
}
