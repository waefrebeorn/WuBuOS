/*
 * wubu_ladder.c -- virtual-analog Moog-ladder lowpass (WT-B). C11.
 * Four one-pole stages + resonance feedback + tanh saturation, in the
 * style of the classic transistor-ladder filters. The feedback path
 * creates the self-oscillation at high resonance.
 */
#include "wubu_ladder.h"
#include <math.h>
#include <string.h>

static float sat(float x)
{
    return tanhf(x);   /* the soft-saturation (the ladder's drive) */
}

static float one_pole(float z, float in, float g)
{
    /* one-pole lowpass: y = g*in + (1-g)*z */
    float y = g * in + (1.0f - g) * z;
    return y;
}

int wubu_ladder_init(wubu_ladder_t *f, float sample_rate,
                     float cutoff, float res)
{
    if (!f || sample_rate <= 0) return -1;
    memset(f, 0, sizeof(*f));
    f->sample_rate = sample_rate;
    f->cutoff = cutoff < 0 ? 0 : (cutoff > 1 ? 1 : cutoff);
    f->res = res < 0 ? 0 : (res > 1 ? 1 : res);
    f->drive = 1.0f + 2.0f * f->res;   /* resonance raises the drive */
    return 0;
}

static float ladder_gain(const wubu_ladder_t *f)
{
    /* the one-pole coefficient from the normalized cutoff.
     * g = 1 - exp(-2*pi*fc/fs) is ROCK-STABLE (g < 1 always; the DC
     * gain = 1 exactly); 2*sin() grows toward 2 and the 4-stage
     * cascade of near-integrators blew up to peak ~825 (DA-caught). */
    float fc = f->cutoff;
    /* map 0..1 onto ~40Hz..~0.45*fs (exponential feel) */
    float hz = 40.0f * powf(f->sample_rate / 4000.0f, fc) * powf(100.0f, fc);
    if (hz > f->sample_rate * 0.45f) hz = f->sample_rate * 0.45f;
    float g = 1.0f - expf(-6.283185307179586f * hz / f->sample_rate);
    return g;
}

float wubu_ladder_process(wubu_ladder_t *f, float in)
{
    if (!f) return 0;
    float g = ladder_gain(f);
    float fb = 4.0f * f->res * f->z[3];
    float u = sat(f->drive * in - fb);
    float y1 = one_pole(f->z[0], u, g);
    float y2 = one_pole(f->z[1], y1, g);
    float y3 = one_pole(f->z[2], y2, g);
    float y4 = one_pole(f->z[3], y3, g);
    f->z[0] = y1; f->z[1] = y2; f->z[2] = y3; f->z[3] = y4;
    return y4;
}

int wubu_ladder_process_buf(wubu_ladder_t *f, float *buf, int n)
{
    if (!f || !buf || n <= 0) return 0;
    for (int i = 0; i < n; i++) buf[i] = wubu_ladder_process(f, buf[i]);
    return n;
}

float wubu_ladder_gain_at(const wubu_ladder_t *f, float freq_hz)
{
    if (!f || freq_hz <= 0) return 0;
    /* a steady-state magnitude estimate by sweeping a sine through the
     * filter and measuring the output/input RMS (deterministic). */
    wubu_ladder_t tmp = *f;
    /* process 2048 samples of a unit sine at freq_hz, compare RMS */
    int n = 4096;
    float phase = 0, step = 6.283185307179586f * freq_hz / f->sample_rate;
    double in_rms = 0, out_rms = 0;
    for (int i = 0; i < n; i++) {
        float s = sinf(phase);
        float o = wubu_ladder_process(&tmp, s);
        in_rms += (double)s * s;
        out_rms += (double)o * o;
        phase += step;
    }
    if (in_rms <= 0) return 0;
    return (float)sqrt(out_rms / in_rms);
}
