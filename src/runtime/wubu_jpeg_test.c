/* wubu_jpeg_test.c -- the gate for SLERM-2024 stage A.
 *
 * The oracle is foreign: tools/make_jpeg_fixtures.py lets Pillow encode each file AND
 * decode it back to raw RGB. A baseline JPEG decoder cannot be bit-exact with libjpeg (the
 * spec permits several IDCTs), so the comparison is a stated tolerance plus a structural
 * check that does not depend on IDCT quality at all: the red square must BE red, and it
 * must be in the right place. That catches a decoder that is smoothly wrong.
 *
 * The tolerance is 2.0 mean / 8 worst because that is what the decoder MEASURES (0.34/2,
 * 0.04/2, 0.01/1); it was 8/90 when chroma was upsampled nearest-neighbour, and a
 * regression to nearest now fails instead of passing inside a loose limit.
 */
#include "wubu_jpeg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass, fail;

static void check(int cond, const char *what) {
    if (cond) { pass++; return; }
    printf("  [FAIL] %s\n", what);
    fail++;
}

static unsigned char *slurp(const char *path, long *n) {
    FILE *f = fopen(path, "rb");
    if (!f) { *n = -1; return NULL; }
    fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f);
    unsigned char *b = malloc((size_t)*n + 1);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); free(b); *n = -1; return NULL; }
    b[*n] = 0;
    fclose(f);
    return b;
}

/* Minimal P6 reader: "P6\n<w> <h>\n<max>\n" then w*h*3 bytes. */
static unsigned char *read_ppm(const char *path, int *w, int *h) {
    long n; unsigned char *b = slurp(path, &n);
    if (!b || n < 15) { free(b); return NULL; }
    int vals[3], got = 0;
    size_t i = 2;
    while (got < 3 && i < (size_t)n) {
        while (i < (size_t)n && (b[i] == ' ' || b[i] == '\n' || b[i] == '\r' || b[i] == '\t')) i++;
        if (b[i] == '#') { while (i < (size_t)n && b[i] != '\n') i++; continue; }
        int v = 0, any = 0;
        while (i < (size_t)n && b[i] >= '0' && b[i] <= '9') { v = v * 10 + (b[i] - '0'); i++; any = 1; }
        if (!any) break;
        vals[got++] = v;
    }
    if (got != 3 || vals[2] != 255) { free(b); return NULL; }
    i++;                                          /* the single whitespace after maxval */
    size_t need = (size_t)vals[0] * (size_t)vals[1] * 3;
    if (i + need > (size_t)n) { free(b); return NULL; }
    unsigned char *px = malloc(need);
    if (px) memcpy(px, b + i, need);
    *w = vals[0]; *h = vals[1];
    free(b);
    return px;
}

/* Progressive and baseline at the same quality quantise to the SAME coefficients, so the two
 * decode paths must agree exactly. A mean-difference tolerance would hide a single wrong
 * refinement bit; this cannot. */
static void case_same_pixels(const char *dir, const char *a, const char *b) {
    char pa[512], pb[512];
    snprintf(pa, sizeof pa, "%s/%s", dir, a);
    snprintf(pb, sizeof pb, "%s/%s", dir, b);
    long na = 0, nb = 0;
    unsigned char *ba = slurp(pa, &na), *bb = slurp(pb, &nb);
    WubuJpegImage ia = {0}, ib = {0};
    int ea = 0, eb = 0;
    int ok = ba && bb
          && wubu_jpeg_decode(ba, (size_t)na, &ia, 0, &ea) == WUBU_JPEG_OK
          && wubu_jpeg_decode(bb, (size_t)nb, &ib, 0, &eb) == WUBU_JPEG_OK;
    if (!ok) { printf("  [FAIL] %s vs %s: could not decode both\n", a, b); fail++; free(ba); free(bb); return; }
    if (ia.w != ib.w || ia.h != ib.h) {
        printf("  [FAIL] %s vs %s: %dx%d against %dx%d\n", a, b, ia.w, ia.h, ib.w, ib.h);
        fail++;
    } else {
        long worst = 0, at = -1;
        for (long i = 0; i < (long)ia.w * ia.h * 3; i++) {
            long d = labs((long)ia.rgb[i] - (long)ib.rgb[i]);
            if (d > worst) { worst = d; at = i; }
        }
        if (worst == 0) { printf("  %s == %s: identical (%dx%d)\n", a, b, ia.w, ia.h); pass++; }
        else {
            printf("  [FAIL] %s vs %s: differs by %ld at byte %ld\n", a, b, worst, at);
            fail++;
        }
    }
    wubu_jpeg_free(&ia); wubu_jpeg_free(&ib); free(ba); free(bb);
}

static void case_colour(const char *dir, const char *name, double mean_limit, int max_limit) {
    char jp[512], pp[512];
    snprintf(jp, sizeof jp, "%s/%s", dir, name);
    snprintf(pp, sizeof pp, "%s/%s.rgb", dir, name);

    long n = 0;
    unsigned char *jpeg = slurp(jp, &n);
    if (!jpeg) { printf("  [FAIL] %s: cannot read fixture\n", name); fail++; return; }

    WubuJpegImage img; int err = 0;
    int rc = wubu_jpeg_decode(jpeg, (size_t)n, &img, 0, &err);
    free(jpeg);
    if (rc != WUBU_JPEG_OK) {
        printf("  [FAIL] %s: refused (%s)\n", name, wubu_jpeg_reason(err));
        fail++;
        return;
    }
    pass++;                                        /* it decoded at all */

    int ow = 0, oh = 0;
    unsigned char *want = read_ppm(pp, &ow, &oh);
    if (!want) { printf("  [FAIL] %s: cannot read the oracle\n", name); fail++; wubu_jpeg_free(&img); return; }

    if (img.w == ow && img.h == oh) pass++;
    else { printf("  [FAIL] %s: size %dx%d, oracle %dx%d\n", name, img.w, img.h, ow, oh); fail++; }

    if (img.w == ow && img.h == oh) {
        long double sum = 0;
        int worst = 0;
        size_t np = (size_t)ow * (size_t)oh * 3;
        for (size_t k = 0; k < np; k++) {
            int d = (int)img.rgb[k] - (int)want[k];
            if (d < 0) d = -d;
            sum += d;
            if (d > worst) worst = d;
        }
        double mean = (double)(sum / (long double)np);
        printf("  %s: %dx%d, mean |diff| %.2f, worst %d\n", name, img.w, img.h, mean, worst);
        if (mean <= mean_limit) pass++;
        else { printf("  [FAIL] %s: mean difference %.2f above %.2f\n", name, mean, mean_limit); fail++; }
        if (worst <= max_limit) pass++;
        else { printf("  [FAIL] %s: worst pixel off by %d, above %d\n", name, worst, max_limit); fail++; }

        /* structural: the red square was drawn at (w/4..w/2, h/4..h/2) */
        if (ow >= 8 && oh >= 8 && strstr(name, "grey") == NULL) {   /* the grey fixture pads with white, not red */
            int cx = ow * 3 / 8, cy = oh * 3 / 8;
            const unsigned char *p = img.rgb + ((size_t)cy * ow + cx) * 3;
            check(p[0] > 180 && p[1] < 80 && p[2] < 80, "the red square is red, in the right place");
        }
    }
    free(want);
    wubu_jpeg_free(&img);
}

static void case_refused(const char *dir, const char *name, int want, const char *why) {
    char jp[512];
    snprintf(jp, sizeof jp, "%s/%s", dir, name);
    long n = 0;
    unsigned char *buf = slurp(jp, &n);
    if (!buf) { printf("  [FAIL] %s: cannot read fixture\n", name); fail++; return; }
    WubuJpegImage img; int err = 0;
    int rc = wubu_jpeg_decode(buf, (size_t)n, &img, 0, &err);
    free(buf);
    if (rc == WUBU_JPEG_OK) {
        printf("  [FAIL] %s: decoded, but %s\n", name, why);
        wubu_jpeg_free(&img);
        fail++;
        return;
    }
    if (err == want && img.rgb == NULL) { pass++; printf("  %s: refused (%s)\n", name, wubu_jpeg_reason(err)); return; }
    printf("  [FAIL] %s: got %s, want %s\n", name, wubu_jpeg_reason(err), wubu_jpeg_reason(want));
    fail++;
}

int main(int argc, char **argv) {
    printf("=== wubu_jpeg (gate SLERM-2024 stage A) ===\n");
    const char *dir = (argc > 1) ? argv[1] : "/tmp/jfx";

    case_colour(dir, "colour_420.jpg", 2.0, 8);
    case_colour(dir, "colour_444.jpg", 2.0, 8);
    case_colour(dir, "grey_400.jpg", 2.0, 8);

    /* Progressive is no longer refused: it must DECODE, to the same tolerance as baseline. */
    case_colour(dir, "prog_420.jpg", 2.0, 8);
    case_colour(dir, "prog_444.jpg", 2.0, 8);
    case_colour(dir, "prog_grey.jpg", 2.0, 8);
    case_colour(dir, "progressive.jpg", 2.0, 8);
    case_same_pixels(dir, "progressive.jpg", "base_q40.jpg");
    case_refused(dir, "truncated.jpg", WUBU_JPEG_SHORT, "the file is truncated");
    case_refused(dir, "notjpeg.jpg", WUBU_JPEG_NOT_JPEG, "it is not a JPEG");

    /* a size cap must refuse rather than allocate */
    {
        long n = 0;
        char jp[512];
        snprintf(jp, sizeof jp, "%s/colour_420.jpg", dir);
        unsigned char *buf = slurp(jp, &n);
        if (buf) {
            WubuJpegImage img; int err = 0;
            int rc = wubu_jpeg_decode(buf, (size_t)n, &img, 100, &err);
            check(rc != WUBU_JPEG_OK && err == WUBU_JPEG_SIZE && img.rgb == NULL, "a size cap is refused, not allocated");
            free(buf);
        }
    }

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
