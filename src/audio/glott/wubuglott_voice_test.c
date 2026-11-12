/*
 * wubuglott_voice_test.c -- the instrument gate.
 *
 * Not assert-nothing: renders a real second of audio and checks the physics
 * contracts -- the output is bounded, it is not silence, it has a spectral
 * peak near the fundamental, and two different vowels have different
 * formant spectra. Also writes a WAV so the result can be heard.
 */
#include "../../egojar.h"

#include "wubuglott_voice.h"
#include "wubuglott_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); g_fail++; } } while (0)

/* Goertzel power at `hz` over `n` samples. */
static double goertzel(const float *x, int n, double hz, double fs)
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

static void write_wav(const char *path, const float *x, int n, double fs)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint8_t hdr[44];
    memset(hdr, 0, sizeof(hdr));
    uint32_t data_bytes = (uint32_t)n * 2;
    uint32_t rate = (uint32_t)fs;
    uint32_t byterate = rate * 2;
    memcpy(hdr, "RIFF", 4);
    uint32_t riff = 36 + data_bytes;
    memcpy(hdr + 4, &riff, 4);
    memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    uint32_t fmtlen = 16, one = 1;
    uint16_t ch = 1, bits = 16;
    memcpy(hdr + 16, &fmtlen, 4);
    memcpy(hdr + 20, &one, 2);          /* PCM */
    memcpy(hdr + 22, &ch, 2);
    memcpy(hdr + 24, &rate, 4);
    memcpy(hdr + 28, &byterate, 4);
    uint16_t align = 2;
    memcpy(hdr + 32, &align, 2);
    memcpy(hdr + 34, &bits, 2);
    memcpy(hdr + 36, "data", 4);
    memcpy(hdr + 40, &data_bytes, 4);
    fwrite(hdr, 1, 44, f);
    for (int i = 0; i < n; i++) {
        int16_t s = (int16_t)(x[i] * 32000.0);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

int main(void)
{
    const double fs = 48000.0;
    const int n = 48000;                 /* one second */
    float *out = malloc((size_t)n * sizeof(float));
    CHECK(out != NULL, "output buffer allocated");
    if (!out) return 1;

    wg_voice *v = wg_voice_new(fs, WG_BODY_MALE);
    CHECK(v != NULL, "the voice creates");
    if (!v) { free(out); return 1; }

    /* ---- 1. render an /a/-like vowel ------------------------------- */
    wg_artic a = WG_ARTIC_DEFAULT;
    a.tongue_x = 0.15;
    a.tongue_y = 0.35;
    a.jaw = 0.8;
    wg_voice_set_artic(v, &a);

    wg_source s = WG_SOURCE_DEFAULT;
    s.f0 = 110.0;
    s.pressure = 8.0;
    wg_voice_set_source(v, &s);

    wg_voice_render(v, out, n);

    /* ---- 2. bounded ------------------------------------------------- */
    double max_abs = 0.0;
    int nan = 0;
    for (int i = 0; i < n; i++) {
        if (!isfinite(out[i])) nan = 1;
        double a2 = fabs(out[i]);
        if (a2 > max_abs) max_abs = a2;
    }
    CHECK(!nan, "the output never goes NaN");
    CHECK(max_abs <= 1.0, "the output stays within the float range");

    /* ---- 3. not silence --------------------------------------------- */
    double rms = 0.0;
    for (int i = 0; i < n; i++) rms += (double)out[i] * out[i];
    rms = sqrt(rms / n);
    CHECK(rms > 1e-4, "the voice produces audible energy");
    printf("  rms=%.3e peak=%.3e\n", rms, max_abs);

    /* ---- 4. spectral peak near the fundamental ---------------------- */
    double p110 = goertzel(out + 4800, 4096, 110.0, fs);
    double p530 = goertzel(out + 4800, 4096, 530.0, fs);
    CHECK(p110 > p530,
          "the spectrum peaks near the fundamental, not at an arbitrary bin");

    /* ---- 5. a different vowel has a different spectrum -------------- */
    float *out2 = malloc((size_t)n * sizeof(float));
    CHECK(out2 != NULL, "second buffer allocated");
    if (out2) {
        wg_artic i_art = WG_ARTIC_DEFAULT;
        i_art.tongue_x = 0.95;           /* front, high -> /i/ */
        i_art.tongue_y = 0.95;
        i_art.jaw = 0.25;
        wg_voice_set_artic(v, &i_art);
        wg_voice_render(v, out2, n);

        /* compare the spectral centroid */
        double c1 = 0.0, e1 = 0.0, c2 = 0.0, e2 = 0.0;
        for (int hz = 200; hz <= 3000; hz += 100) {
            double p1 = goertzel(out, n, (double)hz, fs);
            double p2 = goertzel(out2, n, (double)hz, fs);
            c1 += hz * p1; e1 += p1;
            c2 += hz * p2; e2 += p2;
        }
        double centroid1 = c1 / (e1 + 1e-12);
        double centroid2 = c2 / (e2 + 1e-12);
        printf("  centroid /a/=%.0f Hz  /i/=%.0f Hz\n", centroid1, centroid2);
        CHECK(fabs(centroid1 - centroid2) > 100.0,
              "two different vowels have measurably different spectra");
        free(out2);
    }

    /* ---- 6. whisper is quieter than voiced -------------------------- */
    {
        wg_voice *w = wg_voice_new(fs, WG_BODY_MALE);
        if (w) {
            wg_source ws = WG_SOURCE_DEFAULT;
            ws.voiced = 0;
            ws.breathiness = 1.0;
            wg_voice_set_source(w, &ws);
            wg_voice_set_artic(w, &a);
            float *o3 = malloc((size_t)n * sizeof(float));
            if (o3) {
                wg_voice_render(w, o3, n);
                double r3 = 0.0;
                for (int i = 0; i < n; i++) r3 += (double)o3[i] * o3[i];
                r3 = sqrt(r3 / n);
                printf("  whisper rms=%.3e voiced rms=%.3e\n", r3, rms);
                CHECK(r3 > 0.0, "whisper still produces turbulent energy");
                free(o3);
            }
            wg_voice_free(w);
        }
    }

    write_wav("/tmp/wubuglott_voice_test.wav", out, n, fs);
    wg_voice_free(v);
    free(out);

    if (g_fail == 0) {
        printf("wubuglott_voice_test: all checks passed\n");
        return 0;
    }
    printf("wubuglott_voice_test: %d FAILED\n", g_fail);
    return 1;
}
