/* wubu_wav.c -- see wubu_wav.h. Chunk walker, sample converters, PCM16 writer. */
#include "wubu_wav.h"
#include <string.h>

static unsigned rd32(const uint8_t *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
static unsigned rd16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static void wr32(uint8_t *p, unsigned v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void wr16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t data_off;
    size_t data_len;
    int channels, rate, bits, is_float;
    int have_fmt, have_data;
} Scan;

static int walk(const uint8_t *d, size_t len, Scan *s, int *err) {
    if (len < 12 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) {
        *err = WUBU_WAV_FORMAT;
        return -1;
    }
    memset(s, 0, sizeof *s);
    s->data = d; s->len = len;
    size_t off = 12;
    while (off + 8 <= len) {
        const uint8_t *id = d + off;
        unsigned size = rd32(d + off + 4);
        size_t body = off + 8;
        if (body + size > len) size = (unsigned)(len - body);   /* tolerate a short tail */
        if (!memcmp(id, "fmt ", 4)) {
            if (size < 16) { *err = WUBU_WAV_TRUNCATED; return -1; }
            unsigned format = rd16(d + body);
            s->channels = (int)rd16(d + body + 2);
            s->rate = (int)rd32(d + body + 4);
            s->bits = (int)rd16(d + body + 14);
            if (format == 0xFFFE && size >= 40) format = rd16(d + body + 24);   /* extensible */
            if (format == 1) s->is_float = 0;
            else if (format == 3) s->is_float = 1;
            else { *err = WUBU_WAV_UNSUPPORTED; return -1; }
            if (s->channels <= 0 || s->rate <= 0) { *err = WUBU_WAV_FORMAT; return -1; }
            if (!((s->is_float && (s->bits == 32 || s->bits == 64)) ||
                  (!s->is_float && (s->bits == 8 || s->bits == 16 || s->bits == 24 || s->bits == 32)))) {
                *err = WUBU_WAV_UNSUPPORTED;
                return -1;
            }
            s->have_fmt = 1;
        } else if (!memcmp(id, "data", 4)) {
            s->data_off = body;
            s->data_len = size;
            s->have_data = 1;
        }
        off = body + size + (size & 1);          /* chunks are word-aligned */
    }
    if (!s->have_fmt || !s->have_data) { *err = WUBU_WAV_FORMAT; return -1; }
    return 0;
}

int wubu_wav_info(const void *data, size_t len, WubuWav *info, int *err) {
    if (err) *err = WUBU_WAV_OK;
    if (!data || !info) { if (err) *err = WUBU_WAV_ARGS; return -1; }
    Scan s; int e = 0;
    if (walk((const uint8_t *)data, len, &s, &e)) { if (err) *err = e; return -1; }
    unsigned frame_bytes = (unsigned)(s.channels * (s.bits / 8));
    if (frame_bytes == 0) { if (err) *err = WUBU_WAV_FORMAT; return -1; }
    info->channels = s.channels;
    info->sample_rate = s.rate;
    info->bits = s.bits;
    info->is_float = s.is_float;
    info->frames = (int)(s.data_len / frame_bytes);
    return 0;
}

static float pcm8(uint8_t v)  { return ((int)v - 128) / 128.0f; }
static float pcm16(const uint8_t *p) { return (float)(int16_t)(rd16(p)) / 32768.0f; }
static float pcm24(const uint8_t *p) {
    int32_t v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8;
    return (float)v / 8388608.0f;
}
static float pcm32(const uint8_t *p) { return (float)((double)(int32_t)rd32(p) / 2147483648.0); }
static float f32(const uint8_t *p) {
    uint32_t u = rd32(p);
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}
static float f64(const uint8_t *p) {
    uint64_t u = (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
    double d;
    memcpy(&d, &u, sizeof d);
    return (float)d;
}

int wubu_wav_decode(const void *data, size_t len, float *out, size_t out_cap,
                    WubuWav *info, int *err) {
    if (err) *err = WUBU_WAV_OK;
    if (!data || !out || !info) { if (err) *err = WUBU_WAV_ARGS; return -1; }
    Scan s; int e = 0;
    if (walk((const uint8_t *)data, len, &s, &e)) { if (err) *err = e; return -1; }
    int bytes = s.bits / 8;
    unsigned frame_bytes = (unsigned)(s.channels * bytes);
    size_t frames = s.data_len / frame_bytes;
    size_t samples = frames * (size_t)s.channels;
    if (out_cap < samples) { if (err) *err = WUBU_WAV_OVERFLOW; return -1; }
    const uint8_t *p = s.data + s.data_off;
    for (size_t i = 0; i < samples; i++, p += bytes) {
        if (s.is_float) out[i] = (s.bits == 32) ? f32(p) : f64(p);
        else if (s.bits == 8)  out[i] = pcm8(*p);
        else if (s.bits == 16) out[i] = pcm16(p);
        else if (s.bits == 24) out[i] = pcm24(p);
        else                   out[i] = pcm32(p);
    }
    info->channels = s.channels;
    info->sample_rate = s.rate;
    info->bits = s.bits;
    info->is_float = s.is_float;
    info->frames = (int)frames;
    return 0;
}

long wubu_wav_encode_pcm16(const float *samples, size_t frames, int channels, int sample_rate,
                           uint8_t *out, size_t out_cap, int *err) {
    if (err) *err = WUBU_WAV_OK;
    if (!samples || !out || channels <= 0 || sample_rate <= 0) { if (err) *err = WUBU_WAV_ARGS; return -1; }
    size_t samples_n = frames * (size_t)channels;
    size_t data_bytes = samples_n * 2;
    size_t total = 44 + data_bytes;
    if (out_cap < total) { if (err) *err = WUBU_WAV_OVERFLOW; return -1; }
    memcpy(out, "RIFF", 4); wr32(out + 4, (unsigned)(total - 8)); memcpy(out + 8, "WAVE", 4);
    memcpy(out + 12, "fmt ", 4); wr32(out + 16, 16);
    wr16(out + 20, 1);                       /* PCM */
    wr16(out + 22, (unsigned)channels);
    wr32(out + 24, (unsigned)sample_rate);
    wr32(out + 28, (unsigned)(sample_rate * channels * 2));
    wr16(out + 32, (unsigned)(channels * 2));
    wr16(out + 34, 16);
    memcpy(out + 36, "data", 4); wr32(out + 40, (unsigned)data_bytes);
    uint8_t *p = out + 44;
    for (size_t i = 0; i < samples_n; i++, p += 2) {
        float v = samples[i];
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        int s = (int)(v * 32767.0f + (v >= 0 ? 0.5f : -0.5f));
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        wr16(p, (unsigned)(uint16_t)(int16_t)s);
    }
    return (long)total;
}

const char *wubu_wav_reason(int err) {
    switch (err) {
        case WUBU_WAV_OK:          return "ok";
        case WUBU_WAV_ARGS:        return "bad arguments";
        case WUBU_WAV_FORMAT:      return "not a RIFF/WAVE file";
        case WUBU_WAV_TRUNCATED:   return "that WAV is truncated";
        case WUBU_WAV_UNSUPPORTED: return "that WAV uses a codec we do not decode";
        case WUBU_WAV_OVERFLOW:    return "output buffer too small";
        default:                   return "invalid";
    }
}
