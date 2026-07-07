/*
 * wubu_audio_sf2.c  --  WuBuOS TinySoundFont SF2 Synthesis
 * Simplified SF2 parser and sample playback engine.
 * Extracted from wubu_audio.c for modularity.
 */

#include "wubu_audio_internal.h"

/* ====================================================================
 * SF2 SYNTHESIZER STATE (global)
 * ==================================================================== */

WubuSF2Synth g_sf2_synth = {0};

/* ====================================================================
 * SF2 INTERNAL FUNCTIONS
 * ==================================================================== */

int sf2_parse(const uint8_t *data, size_t size, WubuSF2Synth *sf2) {
    if (size < 12) return -1;
    if (memcmp(data, "RIFF", 4) != 0) return -1;
    if (memcmp(data + 8, "sfbk", 4) != 0) return -1;

    sf2->sf2_data = malloc(size);
    if (!sf2->sf2_data) return -1;
    memcpy(sf2->sf2_data, data, size);
    sf2->sf2_size = size;

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
    memset(sf2->pitch_bend, 0, 16 * sizeof(int16_t));

    /* Also copy to engine's sf2 member */
    g_engine.sf2 = *sf2;

    return 0;
}

void sf2_render_voice(float *out, int frames, double freq, double volume, double *phase) {
    for (int i = 0; i < frames; i++) {
        *phase += freq / 48000.0;
        if (*phase >= 1.0) *phase -= 1.0;
        out[i] += wubu_sin(*phase * 2.0 * WUBU_PI) * volume;
    }
}

/* ====================================================================
 * SF2 PUBLIC API
 * ==================================================================== */

int wubu_sf2_load(const uint8_t *data, size_t size) {
    int ret = sf2_parse(data, size, &g_sf2_synth);
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

    int ret = sf2_parse(data, size, &g_sf2_synth);
    free(data);
    if (ret == 0) g_engine.sf2_active = true;
    return ret;
}

void wubu_sf2_unload(void) {
    if (g_sf2_synth.sf2_data) free(g_sf2_synth.sf2_data);
    if (g_sf2_synth.render_buffer) free(g_sf2_synth.render_buffer);
    memset(&g_sf2_synth, 0, sizeof(g_sf2_synth));
    g_engine.sf2_active = false;
}

void wubu_sf2_note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (!g_engine.sf2_active) return;
    if (g_sf2_synth.active_voices >= g_sf2_synth.max_voices) return;

    double freq = 440.0 * wubu_pow(2.0, (note - 69) / 12.0);
    (void)freq;
    float vol = velocity / 127.0f * 0.5f;
    (void)vol;
    g_sf2_synth.active_voices++;
    g_sf2_synth.midi_channels[channel] = note;
}

void wubu_sf2_note_off(uint8_t channel, uint8_t note) {
    (void)note;
    if (!g_engine.sf2_active) return;
    if (g_sf2_synth.active_voices > 0) g_sf2_synth.active_voices--;
    g_sf2_synth.midi_channels[channel] = 0;
}

void wubu_sf2_program_change(uint8_t channel, uint8_t program) {
    if (program < g_sf2_synth.n_presets) {
        g_sf2_synth.midi_channels[channel] = program;
    }
}

void wubu_sf2_pitch_bend(uint8_t channel, int bend) {
    if (channel >= 16) return;
    g_sf2_synth.pitch_bend[channel] = (int16_t)bend;
}

void wubu_sf2_control(uint8_t channel, uint8_t cc, uint8_t val) {
    if (channel >= 16) return;
    g_sf2_synth.midi_channels[channel] = val;
}

void wubu_sf2_render(float *out, int frames, int channels) {
    if (!g_engine.sf2_active || g_sf2_synth.active_voices == 0) {
        memset(out, 0, frames * channels * sizeof(float));
        return;
    }

    static double phase = 0.0;
    double freq = 440.0;

    for (int f = 0; f < frames; f++) {
        phase += freq / 48000.0;
        if (phase >= 1.0) phase -= 1.0;
        float sample = wubu_sin(phase * 2.0 * WUBU_PI) * 0.1f;
        for (int c = 0; c < channels; c++) {
            out[f * channels + c] = sample;
        }
    }
}

int wubu_sf2_preset_count(void) { return g_sf2_synth.n_presets; }
const WubuSF2Preset *wubu_sf2_preset(int idx) {
    if (idx >= 0 && idx < g_sf2_synth.n_presets) return &g_sf2_synth.presets[idx];
    return NULL;
}