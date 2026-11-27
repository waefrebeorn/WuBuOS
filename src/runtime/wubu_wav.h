/* wubu_wav.h -- RIFF/WAVE decode to float, and PCM16 encode.
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2017). WAV is what a sound recorder,
 * a notification pack and an audio editor's scratch file arrive as; decoding it is the
 * difference between "we have audio" and "we have an audio icon".
 *
 * Samples are delivered as interleaved floats in [-1, 1], because every consumer above
 * this (mixer, resampler, visualiser) wants that, and because 8/16/24/32-bit integer
 * and 32/64-bit float inputs then share one interface. Compressed WAV variants (ADPCM,
 * mu-law, A-law) are refused by name rather than silently mis-decoded.
 */
#ifndef WUBU_WAV_H
#define WUBU_WAV_H

#include <stddef.h>
#include <stdint.h>

enum {
    WUBU_WAV_OK          =  0,
    WUBU_WAV_ARGS        = -1,
    WUBU_WAV_FORMAT      = -2,   /* not RIFF/WAVE */
    WUBU_WAV_TRUNCATED   = -3,   /* data chunk shorter than its declared frames */
    WUBU_WAV_UNSUPPORTED = -4,   /* ADPCM / mu-law / an unknown codec */
    WUBU_WAV_OVERFLOW    = -5    /* output buffer too small */
};

typedef struct {
    int channels;
    int sample_rate;
    int bits;            /* 8, 16, 24, 32 or 64 */
    int is_float;
    int frames;          /* per-channel sample count */
} WubuWav;

int wubu_wav_info(const void *data, size_t len, WubuWav *info, int *err);

/* Decode to interleaved float. `out_cap` is in SAMPLES (frames * channels). */
int wubu_wav_decode(const void *data, size_t len, float *out, size_t out_cap,
                    WubuWav *info, int *err);

/* Encode interleaved float as a canonical 16-bit PCM WAV. Returns bytes written or -1. */
long wubu_wav_encode_pcm16(const float *samples, size_t frames, int channels, int sample_rate,
                           uint8_t *out, size_t out_cap, int *err);

const char *wubu_wav_reason(int err);

#endif
