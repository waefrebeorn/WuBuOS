/*
 * wubu_glott.c -- the WuBuGLOTT facade: make the voice speak out loud.
 *
 * WuBuGLOTT (src/audio/glott/) renders a physical-model voice into a float
 * buffer. The desktop's device sink (wubu_audio_sink.c) takes int16 PCM.
 * This is the join: render a vowel and put it on the speaker.
 *
 * C11. The voice is created per call (it allocates); a long-lived voice
 * would belong in a session object once the UI has one.
 */
#include "../egojar.h"

#include "wubu_audio.h"
#include "glott/wubuglott_voice.h"
#include "glott/wubuglott_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* A small vowel chart: the articulation targets for the classic set.
 * Values are the engine's own units (tongue_x forward, tongue_y high,
 * jaw open, lip rounding). */
static const struct {
    const char *name;
    wg_artic    artic;
} g_vowels[] = {
    { "a",  { .jaw = 0.85, .velum = 0.0, .larynx = 0.0,
              .tongue_x = 0.15, .tongue_y = 0.35, .lip_round = 0.0 } },
    { "e",  { .jaw = 0.65, .velum = 0.0, .larynx = 0.0,
              .tongue_x = 0.55, .tongue_y = 0.70, .lip_round = 0.05 } },
    { "i",  { .jaw = 0.25, .velum = 0.0, .larynx = 0.0,
              .tongue_x = 0.95, .tongue_y = 0.95, .lip_round = 0.0 } },
    { "o",  { .jaw = 0.60, .velum = 0.0, .larynx = 0.0,
              .tongue_x = 0.25, .tongue_y = 0.45, .lip_round = 0.85 } },
    { "u",  { .jaw = 0.30, .velum = 0.0, .larynx = 0.0,
              .tongue_x = 0.85, .tongue_y = 0.85, .lip_round = 0.90 } },
};

int wubu_glott_vowel_count(void)
{
    return (int)(sizeof(g_vowels) / sizeof(g_vowels[0]));
}

const char *wubu_glott_vowel_name(int index)
{
    if (index < 0 || index >= wubu_glott_vowel_count())
        return NULL;
    return g_vowels[index].name;
}

/*
 * Render `vowel` for `ms` milliseconds and push it through the device sink.
 * Returns 0 on success, -1 if the vowel is unknown or the sink is closed.
 *
 * The sink must be open (wubu_audio_sink_open()) -- the desktop does that at
 * boot. Rendering goes through an int16 staging buffer in blocks, so the
 * memory cost is bounded regardless of the duration.
 */
int wubu_glott_speak(int vowel, int ms, double f0)
{
    if (vowel < 0 || vowel >= wubu_glott_vowel_count())
        return -1;

    wg_voice *v = wg_voice_new(48000.0, WG_BODY_MALE);
    if (!v)
        return -1;

    wg_artic a = g_vowels[vowel].artic;
    wg_source s = WG_SOURCE_DEFAULT;
    s.f0 = f0 > 20.0 ? f0 : 110.0;
    wg_voice_set_artic(v, &a);
    wg_voice_set_source(v, &s);

    const int block = 512;
    float *buf = malloc((size_t)block * sizeof(float));
    if (!buf) {
        wg_voice_free(v);
        return -1;
    }

    int total = (int)((double)ms / 1000.0 * 48000.0);
    int done = 0;
    int rc = 0;

    while (done < total) {
        int n = total - done;
        if (n > block) n = block;
        wg_voice_render(v, buf, n);

        /* float -> int16 for the sink */
        int16_t *pcm = malloc((size_t)n * sizeof(int16_t));
        if (!pcm) { rc = -1; break; }
        for (int i = 0; i < n; i++) {
            double x = (double)buf[i] * 32000.0;
            if (x > 32767.0) x = 32767.0;
            if (x < -32768.0) x = -32768.0;
            pcm[i] = (int16_t)x;
        }
        wubu_audio_sink_int16(pcm, n);
        free(pcm);
        done += n;
    }

    free(buf);
    wg_voice_free(v);
    return rc;
}

/* A short arpeggio of the five vowels -- proves the set without a UI. */
int wubu_glott_scale(double f0)
{
    int n = wubu_glott_vowel_count();
    for (int i = 0; i < n; i++)
        wubu_glott_speak(i, 200, f0);
    return 0;
}
