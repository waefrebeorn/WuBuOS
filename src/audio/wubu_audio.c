/*
 * wubu_audio.c — WuBuOS Audio Engine
 *
 * Cell 401: Combined DAW + Tracker + Synthesizer engine.
 *
 * Architecture:
 *   MIDI Input (USB/evdev) → Furnace Tracker → Chip Sound
 *                         → TinySoundFont → Sample Synth
 *                         → Ardour Mixer → Master Bus
 *                         → ALSA/JACK/PipeWire → Speakers
 *
 *   AI Plugin Container → DSP Processing → Insert on any track
 */

#include "wubu_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <dirent.h>

/* Include our pure-C math */
#include "../kernel/wubu_math.h"

/* ──────────────────────────────────────────────────────────────────
 *  GLOBAL ENGINE STATE
 * ────────────────────────────────────────────────────────────────── */

static WubuAudioEngine    g_engine     = {0};
static bool               g_init       = false;
static pthread_mutex_t    g_engine_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ──────────────────────────────────────────────────────────────────
 *  FURNACE TRACKER — CHIP EMULATIONS
 * ────────────────────────────────────────────────────────────────── */

/* Simple square wave for basic chip emulation */
static void furnace_square_wave(float *out, int frames, double freq, double volume, double *phase) {
    for (int i = 0; i < frames; i++) {
        *phase += freq / 48000.0;
        if (*phase >= 1.0) *phase -= 1.0;
        out[i] += (*phase < 0.5 ? 1.0 : -1.0) * volume;
    }
}

/* Simple saw wave */
static void furnace_saw_wave(float *out, int frames, double freq, double volume, double *phase) {
    for (int i = 0; i < frames; i++) {
        *phase += freq / 48000.0;
        if (*phase >= 1.0) *phase -= 1.0;
        out[i] += (2.0 * *phase - 1.0) * volume;
    }
}

/* Simple triangle wave */
static void furnace_triangle_wave(float *out, int frames, double freq, double volume, double *phase) {
    for (int i = 0; i < frames; i++) {
        *phase += freq / 48000.0;
        if (*phase >= 1.0) *phase -= 1.0;
        double val = *phase < 0.5 ? 4.0 * *phase - 1.0 : 3.0 - 4.0 * *phase;
        out[i] += val * volume;
    }
}

/* Noise generator */
static void furnace_noise(float *out, int frames, double volume, uint32_t *seed) {
    for (int i = 0; i < frames; i++) {
        *seed ^= *seed << 13;
        *seed ^= *seed >> 17;
        *seed ^= *seed << 5;
        float val = (*seed & 0x7FFFFFFF) / 1073741824.0 - 1.0;
        out[i] += val * volume;
    }
}

/* Note to frequency (A-4 = 440Hz) */
static double note_to_freq(int note, int octave) {
    int midi_note = octave * 12 + note;
    return 440.0 * wubu_pow(2.0, (midi_note - 69) / 12.0);
}

/* Furnace channel state */
typedef struct {
    WubuChipType  chip;
    double        phase;
    uint32_t      noise_seed;
    bool          active;
    uint8_t       note, octave, instrument, volume;
    uint8_t       effect, effect_val;
    double        freq;
    float         env_attack, env_decay, env_sustain, env_release;
    int           env_stage;
    double        env_level;
} FurnaceChannel;

/* Furnace pattern render context */
static FurnaceChannel furnace_chans[WUBU_AUDIO_MAX_FURNACE_CHANS];

int wubu_furnace_init(int n_chips, const WubuChipType *chips) {
    if (n_chips > WUBU_AUDIO_MAX_FURNACE_CHANS) n_chips = WUBU_AUDIO_MAX_FURNACE_CHANS;

    g_engine.furnace.n_chips = n_chips;
    for (int i = 0; i < n_chips; i++) {
        g_engine.furnace.chips[i] = chips ? chips[i] : CHIP_NONE;
        furnace_chans[i].chip = g_engine.furnace.chips[i];
        furnace_chans[i].noise_seed = 0x12345678 + i * 0x9ABCDEF;
        furnace_chans[i].env_attack = 0.01;
        furnace_chans[i].env_decay = 0.1;
        furnace_chans[i].env_sustain = 0.7;
        furnace_chans[i].env_release = 0.2;
    }

    g_engine.furnace.n_patterns = 0;
    g_engine.furnace.n_orders = 0;
    g_engine.furnace.tempo = 120;
    g_engine.furnace.speed = 6;
    g_engine.furnace.playing = false;

    g_engine.furnace_active = true;
    printf("[audio] Furnace tracker initialized: %d chips\n", n_chips);
    return 0;
}

void wubu_furnace_shutdown(void) {
    g_engine.furnace_active = false;
    memset(furnace_chans, 0, sizeof(furnace_chans));
    memset(&g_engine.furnace, 0, sizeof(g_engine.furnace));
}

void wubu_furnace_set_note(int pattern, int row, int chan,
                           uint8_t note, uint8_t octave) {
    if (pattern < WUBU_AUDIO_MAX_PATTERNS && row < 256 && chan < WUBU_AUDIO_MAX_FURNACE_CHANS) {
        WubuFurnacePattern *p = &g_engine.furnace.patterns[pattern];
        if (pattern >= g_engine.furnace.n_patterns) g_engine.furnace.n_patterns = pattern + 1;
        p->rows[row].cells[chan].note = note;
        p->rows[row].cells[chan].octave = octave;
        if (row >= p->n_rows) p->n_rows = row + 1;
    }
}

void wubu_furnace_set_inst(int pattern, int row, int chan, uint8_t inst) {
    if (pattern < WUBU_AUDIO_MAX_PATTERNS && row < 256 && chan < WUBU_AUDIO_MAX_FURNACE_CHANS) {
        WubuFurnacePattern *p = &g_engine.furnace.patterns[pattern];
        if (pattern >= g_engine.furnace.n_patterns) g_engine.furnace.n_patterns = pattern + 1;
        p->rows[row].cells[chan].instrument = inst;
        if (row >= p->n_rows) p->n_rows = row + 1;
    }
}

void wubu_furnace_set_effect(int pattern, int row, int chan,
                             uint8_t effect, uint8_t val) {
    if (pattern < WUBU_AUDIO_MAX_PATTERNS && row < 256 && chan < WUBU_AUDIO_MAX_FURNACE_CHANS) {
        WubuFurnacePattern *p = &g_engine.furnace.patterns[pattern];
        if (pattern >= g_engine.furnace.n_patterns) g_engine.furnace.n_patterns = pattern + 1;
        p->rows[row].cells[chan].effect = effect;
        p->rows[row].cells[chan].effect_val = val;
        if (row >= p->n_rows) p->n_rows = row + 1;
    }
}

void wubu_furnace_play(void) {
    g_engine.furnace.playing = true;
    g_engine.furnace.current_pattern = 0;
    g_engine.furnace.current_row = 0;
    g_engine.furnace.tick_accum = 0.0;

    /* Start notes on all active channels */
    for (int i = 0; i < g_engine.furnace.n_chips; i++) {
        furnace_chans[i].env_stage = 0;
        furnace_chans[i].env_level = 0.0;
        furnace_chans[i].active = false;
    }
    printf("[audio] Furnace: PLAY\n");
}

void wubu_furnace_stop(void) {
    g_engine.furnace.playing = false;
    for (int i = 0; i < g_engine.furnace.n_chips; i++) {
        if (furnace_chans[i].active) {
            furnace_chans[i].env_stage = 3; /* Release */
        }
    }
    printf("[audio] Furnace: STOP\n");
}

void wubu_furnace_set_tempo(int tempo) {
    if (tempo > 20 && tempo < 400) g_engine.furnace.tempo = tempo;
}

/* Render one Furnace pattern to audio buffer */
int wubu_furnace_render_pattern(int pattern, float *out, int frames) {
    if (pattern >= g_engine.furnace.n_patterns) return -1;

    WubuFurnacePattern *p = &g_engine.furnace.patterns[pattern];
    if (!p->n_rows) return 0; /* Empty pattern */

    /* Clear output */
    memset(out, 0, frames * sizeof(float));

    double rows_per_second = g_engine.furnace.tempo / 60.0 * g_engine.furnace.speed;
    double frame_duration = 1.0 / 48000.0;
    double row_duration = 1.0 / rows_per_second;

    int current_row = 0;
    double row_time = 0.0;

    /* Trigger row 0 notes at the start */
    for (int c = 0; c < g_engine.furnace.n_chips; c++) {
        WubuTrackerCell *cell = &p->rows[0].cells[c];
        if (cell->note != 255) {
            furnace_chans[c].note = cell->note;
            furnace_chans[c].octave = cell->octave;
            furnace_chans[c].instrument = cell->instrument;
            furnace_chans[c].volume = cell->volume / 15.0;
            furnace_chans[c].freq = note_to_freq(cell->note, cell->octave);
            furnace_chans[c].env_stage = 0; /* Attack */
            furnace_chans[c].env_level = 0.0;
            furnace_chans[c].active = true;
        }
    }

    for (int f = 0; f < frames; f++) {
        /* Check if we need to advance to next row */
        row_time += frame_duration;
        if (row_time >= row_duration) {
            row_time -= row_duration;
            current_row = (current_row + 1) % p->n_rows;

            /* Trigger new notes on this row */
            for (int c = 0; c < g_engine.furnace.n_chips; c++) {
                WubuTrackerCell *cell = &p->rows[current_row].cells[c];
                if (cell->note != 255) {
                    furnace_chans[c].note = cell->note;
                    furnace_chans[c].octave = cell->octave;
                    furnace_chans[c].instrument = cell->instrument;
                    furnace_chans[c].volume = cell->volume / 15.0;
                    furnace_chans[c].freq = note_to_freq(cell->note, cell->octave);
                    furnace_chans[c].env_stage = 0; /* Attack */
                    furnace_chans[c].env_level = 0.0;
                    furnace_chans[c].active = true;
                }
            }
        }

        /* Render all active channels */
        float sample = 0.0;
        (void)sample;
        for (int c = 0; c < g_engine.furnace.n_chips; c++) {
            if (!furnace_chans[c].active) continue;

            FurnaceChannel *ch = &furnace_chans[c];
            float vol = ch->volume * 0.1f;
            (void)vol;

            /* Envelope */
            if (ch->env_stage == 0) { /* Attack */
                ch->env_level += frame_duration / ch->env_attack;
                if (ch->env_level >= 1.0) {
                    ch->env_level = 1.0;
                    ch->env_stage = 1; /* Decay */
                }
            } else if (ch->env_stage == 1) { /* Decay */
                ch->env_level -= frame_duration / ch->env_decay;
                if (ch->env_level <= ch->env_sustain) {
                    ch->env_level = ch->env_sustain;
                    ch->env_stage = 2; /* Sustain */
                }
            } else if (ch->env_stage == 3) { /* Release */
                ch->env_level -= frame_duration / ch->env_release;
                if (ch->env_level <= 0.0) {
                    ch->env_level = 0.0;
                    ch->active = false;
                    continue;
                }
            }

            float env_vol = vol * ch->env_level;

            /* Generate based on chip type */
            WubuChipType chip = ch->chip;
            if (chip == CHIP_NONE) chip = g_engine.furnace.chips[c];

            switch (chip) {
                case CHIP_NES_APU:
                case CHIP_GB_APU:
                case CHIP_PCSPEAKER:
                    furnace_square_wave(out + f, 1, ch->freq, env_vol, &ch->phase);
                    break;
                case CHIP_YM2612:
                case CHIP_OPL:
                case CHIP_OPL2:
                case CHIP_OPL3:
                    furnace_saw_wave(out + f, 1, ch->freq, env_vol * 0.5f, &ch->phase);
                    break;
                case CHIP_SN76489:
                    furnace_square_wave(out + f, 1, ch->freq, env_vol, &ch->phase);
                    /* Add noise channel occasionally */
                    if (c == g_engine.furnace.n_chips - 1) furnace_noise(out + f, 1, env_vol * 0.1f, &ch->noise_seed);
                    break;
                case CHIP_SID:
                case CHIP_C64:
                    furnace_triangle_wave(out + f, 1, ch->freq, env_vol, &ch->phase);
                    break;
                default:
                    furnace_square_wave(out + f, 1, ch->freq, env_vol, &ch->phase);
            }
        }
    }

    return frames;
}

/* ──────────────────────────────────────────────────────────────────
 *  TINYSOUNDFONT — SF2 SYNTHESIS
 * ────────────────────────────────────────────────────────────────── */

/* Simplified SF2 parser — real implementation would parse RIFF/SF2 chunks */
static int sf2_parse(const uint8_t *data, size_t size, WubuSF2Synth *sf2) {
    if (size < 12) return -1;
    if (memcmp(data, "RIFF", 4) != 0) return -1;
    if (memcmp(data + 8, "sfbk", 4) != 0) return -1;

    /* Basic validation passed - set up dummy presets */
    sf2->sf2_data = malloc(size);
    if (!sf2->sf2_data) return -1;
    memcpy(sf2->sf2_data, data, size);
    sf2->sf2_size = size;

    /* Create some default presets for testing */
    const char *preset_names[] = {
        "Acoustic Grand Piano", "Bright Piano", "Electric Grand", "Honky-tonk",
        "E.Piano 1", "E.Piano 2", "Harpsichord", "Clavinet",
        "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
        "Marimba", "Xylophone", "Tubular Bells", "Dulcimer"
    };

    sf2->n_presets = 16;
    for (int i = 0; i < 16; i++) {
        strncpy(sf2->presets[i].name, preset_names[i], 31);
        sf2->presets[i].bank = 0;
        sf2->presets[i].preset = i;
        sf2->presets[i].layers = 1;
        sf2->presets[i].volume = 0.0f;
        sf2->presets[i].pan = 0.0f;
    }

    sf2->render_buffer = calloc(1024, sizeof(float));
    sf2->render_frames = 1024;
    sf2->render_pos = 0;
    sf2->max_voices = 64;
    sf2->active_voices = 0;
    sf2->reverb_mix = 0.1f;
    sf2->chorus_mix = 0.05f;
    memset(sf2->midi_channels, 0, 16);

    return 0;
}

int wubu_sf2_load(const uint8_t *data, size_t size) {
    int ret = sf2_parse(data, size, &g_engine.sf2);
    if (ret == 0) g_engine.sf2_active = true;
    return ret;
}

int wubu_sf2_load_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return -1; }
    size_t read = fread(data, 1, size, f);
    (void)read;
    fclose(f);

    int ret = sf2_parse(data, size, &g_engine.sf2);
    free(data);
    if (ret == 0) g_engine.sf2_active = true;
    return ret;
}

void wubu_sf2_unload(void) {
    if (g_engine.sf2.sf2_data) free(g_engine.sf2.sf2_data);
    if (g_engine.sf2.render_buffer) free(g_engine.sf2.render_buffer);
    memset(&g_engine.sf2, 0, sizeof(g_engine.sf2));
    g_engine.sf2_active = false;
}

/* Simple sine wave for SF2 sample playback (placeholder) */
static void sf2_render_voice(float *out, int frames, double freq, double volume, double *phase) {
    for (int i = 0; i < frames; i++) {
        *phase += freq / 48000.0;
        if (*phase >= 1.0) *phase -= 1.0;
        out[i] += wubu_sin(*phase * 2.0 * WUBU_PI) * volume;
    }
}

void wubu_sf2_note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (!g_engine.sf2_active) return;
    if (g_engine.sf2.active_voices >= g_engine.sf2.max_voices) return;

    double freq = 440.0 * wubu_pow(2.0, (note - 69) / 12.0);
    (void)freq;
    float vol = velocity / 127.0f * 0.5f;
    (void)vol;
    g_engine.sf2.active_voices++;
    g_engine.sf2.midi_channels[channel] = note;
}

void wubu_sf2_note_off(uint8_t channel, uint8_t note) {
    (void)note;
    if (!g_engine.sf2_active) return;
    if (g_engine.sf2.active_voices > 0) g_engine.sf2.active_voices--;
    g_engine.sf2.midi_channels[channel] = 0;
}

void wubu_sf2_program_change(uint8_t channel, uint8_t program) {
    if (program < g_engine.sf2.n_presets) {
        g_engine.sf2.midi_channels[channel] = program;
    }
}

void wubu_sf2_pitch_bend(uint8_t channel, int bend) {
    (void)channel; (void)bend;
    /* bend: -8192 to +8192, center 0 */
}

void wubu_sf2_control(uint8_t channel, uint8_t cc, uint8_t val) {
    (void)channel; (void)cc; (void)val;
    /* CC events: volume, pan, reverb, chorus, etc. */
}

void wubu_sf2_render(float *out, int frames, int channels) {
    if (!g_engine.sf2_active || g_engine.sf2.active_voices == 0) {
        memset(out, 0, frames * channels * sizeof(float));
        return;
    }

    /* Simple render - in reality would play samples with envelopes */
    static double phase = 0.0;
    double freq = 440.0; /* Would use actual played notes */

    for (int f = 0; f < frames; f++) {
        phase += freq / 48000.0;
        if (phase >= 1.0) phase -= 1.0;
        float sample = wubu_sin(phase * 2.0 * WUBU_PI) * 0.1f;
        for (int c = 0; c < channels; c++) {
            out[f * channels + c] = sample;
        }
    }
}

int wubu_sf2_preset_count(void) { return g_engine.sf2.n_presets; }
const WubuSF2Preset *wubu_sf2_preset(int idx) {
    if (idx >= 0 && idx < g_engine.sf2.n_presets) return &g_engine.sf2.presets[idx];
    return NULL;
}

/* ──────────────────────────────────────────────────────────────────
 *  ARDOUR-STYLE DAW MIXER
 * ────────────────────────────────────────────────────────────────── */

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
        t->sf2 = &g_engine.sf2;
    }

    int idx = g_engine.n_tracks++;
    pthread_mutex_unlock(&g_engine_mutex);
    printf("[audio] DAW: Added track %d: %s (%d)\n", idx, t->name, type);
    return idx;
}

void wubu_daw_remove_track(int track) {
    pthread_mutex_lock(&g_engine_mutex);
    if (track >= 0 && track < g_engine.n_tracks) {
        /* Shift down */
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

static void daw_mix_track(WubuDAWTrack *t, float *output, int frames, int channels, double dt) {
    (void)dt;
    if (t->mute) return;

    float gain = t->mute ? 0.0f : t->volume;

    /* Furnace track */
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

    /* SF2 track */
    if (t->type == TRACK_SF2 && t->sf2 && g_engine.sf2_active) {
        float temp[4096];
        wubu_sf2_render(temp, frames, channels);
        for (int f = 0; f < frames; f++) {
            for (int c = 0; c < channels; c++) {
                output[f * channels + c] += temp[f] * gain;
            }
        }
    }

    /* Audio track with regions */
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

    /* AI Plugin track */
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

    /* Apply pan */
    if (channels >= 2 && t->pan != 0.0f) {
        float left_gain = t->pan < 0 ? 1.0f : 1.0f - t->pan;
        float right_gain = t->pan > 0 ? 1.0f : 1.0f + t->pan;
        for (int f = 0; f < frames; f++) {
            output[f * channels]     *= left_gain;
            output[f * channels + 1] *= right_gain;
        }
    }
}

static void daw_mix_buses(float *output, int frames, int channels) {
    float bus_out[4096][2];
    memset(bus_out, 0, sizeof(bus_out));

    /* Process tracks in order, routing to buses */
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

    /* Process buses */
    for (int b = 0; b < g_engine.n_buses; b++) {
        if (!g_engine.buses[b].mute) {
            for (int f = 0; f < frames; f++) {
                for (int c = 0; c < channels; c++) {
                    output[f * channels + c] += bus_out[b][f] * g_engine.buses[b].volume;
                }
            }
        }
    }

    /* Master track processing */
    for (int t = 0; t < g_engine.n_tracks; t++) {
        if (g_engine.tracks[t].type == TRACK_MASTER) {
            daw_mix_track(&g_engine.tracks[t], output, frames, channels, 0);
        }
    }
}

/* ──────────────────────────────────────────────────────────────────
 *  AUDIO ENGINE MAIN PROCESSING
 * ────────────────────────────────────────────────────────────────── */

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

    /* Add master bus */
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

    /* Clear output */
    memset(output, 0, frames * g_engine.channels * sizeof(float));

    /* Mix all tracks through bus system */
    daw_mix_buses(output, frames, g_engine.channels);

    /* Update playhead */
    g_engine.playhead += (double)frames / g_engine.sample_rate;
    if (g_engine.total_length > 0 && g_engine.playhead >= g_engine.total_length) {
        g_engine.playhead = 0.0; /* Loop */
    }

    pthread_mutex_unlock(&g_engine_mutex);
}

/* ──────────────────────────────────────────────────────────────────
 *  USB MIDI INPUT
 * ────────────────────────────────────────────────────────────────── */

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

    /* ALSA sequencer */
    if (access("/dev/snd/seq", R_OK) == 0 && count < max) {
        snprintf(paths[count], 256, "/dev/snd/seq");
        snprintf(names[count], 64, "ALSA Sequencer");
        count++;
    }

    /* Raw MIDI devices */
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

    /* HID MIDI via evdev */
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

/* ──────────────────────────────────────────────────────────────────
 *  AI AUDIO PLUGINS (CONTAINER-BASED)
 * ────────────────────────────────────────────────────────────────── */

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
    p->container_id = -1; /* Would be set when container starts */

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

    /* Buffer input for container */
    if (p->input_buf && frames <= p->buf_frames) {
        memcpy(p->input_buf, input, frames * channels * sizeof(float));
    }

    /* In real implementation: send to container via 9P/pipe, wait for result */
    /* For now: pass through with slight processing */
    for (int f = 0; f < frames; f++) {
        for (int c = 0; c < channels; c++) {
            output[f * channels + c] = input[f * channels + c] * 0.95f; /* Slight gain change */
        }
    }

    return frames;
}

int wubu_ai_plugin_start(int plugin_idx) {
    if (plugin_idx < 0 || plugin_idx >= g_engine.n_ai_plugins) return -1;
    WubuAIPlugin *p = &g_engine.ai_plugins[plugin_idx];
    p->active = true;
    /* Would launch .wubu container with model */
    printf("[audio] AI Plugin started: %s\n", p->name);
    return 0;
}

void wubu_ai_plugin_stop(int plugin_idx) {
    if (plugin_idx < 0 || plugin_idx >= g_engine.n_ai_plugins) return;
    WubuAIPlugin *p = &g_engine.ai_plugins[plugin_idx];
    p->active = false;
    /* Would stop container */
}

/* ──────────────────────────────────────────────────────────────────
 *  INGESTION PROTOCOLS
 * ────────────────────────────────────────────────────────────────── */

static int ingest_handles[16] = {0};
static int n_ingest_handles = 0;

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
        /* JACK would be opened here */
        return ingest_handles[n_ingest_handles++] = -2; /* Special marker */
    } else if (strncmp(uri, "file:", 5) == 0) {
        const char *path = uri + 5;
        int fd = open(path, O_RDONLY);
        if (fd >= 0) return ingest_handles[n_ingest_handles++] = fd;
    } else if (strncmp(uri, "usb:bulk", 8) == 0) {
        /* USB bulk endpoint */
        return ingest_handles[n_ingest_handles++] = -3;
    } else if (strncmp(uri, "rtp:", 4) == 0) {
        /* RTP-MIDI */
        return ingest_handles[n_ingest_handles++] = -4;
    } else if (strncmp(uri, "container:", 10) == 0) {
        /* .wubu container namespace */
        return ingest_handles[n_ingest_handles++] = -5;
    }

    return -1;
}

int wubu_ingest_read(int handle, uint8_t *buf, int len) {
    for (int i = 0; i < n_ingest_handles; i++) {
        if (ingest_handles[i] == handle) {
            if (handle > 0) return read(handle, buf, len);
            return 0; /* Protocol handles */
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
void wubu_furnace_set_volume(int pattern, int row, int chan, uint8_t volume) {
    if (pattern < WUBU_AUDIO_MAX_PATTERNS && row < 256 && chan < WUBU_AUDIO_MAX_FURNACE_CHANS) {
        WubuFurnacePattern *p = &g_engine.furnace.patterns[pattern];
        if (pattern >= g_engine.furnace.n_patterns) g_engine.furnace.n_patterns = pattern + 1;
        p->rows[row].cells[chan].volume = volume;
        if (row >= p->n_rows) p->n_rows = row + 1;
    }
}

