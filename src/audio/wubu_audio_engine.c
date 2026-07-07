/*
 * wubu_audio_engine.c  --  WuBuOS Audio Engine Main
 * Engine lifecycle, process callback, global state.
 * Extracted from wubu_audio.c for modularity.
 */

#include "wubu_audio_internal.h"

/* ====================================================================
 * AUDIO ENGINE GLOBAL STATE
 * ==================================================================== */

WubuAudioEngine    g_engine     = {0};
bool               g_init       = false;
pthread_mutex_t    g_engine_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ====================================================================
 * MIDI/INGESTION STATE
 * ==================================================================== */

int ingest_handles[16] = {0};
int n_ingest_handles = 0;

/* ====================================================================
 * AUDIO ENGINE LIFECYCLE
 * ==================================================================== */

int wubu_audio_engine_create(int sample_rate, int buffer_frames, int channels) {
    if (g_init) return 0;

    g_engine.sample_rate = sample_rate;
    g_engine.buffer_frames = buffer_frames;
    g_engine.channels = channels;

    g_engine.n_tracks = 0;
    g_engine.n_buses = 0;
    g_engine.playhead = 0.0;
    g_engine.total_length = 0.0;
    g_engine.playing = false;
    g_engine.recording = false;

    g_engine.furnace_active = false;
    g_engine.sf2_active = false;
    g_engine.n_ai_plugins = 0;
    g_engine.n_midi_fds = 0;

    g_engine.master_buf = calloc(buffer_frames * channels, sizeof(float));
    g_engine.master_buf_size = buffer_frames * channels;
    g_engine.master_r = g_engine.master_w = 0;

    g_engine.buses[0].volume = 1.0f;
    g_engine.buses[0].mute = false;
    g_engine.buses[0].input_track = -1;
    g_engine.n_buses = 1;

    g_init = true;
    printf("[audio] Engine created: %dHz %dch %d frames\n", sample_rate, channels, buffer_frames);
    return 0;
}

void wubu_audio_engine_destroy(void) {
    if (!g_init) return;
    wubu_audio_stop();
    wubu_furnace_shutdown();
    wubu_sf2_unload();
    for (int i = 0; i < g_engine.n_ai_plugins; i++) {
        wubu_ai_plugin_stop(i);
    }
    if (g_engine.master_buf) free(g_engine.master_buf);
    memset(&g_engine, 0, sizeof(g_engine));
    g_init = false;
}

WubuAudioEngine *wubu_audio_engine(void) { return &g_engine; }

int wubu_audio_start(void) {
    if (!g_init) return -1;
    wubu_daw_play();
    return 0;
}

void wubu_audio_stop(void) {
    wubu_daw_stop();
}

void wubu_audio_process(float *output, int frames) {
    if (!g_init || !output || !g_engine.playing) {
        memset(output, 0, frames * g_engine.channels * sizeof(float));
        return;
    }

    pthread_mutex_lock(&g_engine_mutex);

    memset(output, 0, frames * g_engine.channels * sizeof(float));
    daw_mix_buses(output, frames, g_engine.channels);

    g_engine.playhead += (double)frames / g_engine.sample_rate;
    if (g_engine.total_length > 0 && g_engine.playhead >= g_engine.total_length) {
        g_engine.playhead = 0.0;
    }

    pthread_mutex_unlock(&g_engine_mutex);
}

/* ====================================================================
 * MIDI INPUT API
 * ==================================================================== */

int wubu_midi_open(const char *path) {
    if (g_engine.n_midi_fds >= 4) return -1;

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return -1;

    g_engine.midi_fds[g_engine.n_midi_fds++] = fd;
    return fd;
}

void wubu_midi_close(int fd) {
    for (int i = 0; i < g_engine.n_midi_fds; i++) {
        if (g_engine.midi_fds[i] == fd) {
            close(fd);
            for (int j = i; j < g_engine.n_midi_fds - 1; j++) {
                g_engine.midi_fds[j] = g_engine.midi_fds[j + 1];
            }
            g_engine.n_midi_fds--;
            break;
        }
    }
}

int wubu_midi_read(int fd, uint8_t *buf, int len) {
    return read(fd, buf, len);
}

int wubu_midi_enumerate(char paths[][256], char names[][64], int max) {
    int count = 0;

    if (access("/dev/snd/seq", R_OK) == 0 && count < max) {
        snprintf(paths[count], 256, "/dev/snd/seq");
        snprintf(names[count], 64, "ALSA Sequencer");
        count++;
    }

    DIR *d = opendir("/dev/snd");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) && count < max) {
            if (strncmp(ent->d_name, "midi", 4) == 0) {
                char path[256];
                snprintf(path, sizeof(path), "/dev/snd/%s", ent->d_name);
                snprintf(paths[count], 256, "%s", path);
                snprintf(names[count], 64, "MIDI: %s", ent->d_name);
                count++;
            }
        }
        closedir(d);
    }

    d = opendir("/dev/input");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) && count < max) {
            if (strncmp(ent->d_name, "event", 5) == 0) {
                char path[256];
                snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
                int fd = open(path, O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    uint8_t evbit[EV_MAX / 8 + 1] = {0};
                    ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit);
                    if (evbit[EV_KEY / 8] & (1 << (EV_KEY % 8))) {
                        uint8_t keybit[KEY_MAX / 8 + 1] = {0};
                        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
                        if (keybit[BTN_MISC / 8] & (1 << (BTN_MISC % 8))) {
                            snprintf(paths[count], 256, "%s", path);
                            snprintf(names[count], 64, "HID MIDI: %s", ent->d_name);
                            count++;
                        }
                    }
                    close(fd);
                }
            }
        }
        closedir(d);
    }

    return count;
}

/* ====================================================================
 * AI AUDIO PLUGINS
 * ==================================================================== */

int wubu_ai_plugin_register(const char *name, WubuAIPluginType type,
                            const char *model_path) {
    pthread_mutex_lock(&g_engine_mutex);
    if (g_engine.n_ai_plugins >= WUBU_AUDIO_MAX_AI_PLUGINS) {
        pthread_mutex_unlock(&g_engine_mutex);
        return -1;
    }

    WubuAIPlugin *p = &g_engine.ai_plugins[g_engine.n_ai_plugins];
    memset(p, 0, sizeof(*p));
    p->type = type;
    strncpy(p->name, name ? name : "AI Plugin", 63);
    if (model_path) strncpy(p->model_path, model_path, 255);
    p->active = false;
    p->buf_frames = 1024;
    p->channels = 2;
    p->model_frames = 2048;
    p->input_buf = calloc(1024 * 2, sizeof(float));
    p->output_buf = calloc(1024 * 2, sizeof(float));
    p->model_input = calloc(2048, sizeof(float));
    p->model_output = calloc(2048, sizeof(float));
    p->container_id = -1;

    int idx = g_engine.n_ai_plugins++;
    pthread_mutex_unlock(&g_engine_mutex);
    printf("[audio] AI Plugin registered: %d: %s (%d)\n", idx, name, type);
    return idx;
}

int wubu_ai_plugin_process(int plugin_idx,
                           const float *input, float *output,
                           int frames, int channels) {
    if (plugin_idx < 0 || plugin_idx >= g_engine.n_ai_plugins) return -1;
    WubuAIPlugin *p = &g_engine.ai_plugins[plugin_idx];
    if (!p->active) return 0;

    if (p->input_buf && frames <= p->buf_frames) {
        memcpy(p->input_buf, input, frames * channels * sizeof(float));
    }

    for (int f = 0; f < frames; f++) {
        for (int c = 0; c < channels; c++) {
            output[f * channels + c] = input[f * channels + c] * 0.95f;
        }
    }

    return frames;
}

int wubu_ai_plugin_start(int plugin_idx) {
    if (plugin_idx < 0 || plugin_idx >= g_engine.n_ai_plugins) return -1;
    WubuAIPlugin *p = &g_engine.ai_plugins[plugin_idx];
    p->active = true;
    printf("[audio] AI Plugin started: %s\n", p->name);
    return 0;
}

void wubu_ai_plugin_stop(int plugin_idx) {
    if (plugin_idx < 0 || plugin_idx >= g_engine.n_ai_plugins) return;
    WubuAIPlugin *p = &g_engine.ai_plugins[plugin_idx];
    p->active = false;
}

/* ====================================================================
 * INGESTION PROTOCOLS
 * ==================================================================== */

int wubu_ingest_open(const char *uri) {
    if (n_ingest_handles >= 16) return -1;
    if (!uri) return -1;

    printf("[audio] Ingest open: %s\n", uri);

    if (strncmp(uri, "alsa:seq", 8) == 0) {
        int fd = open("/dev/snd/seq", O_RDONLY | O_NONBLOCK);
        if (fd >= 0) return ingest_handles[n_ingest_handles++] = fd;
    } else if (strncmp(uri, "evdev:", 6) == 0) {
        const char *path = uri + 6;
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) return ingest_handles[n_ingest_handles++] = fd;
    } else if (strncmp(uri, "jack", 4) == 0) {
        return ingest_handles[n_ingest_handles++] = -2;
    } else if (strncmp(uri, "file:", 5) == 0) {
        const char *path = uri + 5;
        int fd = open(path, O_RDONLY);
        if (fd >= 0) return ingest_handles[n_ingest_handles++] = fd;
    } else if (strncmp(uri, "usb:bulk", 8) == 0) {
        return ingest_handles[n_ingest_handles++] = -3;
    } else if (strncmp(uri, "rtp:", 4) == 0) {
        return ingest_handles[n_ingest_handles++] = -4;
    } else if (strncmp(uri, "container:", 10) == 0) {
        return ingest_handles[n_ingest_handles++] = -5;
    }

    return -1;
}

int wubu_ingest_read(int handle, uint8_t *buf, int len) {
    for (int i = 0; i < n_ingest_handles; i++) {
        if (ingest_handles[i] == handle) {
            if (handle > 0) return read(handle, buf, len);
            return 0;
        }
    }
    return -1;
}

void wubu_ingest_close(int handle) {
    for (int i = 0; i < n_ingest_handles; i++) {
        if (ingest_handles[i] == handle) {
            if (handle > 0) close(handle);
            for (int j = i; j < n_ingest_handles - 1; j++) {
                ingest_handles[j] = ingest_handles[j + 1];
            }
            n_ingest_handles--;
            break;
        }
    }
}

int wubu_ingest_enumerate(char uris[][256], char names[][64], int max) {
    int count = 0;

    if (access("/dev/snd/seq", R_OK) == 0 && count < max) {
        snprintf(uris[count], 256, "alsa:seq");
        snprintf(names[count], 64, "ALSA Sequencer");
        count++;
    }

    if (access("/dev/snd/midiC0D0", R_OK) == 0 && count < max) {
        snprintf(uris[count], 256, "file:/dev/snd/midiC0D0");
        snprintf(names[count], 64, "Raw MIDI 0");
        count++;
    }

    DIR *d = opendir("/dev/input");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) && count < max) {
            if (strncmp(ent->d_name, "event", 5) == 0) {
                char path[256];
                snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
                int fd = open(path, O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    uint8_t evbit[EV_MAX / 8 + 1] = {0};
                    ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit);
                    if (evbit[EV_KEY / 8] & (1 << (EV_KEY % 8))) {
                        snprintf(uris[count], 256, "evdev:%s", path);
                        snprintf(names[count], 64, "evdev: %s", ent->d_name);
                        count++;
                    }
                    close(fd);
                }
            }
        }
        closedir(d);
    }

    if (count < max) {
        snprintf(uris[count], 256, "jack");
        snprintf(names[count], 64, "JACK Audio");
        count++;
    }

    if (count < max) {
        snprintf(uris[count], 256, "container:wubu-ai-separation");
        snprintf(names[count], 64, "AI: Source Separation");
        count++;
    }

    return count;
}