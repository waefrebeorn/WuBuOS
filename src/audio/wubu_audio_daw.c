/*
 * wubu_audio_daw.c  --  WuBuOS DAW Mixer (Ardour-style)
 * Track management, bus routing, master processing.
 * Extracted from wubu_audio.c for modularity.
 */

#include "wubu_audio_internal.h"

/* ====================================================================
 * DAW INTERNAL IMPLEMENTATION
 * ==================================================================== */

void daw_mix_track(WubuDAWTrack *t, float *output, int frames, int channels, double dt) {
    (void)dt;
    if (t->mute) return;

    float gain = t->mute ? 0.0f : t->volume;

    if (t->type == TRACK_FURNACE && t->furnace && g_engine.furnace_active) {
        float temp[4096];
        int rendered = wubu_furnace_render_pattern(t->furnace->current_pattern, temp, frames);
        if (rendered > 0) {
            for (int f = 0; f < frames; f++) {
                for (int c = 0; c < channels; c++) {
                    output[f * channels + c] += temp[f] * gain;
                }
            }
        }
    }

    if (t->type == TRACK_SF2 && t->sf2 && g_engine.sf2_active) {
        float temp[4096];
        wubu_sf2_render(temp, frames, channels);
        for (int f = 0; f < frames; f++) {
            for (int c = 0; c < channels; c++) {
                output[f * channels + c] += temp[f] * gain;
            }
        }
    }

    if (t->type == TRACK_AUDIO) {
        for (int r = 0; r < t->n_regions; r++) {
            WubuAudioRegion *reg = &t->regions[r];
            double reg_end = reg->start_time + reg->duration;
            if (g_engine.playhead >= reg->start_time && g_engine.playhead < reg_end) {
                int sample_offset = (int)((g_engine.playhead - reg->start_time) * 48000.0);
                int samples_avail = (int)(reg->duration * 48000.0) - sample_offset;
                if (samples_avail > frames) samples_avail = frames;
                if (samples_avail > 0 && reg->samples) {
                    for (int f = 0; f < samples_avail; f++) {
                        for (int c = 0; c < channels; c++) {
                            if (c < reg->channels) {
                                output[f * channels + c] += reg->samples[(sample_offset + f) * reg->channels + c] * gain;
                            }
                        }
                    }
                }
            }
        }
    }

    if (t->type == TRACK_AI) {
        for (int p = 0; p < t->n_plugins; p++) {
            int plugin_idx = t->plugin_chain[p];
            if (plugin_idx >= 0 && plugin_idx < g_engine.n_ai_plugins) {
                WubuAIPlugin *plugin = &g_engine.ai_plugins[plugin_idx];
                if (plugin->active && plugin->input_buf && plugin->output_buf) {
                    for (int f = 0; f < frames; f++) {
                        for (int c = 0; c < channels; c++) {
                            output[f * channels + c] += plugin->output_buf[f * channels + c] * gain;
                        }
                    }
                }
            }
        }
    }

    if (channels >= 2 && t->pan != 0.0f) {
        float left_gain = t->pan < 0 ? 1.0f : 1.0f - t->pan;
        float right_gain = t->pan > 0 ? 1.0f : 1.0f + t->pan;
        for (int f = 0; f < frames; f++) {
            output[f * channels]     *= left_gain;
            output[f * channels + 1] *= right_gain;
        }
    }
}

void daw_mix_buses(float *output, int frames, int channels) {
    float bus_out[4096][2];
    memset(bus_out, 0, sizeof(bus_out));

    for (int t = 0; t < g_engine.n_tracks; t++) {
        WubuDAWTrack *tr = &g_engine.tracks[t];
        if (tr->type == TRACK_MASTER) continue;
        if (tr->type == TRACK_BUS) continue;

        int target_bus = tr->bus_send;
        if (target_bus >= 0 && target_bus < g_engine.n_buses) {
            daw_mix_track(tr, (float*)bus_out[target_bus], frames, 2, 0);
        } else {
            daw_mix_track(tr, output, frames, channels, 0);
        }
    }

    for (int b = 0; b < g_engine.n_buses; b++) {
        if (!g_engine.buses[b].mute) {
            for (int f = 0; f < frames; f++) {
                for (int c = 0; c < channels; c++) {
                    output[f * channels + c] += bus_out[b][f] * g_engine.buses[b].volume;
                }
            }
        }
    }

    for (int t = 0; t < g_engine.n_tracks; t++) {
        if (g_engine.tracks[t].type == TRACK_MASTER) {
            daw_mix_track(&g_engine.tracks[t], output, frames, channels, 0);
        }
    }
}

/* ====================================================================
 * DAW PUBLIC API
 * ==================================================================== */

int wubu_daw_add_track(const char *name, WubuTrackType type) {
    pthread_mutex_lock(&g_engine_mutex);
    if (g_engine.n_tracks >= WUBU_AUDIO_MAX_TRACKS) {
        pthread_mutex_unlock(&g_engine_mutex);
        return -1;
    }

    WubuDAWTrack *t = &g_engine.tracks[g_engine.n_tracks];
    memset(t, 0, sizeof(*t));
    strncpy(t->name, name ? name : "Track", 63);
    t->type = type;
    t->volume = 1.0f;
    t->pan = 0.0f;
    t->mute = false;
    t->solo = false;
    t->record_arm = false;
    t->bus_send = -1;

    if (type == TRACK_FURNACE) {
        t->furnace = &g_engine.furnace;
    } else if (type == TRACK_SF2) {
        t->sf2 = &g_sf2_synth;
    }

    int idx = g_engine.n_tracks++;
    pthread_mutex_unlock(&g_engine_mutex);
    printf("[audio] DAW: Added track %d: %s (%d)\n", idx, t->name, type);
    return idx;
}

void wubu_daw_remove_track(int track) {
    pthread_mutex_lock(&g_engine_mutex);
    if (track >= 0 && track < g_engine.n_tracks) {
        for (int i = track; i < g_engine.n_tracks - 1; i++) {
            g_engine.tracks[i] = g_engine.tracks[i + 1];
        }
        g_engine.n_tracks--;
    }
    pthread_mutex_unlock(&g_engine_mutex);
}

int wubu_daw_track_count(void) { return g_engine.n_tracks; }

void wubu_daw_play(void) {
    pthread_mutex_lock(&g_engine_mutex);
    g_engine.playing = true;
    g_engine.playhead = 0.0;
    if (g_engine.furnace_active) wubu_furnace_play();
    printf("[audio] DAW: PLAY\n");
    pthread_mutex_unlock(&g_engine_mutex);
}

void wubu_daw_stop(void) {
    pthread_mutex_lock(&g_engine_mutex);
    g_engine.playing = false;
    g_engine.playhead = 0.0;
    if (g_engine.furnace_active) wubu_furnace_stop();
    printf("[audio] DAW: STOP\n");
    pthread_mutex_unlock(&g_engine_mutex);
}

void wubu_daw_seek(double time) {
    pthread_mutex_lock(&g_engine_mutex);
    g_engine.playhead = time < 0 ? 0 : time;
    pthread_mutex_unlock(&g_engine_mutex);
}

void wubu_daw_set_loop(double start, double end) {
    g_engine.total_length = end > start ? end - start : 0;
}

void wubu_daw_record_start(int track) {
    if (track >= 0 && track < g_engine.n_tracks) {
        g_engine.tracks[track].record_arm = true;
        g_engine.recording = true;
        printf("[audio] DAW: Record armed track %d\n", track);
    }
}

void wubu_daw_record_stop(void) {
    g_engine.recording = false;
    for (int i = 0; i < g_engine.n_tracks; i++) {
        g_engine.tracks[i].record_arm = false;
    }
}