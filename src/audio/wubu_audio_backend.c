/*
 * wubu_audio_backend.c -- the device output thread.
 *
 * The engine renders into a caller buffer; big-mac writes frames to a
 * speaker. This is the loop that joins them: a thread that pulls
 * wubu_audio_process() and pushes bigmac_out_write() until stopped.
 *
 * Why a thread and not a callback from the caller: the engine has no
 * real-time clock of its own and nothing was calling it at all. A thread
 * with a fixed frame quantum gives the device a steady stream and keeps
 * the render path free of any knowledge of backends.
 *
 * C11. Best effort: if no backend opens, the thread exits and the engine
 * still renders (a caller can call wubu_audio_process() directly).
 */
#include "../egojar.h"

#include "wubu_audio.h"
#include "wubu_audio_internal.h"
#include "bigmac/bigmac_out.h"

#include <stdio.h>

#define WUBU_AUDIO_QUANTUM 256

static pthread_t g_thread;
static volatile int g_running;
static int g_started;

/* The backend name at init, for wubu_audio_backend_name(). */
static char g_backend_name[16];

static void *backend_thread(void *arg)
{
    (void)arg;
    int channels = g_engine.channels > 0 ? g_engine.channels : 2;
    float *buf = malloc((size_t)WUBU_AUDIO_QUANTUM * (size_t)channels *
                        sizeof(float));
    if (!buf) {
        g_running = 0;
        return NULL;
    }

    while (g_running) {
        wubu_audio_process(buf, WUBU_AUDIO_QUANTUM);
        if (bigmac_out_write(buf, WUBU_AUDIO_QUANTUM) < 0) {
            /* a hard device error ends the loop rather than spinning */
            g_running = 0;
            break;
        }
    }

    free(buf);
    return NULL;
}

/* Start the output thread. Returns the backend name, or NULL if nothing
 * could open (the engine still renders for anyone who calls process()). */
const char *wubu_audio_backend_start(void)
{
    if (g_started)
        return g_backend_name;

    int rate = g_engine.sample_rate > 0 ? g_engine.sample_rate : 48000;

    if (bigmac_out_init(rate, g_engine.channels, "/tmp/wubu_audio.wav") != 0) {
        g_backend_name[0] = '\0';
        return NULL;
    }

    snprintf(g_backend_name, sizeof(g_backend_name), "%s",
             bigmac_out_backend_name());

    g_running = 1;
    if (pthread_create(&g_thread, NULL, backend_thread, NULL) != 0) {
        g_running = 0;
        bigmac_out_close();
        g_backend_name[0] = '\0';
        return NULL;
    }
    g_started = 1;
    return g_backend_name;
}

void wubu_audio_backend_stop(void)
{
    if (!g_started)
        return;
    g_running = 0;
    pthread_join(g_thread, NULL);
    bigmac_out_close();
    g_started = 0;
}

int wubu_audio_backend_running(void) { return g_started && g_running; }

int wubu_audio_backend_latency(void)
{
    return bigmac_out_latency_frames();
}
