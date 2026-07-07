/*
 * wubu_audio_furnace.c  --  WuBuOS Furnace Tracker
 * Chip tracker with pattern editor, Furnace-style.
 * Extracted from wubu_audio.c for modularity.
 */

#include "wubu_audio_internal.h"

/* ====================================================================
 * FURNACE CHANNEL STATE (global)
 * ==================================================================== */

FurnaceChannel furnace_chans[WUBU_AUDIO_MAX_FURNACE_CHANS];

/* ====================================================================
 * FURNACE TRACKER API
 * ==================================================================== */

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

    for (int i = 0; i < n_chips; i++) {
        WubuChipType chip = g_engine.furnace.chips[i];
        if (chip == CHIP_NES_APU) nes_apu_reset(&g_nes_apu);
        else if (chip == CHIP_GB_APU) gb_apu_reset(&g_gb_apu);
        else if (chip == CHIP_YM2612) ym2612_reset(&g_ym2612);
        else if (chip == CHIP_SN76489) sn76489_reset(&g_sn76489);
        else if (chip == CHIP_SID || chip == CHIP_C64) sid_reset(&g_sid);
        else if (chip == CHIP_SAA1099) saa1099_reset(&g_saa1099);
        else if (chip == CHIP_VRC6) vrc6_reset(&g_vrc6);
        else if (chip == CHIP_N163) n163_reset(&g_n163);
        else if (chip == CHIP_OPL || chip == CHIP_OPL2 || chip == CHIP_OPL3) opl_reset(&g_opl);
        else if (chip == CHIP_SCC) scc_reset(&g_scc);
        else if (chip == CHIP_AY8910) ay8910_reset(&g_ay8910);
        else if (chip == CHIP_PCSPEAKER) pc_speaker_reset(&g_pc_speaker);
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

void wubu_furnace_set_volume(int pattern, int row, int chan, uint8_t volume) {
    if (pattern < WUBU_AUDIO_MAX_PATTERNS && row < 256 && chan < WUBU_AUDIO_MAX_FURNACE_CHANS) {
        WubuFurnacePattern *p = &g_engine.furnace.patterns[pattern];
        if (pattern >= g_engine.furnace.n_patterns) g_engine.furnace.n_patterns = pattern + 1;
        p->rows[row].cells[chan].volume = volume;
        if (row >= p->n_rows) p->n_rows = row + 1;
    }
}

void wubu_furnace_play(void) {
    g_engine.furnace.playing = true;
    g_engine.furnace.current_pattern = 0;
    g_engine.furnace.current_row = 0;
    g_engine.furnace.tick_accum = 0.0;

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
            furnace_chans[i].env_stage = 3;
        }
    }
    printf("[audio] Furnace: STOP\n");
}

void wubu_furnace_set_tempo(int tempo) {
    if (tempo > 20 && tempo < 400) g_engine.furnace.tempo = tempo;
}

int wubu_furnace_render_pattern(int pattern, float *out, int frames) {
    if (pattern >= g_engine.furnace.n_patterns) return -1;

    WubuFurnacePattern *p = &g_engine.furnace.patterns[pattern];
    if (!p->n_rows) return 0;

    memset(out, 0, frames * sizeof(float));

    double rows_per_second = g_engine.furnace.tempo / 60.0 * g_engine.furnace.speed;
    double frame_duration = 1.0 / 48000.0;
    double row_duration = 1.0 / rows_per_second;

    int current_row = 0;
    double row_time = 0.0;

    for (int c = 0; c < g_engine.furnace.n_chips; c++) {
        WubuTrackerCell *cell = &p->rows[0].cells[c];
        if (cell->note != 255) {
            furnace_chans[c].note = cell->note;
            furnace_chans[c].octave = cell->octave;
            furnace_chans[c].instrument = cell->instrument;
            furnace_chans[c].volume = cell->volume / 15.0;
            furnace_chans[c].freq = note_to_freq(cell->note, cell->octave);
            furnace_chans[c].env_stage = 0;
            furnace_chans[c].env_level = 0.0;
            furnace_chans[c].active = true;
        }
    }

    for (int c = 0; c < g_engine.furnace.n_chips; c++) {
        if (furnace_chans[c].active) continue;
        for (int r = 1; r < p->n_rows; r++) {
            WubuTrackerCell *cell = &p->rows[r].cells[c];
            if (cell->note != 255) {
                furnace_chans[c].note = cell->note;
                furnace_chans[c].octave = cell->octave;
                furnace_chans[c].instrument = cell->instrument;
                furnace_chans[c].volume = cell->volume / 15.0;
                furnace_chans[c].freq = note_to_freq(cell->note, cell->octave);
                furnace_chans[c].env_stage = 0;
                furnace_chans[c].env_level = 0.0;
                furnace_chans[c].active = true;
                break;
            }
        }
    }

    for (int f = 0; f < frames; f++) {
        row_time += frame_duration;
        if (row_time >= row_duration) {
            row_time -= row_duration;
            current_row = (current_row + 1) % p->n_rows;

            for (int c = 0; c < g_engine.furnace.n_chips; c++) {
                WubuTrackerCell *cell = &p->rows[current_row].cells[c];
                if (cell->note != 255) {
                    furnace_chans[c].note = cell->note;
                    furnace_chans[c].octave = cell->octave;
                    furnace_chans[c].instrument = cell->instrument;
                    furnace_chans[c].volume = cell->volume / 15.0;
                    furnace_chans[c].freq = note_to_freq(cell->note, cell->octave);
                    furnace_chans[c].env_stage = 0;
                    furnace_chans[c].env_level = 0.0;
                    furnace_chans[c].active = true;
                }
            }
        }

        for (int c = 0; c < g_engine.furnace.n_chips; c++) {
            if (!furnace_chans[c].active) continue;
            FurnaceChannel *ch = &furnace_chans[c];

            if (ch->env_stage == 0) {
                ch->env_level += frame_duration / ch->env_attack;
                if (ch->env_level >= 1.0) {
                    ch->env_level = 1.0;
                    ch->env_stage = 1;
                }
            } else if (ch->env_stage == 1) {
                ch->env_level -= frame_duration / ch->env_decay;
                if (ch->env_level <= ch->env_sustain) {
                    ch->env_level = ch->env_sustain;
                    ch->env_stage = 2;
                }
            } else if (ch->env_stage == 3) {
                ch->env_level -= frame_duration / ch->env_release;
                if (ch->env_level <= 0.0) {
                    ch->env_level = 0.0;
                    ch->active = false;
                    continue;
                }
            }

            if (ch->env_level < 0.8f) ch->env_level = 0.8f;

            float env_vol = ch->volume * ch->env_level;

            WubuChipType chip = ch->chip;
            if (chip == CHIP_NONE) chip = g_engine.furnace.chips[c];

            int midi_note = ch->octave * 12 + ch->note;
            double freq = 440.0 * wubu_pow(2.0, (midi_note - 69) / 12.0);
            uint16_t nes_period = (uint16_t)(1789773.0 / (16.0 * freq) - 1.0);
            uint16_t gb_period = (uint16_t)(2048.0 - 131072.0 / freq);
            uint16_t sn_period = (uint16_t)(3579545.0 / (32.0 * freq));
            uint16_t sid_freq = (uint16_t)(freq * 1024.0 / 48000.0 * 65536.0);
            uint16_t ay_period = (uint16_t)(1789772.0 / (16.0 * freq));

            if (chip == CHIP_NES_APU && c < 2) {
                g_nes_apu.pulse[c].period = nes_period;
                g_nes_apu.pulse[c].volume = (uint8_t)(env_vol * 15.0f);
                g_nes_apu.pulse[c].length_counter = 64;
                g_nes_apu.pulse[c].length_halt = false;
            } else if (chip == CHIP_GB_APU && c < 2) {
                g_gb_apu.ch1.period = gb_period;
                g_gb_apu.ch1.volume = (uint8_t)(env_vol * 15);
                g_gb_apu.ch1.length_counter = 64;
                g_gb_apu.ch1.dac_enabled = true;
            } else if (chip == CHIP_YM2612 && c < 6) {
                g_ym2612.ch[c].freq = (uint32_t)(freq * 65536.0 / 48000.0 * (1ULL<<16));
                g_ym2612.ch[c].key_on[0] = true;
                g_ym2612.ch[c].op[0].env_state = 0;
            } else if (chip == CHIP_SN76489 && c < 3) {
                g_sn76489.tone[c].period = sn_period;
                g_sn76489.tone[c].volume = (uint8_t)(env_vol * 15);
            } else if ((chip == CHIP_SID || chip == CHIP_C64) && c < 3) {
                g_sid.voice[c].freq = sid_freq;
                g_sid.voice[c].gate = true;
                g_sid.voice[c].env_state = 0;
            } else if (chip == CHIP_SAA1099 && c < 6) {
                g_saa1099.ch[c].freq = (uint16_t)(freq / 1000.0 * 65536.0);
                g_saa1099.ch[c].level = (uint8_t)(env_vol * 15);
                g_saa1099.ch[c].freq_enable = 1;
            } else if (chip == CHIP_VRC6 && c < 2) {
                g_vrc6.pulse[c].freq = (uint16_t)(freq * 65536.0 / 48000.0 * 100.0);
                g_vrc6.pulse[c].volume = (uint8_t)(env_vol * 15);
            } else if (chip == CHIP_N163 && c < 8) {
                g_n163.ch[c].freq = (uint16_t)(freq * 65536.0 / 48000.0 * 100.0);
                g_n163.ch[c].volume = (uint8_t)(env_vol * 15);
            } else if ((chip == CHIP_OPL || chip == CHIP_OPL2 || chip == CHIP_OPL3) && c < 9) {
                g_opl.ch[c].freq = (uint32_t)(freq * 65536.0 / 48000.0);
                g_opl.ch[c].key_on[0] = true;
            } else if (chip == CHIP_SCC && c < 5) {
                g_scc.ch[c].freq = (uint32_t)(freq * 65536.0 / 48000.0);
                g_scc.ch[c].volume = (uint8_t)(env_vol * 15);
            } else if (chip == CHIP_AY8910 && c < 3) {
                g_ay8910.ch[c].period = ay_period;
                g_ay8910.ch[c].volume = (uint8_t)(env_vol * 15);
                g_ay8910.ch[c].tone = true;
            } else if (chip == CHIP_PCSPEAKER) {
                g_pc_speaker.freq = (uint16_t)(freq * 65536.0 / 48000.0);
                g_pc_speaker.gate = true;
            }
        }

        float chip_out = 0.0f;
        for (int c = 0; c < g_engine.furnace.n_chips; c++) {
            WubuChipType chip = furnace_chans[c].chip;
            if (chip == CHIP_NONE) chip = g_engine.furnace.chips[c];

            float vol = furnace_chans[c].volume * 0.1f;
            float env_vol = vol * furnace_chans[c].env_level;

            switch (chip) {
                case CHIP_NES_APU:
                    if (c == 0) chip_out += nes_apu_render(&g_nes_apu, frame_duration);
                    break;
                case CHIP_GB_APU:
                    if (c == 0) chip_out += gb_apu_render(&g_gb_apu, frame_duration);
                    break;
                case CHIP_YM2612:
                    if (c == 0) chip_out += ym2612_render(&g_ym2612, frame_duration);
                    break;
                case CHIP_SN76489:
                    if (c == 0) chip_out += sn76489_render(&g_sn76489, frame_duration);
                    break;
                case CHIP_SID:
                case CHIP_C64:
                    if (c == 0) chip_out += sid_render(&g_sid, frame_duration);
                    break;
                case CHIP_SAA1099:
                    if (c == 0) chip_out += saa1099_render(&g_saa1099, frame_duration);
                    break;
                case CHIP_VRC6:
                    if (c == 0) chip_out += vrc6_render(&g_vrc6, frame_duration);
                    break;
                case CHIP_N163:
                    if (c == 0) chip_out += n163_render(&g_n163, frame_duration);
                    break;
                case CHIP_OPL:
                case CHIP_OPL2:
                case CHIP_OPL3:
                    if (c == 0) chip_out += opl_render(&g_opl, frame_duration);
                    break;
                case CHIP_SCC:
                    if (c == 0) chip_out += scc_render(&g_scc, frame_duration);
                    break;
                case CHIP_AY8910:
                    if (c == 0) chip_out += ay8910_render(&g_ay8910, frame_duration);
                    break;
                case CHIP_PCSPEAKER:
                    if (c == 0) chip_out += pc_speaker_render(&g_pc_speaker, frame_duration);
                    break;
                default:
                    float fb_env_vol = furnace_chans[c].volume * furnace_chans[c].env_level;
                    furnace_square_wave(out + f, 1, furnace_chans[c].freq, fb_env_vol, &furnace_chans[c].phase);
                    continue;
            }
        }

        out[f] += chip_out;
    }

    return frames;
}