/*
 * wubu_metal_audio.c -- WuBuOS Metal audio backends (split from wubu_metal.c).
 *
 * Self-contained: ALSA + PulseAudio + PipeWire backends (real + dlopen-stub
 * variants), each with init/shutdown/submit/cpu_load. Dispatched by
 * wubu_audio_* in wubu_metal.c. C11 opaque-struct pattern: backend
 * fns declared non-static in wubu_metal_internal.h.
 */

#include "wubu_metal_audio.h"
#include <dlfcn.h>
#include <stdio.h>

/* ------------------------------------------------------------------
 *  ALSA AUDIO BACKEND (BARE-METAL)
 * ------------------------------------------------------------------ */

#ifdef WUBU_USE_ALSA
#include <alsa/asoundlib.h>

int wubu_alsa_init(int sample_rate, int channels, int buffer_frames) {
    snd_pcm_t *handle;
    int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "ALSA open failed: %s\n", snd_strerror(err));
        return -1;
    }

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_FLOAT_LE);
    snd_pcm_hw_params_set_channels(handle, params, channels);
    unsigned int rate = sample_rate;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    snd_pcm_uframes_t frames = buffer_frames;
    snd_pcm_hw_params_set_buffer_size_near(handle, params, &frames);

    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        fprintf(stderr, "ALSA hw_params failed: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return -1;
    }

    snd_pcm_prepare(handle);

    g_audio.backend       = AUDIO_ALSA;
    g_audio.sample_rate   = sample_rate;
    g_audio.channels      = channels;
    g_audio.buffer_frames = buffer_frames;
    g_audio.alsa_pcm_fd   = snd_pcm_file_descriptor(handle);
    g_audio.alsa_handle   = handle;
    g_audio.render_buf    = calloc(buffer_frames * channels, sizeof(float));
    g_audio.render_buf_size = buffer_frames * channels;

    printf("[metal] ALSA initialized: %dHz %dch %d frames\n", sample_rate, channels, buffer_frames);
    return 0;
}

void wubu_alsa_shutdown(void) {
    if (g_audio.alsa_handle) {
        snd_pcm_drain((snd_pcm_t*)g_audio.alsa_handle);
        snd_pcm_close((snd_pcm_t*)g_audio.alsa_handle);
        g_audio.alsa_handle = NULL;
    }
    if (g_audio.render_buf) {
        free(g_audio.render_buf);
        g_audio.render_buf = NULL;
    }
}

void wubu_alsa_submit(const float *buf, int frames) {
    if (!g_audio.alsa_handle || !buf) return;
    snd_pcm_writei((snd_pcm_t*)g_audio.alsa_handle, buf, frames);
}

double wubu_alsa_cpu_load(void) {
    return 0.0; /* Would need ALSA timing info */
}
#else
/* Try dlopen libasound at runtime */
int wubu_alsa_init(int sample_rate, int channels, int buffer_frames) {
    void *alsa_lib = dlopen("libasound.so.2", RTLD_LAZY);
    if (!alsa_lib) alsa_lib = dlopen("libasound.so", RTLD_LAZY);
    if (!alsa_lib) {
        fprintf(stderr, "ALSA not available (libasound not found)\n");
        return -1;
    }
    dlclose(alsa_lib);
    (void)sample_rate; (void)channels; (void)buffer_frames;
    return -1;  /* Not implemented without libasound headers */
}
void wubu_alsa_shutdown(void) {}
void wubu_alsa_submit(const float *buf, int frames) { (void)buf; (void)frames; }
double wubu_alsa_cpu_load(void) { return 0.0; }
#endif

/* ------------------------------------------------------------------
 *  PULSEAUDIO BACKEND (HOSTED/WSL2)
 * ------------------------------------------------------------------ */

#ifdef WUBU_USE_PULSE
#include <pulse/simple.h>
#include <pulse/error.h>

int wubu_pulse_init(int sample_rate, int channels, int buffer_frames) {
    pa_sample_spec ss = { .format = PA_SAMPLE_FLOAT32LE, .rate = sample_rate, .channels = channels };
    pa_buffer_attr attr = { .maxlength = (uint32_t)-1, .tlength = buffer_frames * channels * sizeof(float),
                            .prebuf = (uint32_t)-1, .minreq = (uint32_t)-1, .fragsize = (uint32_t)-1 };
    int error;
    pa_simple *s = pa_simple_new(NULL, "WuBuOS", PA_STREAM_PLAYBACK, NULL, "WuBuOS Audio", &ss, NULL, &attr, &error);
    if (!s) {
        fprintf(stderr, "PulseAudio connect failed: %s\n", pa_strerror(error));
        return -1;
    }

    g_audio.backend       = AUDIO_PULSE;
    g_audio.sample_rate   = sample_rate;
    g_audio.channels      = channels;
    g_audio.buffer_frames = buffer_frames;
    g_audio.pa_handle     = s;

    printf("[metal] PulseAudio initialized: %dHz %dch\n", sample_rate, channels);
    return 0;
}

void wubu_pulse_shutdown(void) {
    if (g_audio.pa_handle) {
        pa_simple_drain((pa_simple*)g_audio.pa_handle, NULL);
        pa_simple_free((pa_simple*)g_audio.pa_handle);
        g_audio.pa_handle = NULL;
    }
}

void wubu_pulse_submit(const float *buf, int frames) {
    if (!g_audio.pa_handle || !buf) return;
    int error;
    pa_simple_write((pa_simple*)g_audio.pa_handle, buf, frames * g_audio.channels * sizeof(float), &error);
}

double wubu_pulse_cpu_load(void) { return 0.0; }
#else
/* Try dlopen libpulse at runtime */
int wubu_pulse_init(int sample_rate, int channels, int buffer_frames) {
    void *pulse_lib = dlopen("libpulse.so.0", RTLD_LAZY);
    if (!pulse_lib) pulse_lib = dlopen("libpulse.so", RTLD_LAZY);
    if (!pulse_lib) {
        fprintf(stderr, "PulseAudio not available (libpulse not found)\n");
        return -1;
    }
    dlclose(pulse_lib);
    (void)sample_rate; (void)channels; (void)buffer_frames;
    return -1;  /* Not implemented without libpulse headers */
}
void wubu_pulse_shutdown(void) {}
void wubu_pulse_submit(const float *buf, int frames) { (void)buf; (void)frames; }
double wubu_pulse_cpu_load(void) { return 0.0; }
#endif

/* ------------------------------------------------------------------
 *  PIPEWIRE BACKEND (MODERN LINUX AUDIO)
 * ------------------------------------------------------------------ */

#ifdef WUBU_USE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

static struct pw_main_loop *g_pw_loop = NULL;
static struct pw_context *g_pw_context = NULL;
static struct pw_core *g_pw_core = NULL;
static struct pw_stream *g_pw_stream = NULL;

static void pw_stream_param_changed(void *data, uint32_t id, const struct spa_pod *param) {
    (void)data; (void)id; (void)param;
    /* Handle format negotiation */
}

static const struct pw_stream_events pw_stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = pw_stream_param_changed,
};

int wubu_pipewire_init(int sample_rate, int channels, int buffer_frames) {
    g_pw_loop = pw_main_loop_new(NULL);
    if (!g_pw_loop) {
        fprintf(stderr, "PipeWire: failed to create main loop\n");
        return -1;
    }

    g_pw_context = pw_context_new(pw_main_loop_get_loop(g_pw_loop), NULL, 0);
    if (!g_pw_context) {
        fprintf(stderr, "PipeWire: failed to create context\n");
        pw_main_loop_destroy(g_pw_loop);
        return -1;
    }

    g_pw_core = pw_context_connect(g_pw_context, PW_CONTEXT_CONNECT_REGISTRY, NULL, 0);
    if (!g_pw_core) {
        fprintf(stderr, "PipeWire: failed to connect to core\n");
        pw_context_destroy(g_pw_context);
        pw_main_loop_destroy(g_pw_loop);
        return -1;
    }

    struct pw_stream *stream = pw_stream_new(g_pw_core, "WuBuOS Audio", PW_KEY_MEDIA_TYPE, "Audio",
                                             PW_KEY_MEDIA_CATEGORY, "Playback",
                                             PW_KEY_MEDIA_ROLE, "Music",
                                             NULL);
    if (!stream) {
        fprintf(stderr, "PipeWire: failed to create stream\n");
        pw_core_disconnect(g_pw_core);
        pw_context_destroy(g_pw_context);
        pw_main_loop_destroy(g_pw_loop);
        return -1;
    }

    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
                                           &SPA_AUDIO_INFO_RAW_INIT(
                                               .format = SPA_AUDIO_FORMAT_F32P,
                                               .channels = channels,
                                               .rate = sample_rate));

    pw_stream_add_listener(stream, &g_audio.pw_listener, &pw_stream_events, NULL);

    if (pw_stream_connect(stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                          PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
                          params, 1) < 0) {
        fprintf(stderr, "PipeWire: failed to connect stream\n");
        pw_stream_destroy(stream);
        pw_core_disconnect(g_pw_core);
        pw_context_destroy(g_pw_context);
        pw_main_loop_destroy(g_pw_loop);
        return -1;
    }

    g_audio.backend       = AUDIO_PIPEWIRE;
    g_audio.sample_rate   = sample_rate;
    g_audio.channels      = channels;
    g_audio.buffer_frames = buffer_frames;
    g_audio.pw_stream     = stream;

    printf("[metal] PipeWire initialized: %dHz %dch\n", sample_rate, channels);
    return 0;
}

void wubu_pipewire_shutdown(void) {
    if (g_audio.pw_stream) {
        pw_stream_destroy(g_audio.pw_stream);
        g_audio.pw_stream = NULL;
    }
    if (g_pw_core) {
        pw_core_disconnect(g_pw_core);
        g_pw_core = NULL;
    }
    if (g_pw_context) {
        pw_context_destroy(g_pw_context);
        g_pw_context = NULL;
    }
    if (g_pw_loop) {
        pw_main_loop_destroy(g_pw_loop);
        g_pw_loop = NULL;
    }
}

void wubu_pipewire_submit(const float *buf, int frames) {
    if (!g_audio.pw_stream || !buf) return;
    
    struct pw_buffer *pw_buf = pw_stream_dequeue_buffer(g_audio.pw_stream);
    if (!pw_buf) return;

    struct spa_data *d = &pw_buf->buffer->datas[0];
    memcpy(d->data, buf, frames * g_audio.channels * sizeof(float));
    d->chunk->offset = 0;
    d->chunk->stride = g_audio.channels * sizeof(float);
    d->chunk->size = frames * d->chunk->stride;

    pw_stream_queue_buffer(g_audio.pw_stream, pw_buf);
}

double wubu_pipewire_cpu_load(void) { return 0.0; }
#else
int wubu_pipewire_init(int sample_rate, int channels, int buffer_frames) {
    void *pw_lib = dlopen("libpipewire-0.3.so.0", RTLD_LAZY);
    if (!pw_lib) pw_lib = dlopen("libpipewire-0.3.so", RTLD_LAZY);
    if (!pw_lib) {
        return -1;  /* Not available */
    }
    dlclose(pw_lib);
    (void)sample_rate; (void)channels; (void)buffer_frames;
    return -1;
}
void wubu_pipewire_shutdown(void) {}
void wubu_pipewire_submit(const float *buf, int frames) { (void)buf; (void)frames; }
double wubu_pipewire_cpu_load(void) { return 0.0; }
#endif