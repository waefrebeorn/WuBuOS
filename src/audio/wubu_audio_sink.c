/*
 * wubu_audio_sink.c -- the device sink for the sound system.
 *
 * Separate from wubu_audio_backend.c on purpose: the backend pulls the
 * ENGINE (a thread calling wubu_audio_process), while the sink is what the
 * SOUND system calls (wubu_sound_play's sink callback). A lean test that
 * links cp_sound.c needs this file and bigmac -- not the whole engine.
 *
 * wubu_sound_play() takes a sink and every call site used to pass NULL, so
 * the PCM went into the void. This is the sink that ends that.
 */
#include "../egojar.h"

#include "wubu_audio.h"
#include "bigmac/bigmac_out.h"

#include <stdlib.h>

static int  g_sink_open;
static int  g_rate = 48000;
static int  g_chan = 2;

void wubu_audio_sink_open(void)
{
    if (g_sink_open)
        return;
    if (bigmac_out_init(g_rate, g_chan, "/tmp/wubu_sound.wav") == 0)
        g_sink_open = 1;
}

void wubu_audio_sink_close(void)
{
    if (g_sink_open) {
        bigmac_out_close();
        g_sink_open = 0;
    }
}

void wubu_audio_sink_int16(const int16_t *pcm, int n)
{
    if (!g_sink_open || !pcm || n <= 0)
        return;

    float *buf = malloc((size_t)n * sizeof(float));
    if (!buf)
        return;
    for (int i = 0; i < n; i++)
        buf[i] = (float)pcm[i] / 32768.0f;
    bigmac_out_write(buf, n / g_chan);
    free(buf);
}

const char *wubu_audio_sink_backend(void)
{
    return g_sink_open ? bigmac_out_backend_name() : NULL;
}
