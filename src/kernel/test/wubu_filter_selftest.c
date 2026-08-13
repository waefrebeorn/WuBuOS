/*
 * wubu_filter_selftest.c -- verifies kernel-owned DSP filter routing.
 */
#include "wubu_filter.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_filter_selftest ===\n\n");

    wubu_hw_detect();
    wubu_filter_probe();

    printf("  filter=%d biquad=%d eq=%d pw=%d alsa=%d\n",
           wubu_filter_present(), wubu_filter_biquad_present(),
           wubu_filter_eq(), wubu_filter_pw(), wubu_filter_alsa());

    /* Filter type routing. */
    CHECK(strcmp(wubu_filter_type_for("lpf"), "lowpass") == 0,
          "lpf -> lowpass");
    CHECK(strcmp(wubu_filter_type_for("hpf"), "highpass") == 0,
          "hpf -> highpass");
    CHECK(strcmp(wubu_filter_type_for("bpf"), "bandpass") == 0,
          "bpf -> bandpass");
    CHECK(strcmp(wubu_filter_type_for("notch"), "notch") == 0,
          "notch -> notch");
    CHECK(strcmp(wubu_filter_type_for("peak"), "peaking") == 0,
          "peak -> peaking");
    CHECK(strcmp(wubu_filter_type_for("lowshelf"), "lowshelf") == 0,
          "lowshelf -> lowshelf");
    CHECK(strcmp(wubu_filter_type_for("highshelf"), "highshelf") == 0,
          "highshelf -> highshelf");
    CHECK(strcmp(wubu_filter_type_for("zzz"), "biquad") == 0,
          "zzz -> biquad fallback");

    /* Biquad computation (Audio EQ Cookbook). */
    double b0, b1, b2, a1, a2;
    /* LPF at 1kHz, fs=48k, Q=0.707: b0=b2, a1=-2cos(w0)/(1+alpha) */
    wubu_filter_biquad(1000, 48000, 0.707, 0, "lowpass",
                       &b0, &b1, &b2, &a1, &a2);
    CHECK(fabs(b0 - b2) < 1e-9, "biquad LPF b0 == b2 (symmetry)");
    CHECK(fabs(b0 - b1/2.0) < 1e-9, "biquad LPF b1 == 2*b0");
    CHECK(fabs(a1 - (-2.0*cos(2.0*M_PI*1000/48000)/(1.0+sin(2.0*M_PI*1000/48000)/(2*0.707)))) < 1e-6,
          "biquad LPF a1 matches cookbook");
    CHECK(a2 < 1.0 && a2 > -1.0, "biquad LPF a2 stable");

    /* HPF at 100Hz */
    wubu_filter_biquad(100, 48000, 0.707, 0, "highpass",
                       &b0, &b1, &b2, &a1, &a2);
    CHECK(b0 > 0, "biquad HPF b0 positive");
    CHECK(fabs(b0 - b2) < 1e-9, "biquad HPF b0 == b2");
    CHECK(b1 < 0, "biquad HPF b1 negative");

    char s[256];
    wubu_filter_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "filter summary generated");

    printf("\n=== FILTER TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
