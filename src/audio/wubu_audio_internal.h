/*
 * wubu_audio_internal.h  --  WuBuOS Audio Engine Internal Header
 * Shared declarations, global state, and internal functions for audio modules.
 * C11 opaque struct pattern: public types in wubu_audio.h, private here.
 */

#ifndef WUBU_AUDIO_INTERNAL_H
#define WUBU_AUDIO_INTERNAL_H

#include "wubu_audio.h"
#include "wubu_audio_types.h"
#include "../kernel/wubu_math.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>

/* ====================================================================
 * GLOBAL CHIP STATES (extern - defined in wubu_audio_chips.c)
 * ==================================================================== */
extern NesApu   g_nes_apu;
extern GbApu    g_gb_apu;
extern Ym2612   g_ym2612;
extern Sn76489  g_sn76489;
extern Sid      g_sid;
extern Saa1099  g_saa1099;
extern Vrc6     g_vrc6;
extern N163     g_n163;
extern Opl      g_opl;
extern Scc      g_scc;
extern Ay8910   g_ay8910;
extern PcSpeaker g_pc_speaker;

/* ====================================================================
 * FURNACE TRACKER STATE
 * ==================================================================== */
extern FurnaceChannel furnace_chans[WUBU_AUDIO_MAX_FURNACE_CHANS];

/* ====================================================================
 * SF2 SYNTHESIZER STATE
 * ==================================================================== */
extern WubuSF2Synth g_sf2_synth;

/* ====================================================================
 * AUDIO ENGINE GLOBAL STATE
 * ==================================================================== */
extern WubuAudioEngine    g_engine;
extern bool               g_init;
extern pthread_mutex_t    g_engine_mutex;

/* ====================================================================
 * MIDI/INGESTION STATE
 * ==================================================================== */
extern int ingest_handles[16];
extern int n_ingest_handles;

/* ====================================================================
 * SHARED HELPER FUNCTIONS (static inline for zero overhead)
 * ==================================================================== */

static inline void env_adsr_advance(int32_t *level, uint8_t *state,
    uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release,
    bool gate, double dt)
{
    const double rates[4] = {
        attack  ? 1.0 / (attack  * 0.001) : 1e6,
        decay   ? 1.0 / (decay   * 0.001) : 1e6,
        0,
        release ? 1.0 / (release * 0.001) : 1e6
    };

    if (gate) {
        if (*state == 0) {
            *level += rates[0] * dt;
            if (*level >= 1.0) { *level = 1.0; *state = 1; }
        } else if (*state == 1) {
            *level -= rates[1] * dt;
            double sl = sustain / 15.0;
            if (*level <= sl) { *level = sl; *state = 2; }
        } else if (*state == 2) {
            *level = sustain / 15.0;
        }
    } else {
        if (*state != 3) *state = 3;
        *level -= rates[3] * dt;
        if (*level <= 0.0) { *level = 0.0; *state = 4; }
    }
}

static inline void furnace_square_wave(float *out, int frames, double freq, double volume, double *phase) {
    for (int i = 0; i < frames; i++) {
        *phase += freq / 48000.0;
        if (*phase >= 1.0) *phase -= 1.0;
        out[i] += (*phase < 0.5 ? 1.0 : -1.0) * volume;
    }
}

static inline void furnace_saw_wave(float *out, int frames, double freq, double volume, double *phase) {
    for (int i = 0; i < frames; i++) {
        *phase += freq / 48000.0;
        if (*phase >= 1.0) *phase -= 1.0;
        out[i] += (2.0 * *phase - 1.0) * volume;
    }
}

static inline void furnace_triangle_wave(float *out, int frames, double freq, double volume, double *phase) {
    for (int i = 0; i < frames; i++) {
        *phase += freq / 48000.0;
        if (*phase >= 1.0) *phase -= 1.0;
        double val = *phase < 0.5 ? 4.0 * *phase - 1.0 : 3.0 - 4.0 * *phase;
        out[i] += val * volume;
    }
}

static inline void furnace_noise(float *out, int frames, double volume, uint32_t *seed) {
    for (int i = 0; i < frames; i++) {
        *seed ^= *seed << 13;
        *seed ^= *seed >> 17;
        *seed ^= *seed << 5;
        float val = (*seed & 0x7FFFFFFF) / 1073741824.0 - 1.0;
        out[i] += val * volume;
    }
}

static inline double note_to_freq(int note, int octave) {
    int midi_note = octave * 12 + note;
    return 440.0 * wubu_pow(2.0, (midi_note - 69) / 12.0);
}

/* ====================================================================
 * INTERNAL FUNCTION DECLARATIONS (non-static, shared across modules)
 * ==================================================================== */

/* Chip reset functions */
void nes_apu_reset(NesApu *a);
void gb_apu_reset(GbApu *g);
void ym2612_reset(Ym2612 *y);
void sn76489_reset(Sn76489 *s);
void sid_reset(Sid *s);
void saa1099_reset(Saa1099 *s);
void vrc6_reset(Vrc6 *v);
void n163_reset(N163 *n);
void opl_reset(Opl *o);
void scc_reset(Scc *s);
void ay8910_reset(Ay8910 *a);
void pc_speaker_reset(PcSpeaker *p);

/* Chip render functions */
float nes_apu_render(NesApu *a, double dt);
float gb_apu_render(GbApu *g, double dt);
float ym2612_render(Ym2612 *y, double dt);
float sn76489_render(Sn76489 *s, double dt);
float sid_render(Sid *s, double dt);
float saa1099_render(Saa1099 *s, double dt);
float vrc6_render(Vrc6 *v, double dt);
float n163_render(N163 *n, double dt);
float opl_render(Opl *o, double dt);
float scc_render(Scc *s, double dt);
float ay8910_render(Ay8910 *a, double dt);
float pc_speaker_render(PcSpeaker *p, double dt);

/* SF2 internal */
int sf2_parse(const uint8_t *data, size_t size, WubuSF2Synth *sf2);
void sf2_render_voice(float *out, int frames, double freq, double volume, double *phase);

/* DAW internal */
void daw_mix_track(WubuDAWTrack *t, float *output, int frames, int channels, double dt);
void daw_mix_buses(float *output, int frames, int channels);

/* MIDI/Ingestion internal */
int wubu_midi_enumerate(char paths[][256], char names[][64], int max);
int wubu_ingest_enumerate(char uris[][256], char names[][64], int max);

/* AI plugin internal */
int wubu_ai_plugin_process(int plugin_idx, const float *input, float *output, int frames, int channels);

#endif /* WUBU_AUDIO_INTERNAL_H */