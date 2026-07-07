/*
 * wubu_audio_chips.c  --  WuBuOS Chip Emulations
 * All retro chip emulations (NES, GB, YM2612, SID, etc.)
 * Extracted from wubu_audio.c for modularity.
 */

#include "wubu_audio_internal.h"

/* ====================================================================
 * GLOBAL CHIP STATES (single instance each)
 * ==================================================================== */

NesApu   g_nes_apu     = {0};
GbApu    g_gb_apu      = {0};
Ym2612   g_ym2612      = {0};
Sn76489  g_sn76489     = {0};
Sid      g_sid         = {0};
Saa1099  g_saa1099     = {0};
Vrc6     g_vrc6        = {0};
N163     g_n163        = {0};
Opl      g_opl         = {0};
Scc      g_scc         = {0};
Ay8910   g_ay8910      = {0};
PcSpeaker g_pc_speaker = {0};

/* ====================================================================
 * CHIP RESET FUNCTIONS
 * ==================================================================== */

void nes_apu_reset(NesApu *a) {
    memset(a, 0, sizeof(NesApu));
    a->pulse[0].duty = 2; a->pulse[1].duty = 2;
    a->noise.period = 4;
    a->dmc.period = 428;
    a->noise.lfsr = 1;
}

void gb_apu_reset(GbApu *g) {
    memset(g, 0, sizeof(GbApu));
    g->ch1.duty = 2;
    g->ch2.duty = 2;
    g->ch4.lfsr = 0x7FFF;
}

void ym2612_reset(Ym2612 *y) {
    memset(y, 0, sizeof(Ym2612));
}

void sn76489_reset(Sn76489 *s) {
    memset(s, 0, sizeof(Sn76489));
    s->noise.lfsr = 0x8000;
}

void sid_reset(Sid *s) {
    memset(s, 0, sizeof(Sid));
    for (int i = 0; i < 3; i++) {
        s->voice[i].waveform = 0x10;
    }
    s->fc = 0x800;
    s->res = 0x0F;
}

void saa1099_reset(Saa1099 *s) {
    memset(s, 0, sizeof(Saa1099));
    s->noise_lfsr = 0xFFFF;
}

void vrc6_reset(Vrc6 *v) {
    memset(v, 0, sizeof(Vrc6));
}

void n163_reset(N163 *n) {
    memset(n, 0, sizeof(N163));
    n->num_ch = 1;
}

void opl_reset(Opl *o) {
    memset(o, 0, sizeof(Opl));
    o->mode = 1;
}

void scc_reset(Scc *s) {
    memset(s, 0, sizeof(Scc));
}

void ay8910_reset(Ay8910 *a) {
    memset(a, 0, sizeof(Ay8910));
    a->noise.lfsr = 1;
}

void pc_speaker_reset(PcSpeaker *p) {
    memset(p, 0, sizeof(PcSpeaker));
}

/* ====================================================================
 * CHIP RENDER FUNCTIONS
 * ==================================================================== */

float nes_apu_render(NesApu *a, double dt) {
    float out = 0.0f;
    double sr = 48000.0;

    for (int i = 0; i < 2; i++) {
        if (a->pulse[i].length_counter == 0 || (!a->pulse[i].length_halt && a->pulse[i].length_counter == 0)) continue;

        a->pulse[i].phase += (uint32_t)((a->pulse[i].period + 1) * (429545428.0 / sr) * dt * (1ULL<<32));

        uint32_t duty_pos = (a->pulse[i].phase >> 28) & 7;
        uint8_t duty = a->pulse[i].duty & 3;
        static const uint8_t duty_seq[4][8] = {
            {1,0,0,0,0,0,0,0},
            {1,1,0,0,0,0,0,0},
            {1,1,1,1,0,0,0,0},
            {0,0,1,1,1,1,1,1},
        };
        bool duty_val = duty_seq[duty][duty_pos];

        if (duty_val) out += a->pulse[i].volume / 15.0f;
    }

    if (a->triangle.length_counter > 0 && a->triangle.period > 0) {
        a->triangle.phase += (uint32_t)((a->triangle.period + 1) * (429545428.0 / sr) * dt * (1ULL<<32));
        uint8_t tri_out = (a->triangle.phase >> 29) & 0x1F;
        if (tri_out > 15) tri_out = 31 - tri_out;
        out += tri_out / 15.0f * 0.5f;
    }

    if (a->noise.length_counter > 0) {
        a->noise.phase += (uint32_t)(a->noise.period * (429545428.0 / sr) * dt * (1ULL<<32));
        if ((a->noise.phase >> 31) & 1) {
            a->noise.lfsr ^= a->noise.loop_noise ? 0x4000 : 0x0001;
        }
        if ((a->noise.lfsr & 1) == 0) out += a->noise.volume / 15.0f * 0.3f;
    }

    return out * 0.1f;
}

float gb_apu_render(GbApu *g, double dt) {
    float out = 0.0f;
    double sr = 48000.0;

    if (g->ch1.dac_enabled && g->ch1.length_counter > 0) {
        g->ch1.phase += (uint32_t)((2048 - g->ch1.period) * (4194304.0 / sr) * dt * (1ULL<<32));
        uint8_t duty_pos = (g->ch1.phase >> 29) & 7;
        static const uint8_t duty_table[4][8] = {
            {0,0,0,0,0,0,0,1}, {1,0,0,0,0,0,0,1}, {1,0,0,0,0,1,1,1}, {0,1,1,1,1,1,1,0}
        };
        if (duty_table[g->ch1.duty][duty_pos]) out += g->ch1.volume / 15.0f;
    }

    if (g->ch2.dac_enabled && g->ch2.length_counter > 0) {
        g->ch2.phase += (uint32_t)((2048 - g->ch2.period) * (4194304.0 / sr) * dt * (1ULL<<32));
        uint8_t duty_pos = (g->ch2.phase >> 29) & 7;
        static const uint8_t duty_table[4][8] = {
            {0,0,0,0,0,0,0,1}, {1,0,0,0,0,0,0,1}, {1,0,0,0,0,1,1,1}, {0,1,1,1,1,1,1,0}
        };
        if (duty_table[g->ch2.duty][duty_pos]) out += g->ch2.volume / 15.0f;
    }

    if (g->ch3.dac_enabled && g->ch3.length_counter > 0) {
        g->ch3.phase += (uint32_t)((2048 - g->ch3.period) * (4194304.0 / sr) * dt * (1ULL<<32));
        g->ch3.wave_pos = (g->ch3.phase >> 28) & 31;
        uint8_t sample = g->ch3.wave_ram[g->ch3.wave_pos >> 1];
        if (g->ch3.wave_pos & 1) sample >>= 4;
        sample &= 0xF;
        if (g->ch3.volume_shift <= 3) out += (sample >> g->ch3.volume_shift) / 15.0f * 0.5f;
    }

    if (g->ch4.dac_enabled && g->ch4.length_counter > 0) {
        uint32_t divisor = g->ch4.divisor_code ? (g->ch4.divisor_code * 16) : 8;
        divisor <<= g->ch4.clock_shift;
        static uint32_t noise_phase = 0;
        noise_phase += (uint32_t)((4194304.0 / divisor) * dt * (1ULL<<32));
        if ((noise_phase >> 31) & 1) {
            uint16_t bit = (g->ch4.lfsr & 1) ^ ((g->ch4.lfsr >> (g->ch4.width_mode ? 6 : 1)) & 1);
            g->ch4.lfsr = (g->ch4.lfsr >> 1) | (bit << 14);
        }
        if ((g->ch4.lfsr & 1) == 0) out += g->ch4.volume / 15.0f * 0.3f;
    }

    return out * 0.1f;
}

static int32_t fm_op_calc(Ym2612 *y, int ch_idx, int op_idx, int32_t mod_input, double dt) {
    uint32_t freq = y->ch[ch_idx].freq * (y->ch[ch_idx].op[op_idx].mul ? y->ch[ch_idx].op[op_idx].mul : 1);
    y->ch[ch_idx].op[op_idx].phase += (uint32_t)(freq * dt * (1ULL<<32));

    if (y->ch[ch_idx].key_on[op_idx]) {
        if (y->ch[ch_idx].op[op_idx].env_state == 0) {
            y->ch[ch_idx].op[op_idx].env_level += 1000.0 * dt;
            if (y->ch[ch_idx].op[op_idx].env_level >= 1024.0) {
                y->ch[ch_idx].op[op_idx].env_level = 1024.0;
                y->ch[ch_idx].op[op_idx].env_state = 1;
            }
        } else if (y->ch[ch_idx].op[op_idx].env_state == 1) {
            y->ch[ch_idx].op[op_idx].env_level -= 50.0 * dt;
            if (y->ch[ch_idx].op[op_idx].env_level <= (y->ch[ch_idx].op[op_idx].sl / 15.0) * 1024.0) {
                y->ch[ch_idx].op[op_idx].env_level = (y->ch[ch_idx].op[op_idx].sl / 15.0) * 1024.0;
                y->ch[ch_idx].op[op_idx].env_state = 2;
            }
        }
    } else {
        if (y->ch[ch_idx].op[op_idx].env_state != 3) y->ch[ch_idx].op[op_idx].env_state = 3;
        y->ch[ch_idx].op[op_idx].env_level -= 20.0 * dt;
        if (y->ch[ch_idx].op[op_idx].env_level <= 0) {
            y->ch[ch_idx].op[op_idx].env_level = 0;
            y->ch[ch_idx].op[op_idx].env_state = 4;
        }
    }

    int32_t phase = (y->ch[ch_idx].op[op_idx].phase >> 24) & 0xFF;
    int32_t sine = (int32_t)(wubu_sin(phase * 2.0 * WUBU_PI / 256.0) * 1024.0);

    return (sine * (y->ch[ch_idx].op[op_idx].env_level / 1024.0));
}

float ym2612_render(Ym2612 *y, double dt) {
    float out = 0.0f;
    for (int c = 0; c < 6; c++) {
        int32_t op_out[4] = {0};

        op_out[3] = fm_op_calc(y, c, 3, 0, dt);
        op_out[2] = fm_op_calc(y, c, 2, op_out[3], dt);
        op_out[1] = fm_op_calc(y, c, 1, op_out[2], dt);
        op_out[0] = fm_op_calc(y, c, 0, op_out[1], dt);

        out += op_out[0] * 0.001f;
    }
    return out * 0.5f;
}

float sn76489_render(Sn76489 *s, double dt) {
    float out = 0.0f;
    double sr = 48000.0;

    for (int i = 0; i < 3; i++) {
        if (s->tone[i].period == 0) continue;
        s->tone[i].phase += (uint32_t)((3579545.0 / (s->tone[i].period * 32)) * dt * (1ULL<<32));
        if ((s->tone[i].phase >> 31) & 1) out += s->tone[i].volume / 15.0f;
    }

    if (s->noise.period > 0) {
        s->noise.phase += (uint32_t)((3579545.0 / (s->noise.period * 16)) * dt * (1ULL<<32));
        if ((s->noise.phase >> 31) & 1) {
            s->noise.lfsr ^= s->noise.white_noise ? 0x0006 : 0x0003;
        }
        if ((s->noise.lfsr & 1) == 0) out += s->noise.volume / 15.0f * 0.3f;
    }

    return out * 0.1f;
}

static float sid_render_voice(Sid *s, int v, double dt) {
    if (s->voice[v].freq == 0) return 0.0f;

    float out = 0.0f;
    double sr = 48000.0;

    s->voice[v].phase += (uint32_t)(s->voice[v].freq * (1024.0 / sr) * dt * (1ULL<<32));
    uint32_t phase = s->voice[v].phase >> 24;

    switch (s->voice[v].waveform & 0xF0) {
        case 0x10:
            out = (phase < 128 ? phase * 2.0f / 255.0f - 1.0f : (255 - phase) * 2.0f / 255.0f - 1.0f);
            break;
        case 0x20:
            out = phase / 127.5f - 1.0f;
            break;
        case 0x40:
            out = (phase < (s->voice[v].pw * 2)) ? 1.0f : -1.0f;
            break;
        case 0x80:
            out = ((phase * 1103515245 + 12345) >> 24) / 127.5f - 1.0f;
            break;
    }

    if (s->voice[v].gate) {
        if (s->voice[v].env_state == 0) {
            s->voice[v].env_level += 1000.0 * dt;
            if (s->voice[v].env_level >= 1024.0) { s->voice[v].env_state = 1; s->voice[v].env_level = 1024.0; }
        } else if (s->voice[v].env_state == 1) {
            s->voice[v].env_level -= 200.0 * dt;
            if (s->voice[v].env_level <= (s->voice[v].sustain/15.0)*1024.0) {
                s->voice[v].env_state = 2; s->voice[v].env_level = (s->voice[v].sustain/15.0)*1024.0;
            }
        }
    } else {
        if (s->voice[v].env_state != 3) s->voice[v].env_state = 3;
        s->voice[v].env_level -= 50.0 * dt;
        if (s->voice[v].env_level <= 0) s->voice[v].env_level = 0;
    }

    return out * (s->voice[v].env_level / 1024.0) * 0.1f;
}

float sid_render(Sid *s, double dt) {
    float out = 0.0f;
    for (int i = 0; i < 3; i++) out += sid_render_voice(s, i, dt);
    return out;
}

float saa1099_render(Saa1099 *s, double dt) {
    float out = 0.0f;
    double sr = 48000.0;
    for (int i = 0; i < 6; i++) {
        if (!(s->ch[i].freq_enable | s->ch[i].noise_enable)) continue;
        s->ch[i].phase += (uint32_t)((s->ch[i].freq + 1) * (8000.0 / sr) * dt * (1ULL<<32));
        bool tone = (s->ch[i].phase >> 31) & 1;
        s->noise_lfsr ^= s->noise_lfsr << 13;
        s->noise_lfsr ^= s->noise_lfsr >> 17;
        bool noise = (s->noise_lfsr & 1) == 0;
        if ((s->ch[i].freq_enable && tone) || (s->ch[i].noise_enable && noise)) {
            out += s->ch[i].level / 15.0f * 0.1f;
        }
    }
    return out;
}

float vrc6_render(Vrc6 *v, double dt) {
    float out = 0.0f;
    double sr = 48000.0;
    for (int i = 0; i < 2; i++) {
        if (v->pulse[i].freq == 0) continue;
        v->pulse[i].phase += (uint32_t)(v->pulse[i].freq * (24576000.0 / sr) * dt * (1ULL<<32));
        uint8_t pos = (v->pulse[i].phase >> 28) & 15;
        if (pos < v->pulse[i].duty) out += v->pulse[i].volume / 15.0f * 0.2f;
    }
    if (v->saw.freq > 0) {
        v->saw.phase += (uint32_t)(v->saw.freq * (24576000.0 / sr) * dt * (1ULL<<32));
        v->saw.accumulator = (v->saw.accumulator + (v->saw.phase >> 24)) & 0x3F;
        out += (v->saw.accumulator - 32) / 32.0f * v->saw.volume / 15.0f * 0.1f;
    }
    return out;
}

float n163_render(N163 *n, double dt) {
    float out = 0.0f;
    double sr = 48000.0;
    for (int i = 0; i < n->num_ch; i++) {
        if (n->ch[i].freq == 0) continue;
        n->ch[i].phase += (uint32_t)(n->ch[i].freq * (21477272.0 / n->num_ch / sr) * dt * (1ULL<<32));
        n->ch[i].wave_pos = (n->ch[i].phase >> 24) % n->ch[i].wave_size;
        out += n->ch[i].wave_ram[n->ch[i].wave_pos] / 128.0f * n->ch[i].volume / 15.0f * 0.1f;
    }
    return out;
}

float opl_render(Opl *o, double dt) {
    (void)dt;
    float out = 0.0f;
    int max_ch = (o->mode >= 2) ? 18 : 9;
    for (int c = 0; c < max_ch; c++) {
        if (!o->ch[c].key_on[0] && !o->ch[c].key_on[1]) continue;
        int32_t env1 = o->ch[c].op[0].env_level;
        int32_t env2 = o->ch[c].op[1].env_level;
        float mod = wubu_sin(o->ch[c].op[1].phase * 2.0 * WUBU_PI / (1ULL<<32)) * env2 * 0.001f;
        float car = wubu_sin((o->ch[c].op[0].phase + (uint32_t)(mod * 1024)) * 2.0 * WUBU_PI / (1ULL<<32)) * env1 * 0.001f;
        out += car;
        o->ch[c].op[0].phase += o->ch[c].freq;
        o->ch[c].op[1].phase += o->ch[c].freq * o->ch[c].op[1].mul;
    }
    return out * 0.1f;
}

float scc_render(Scc *s, double dt) {
    float out = 0.0f;
    double sr = 48000.0;
    for (int i = 0; i < 5; i++) {
        if (s->ch[i].freq == 0) continue;
        s->ch[i].phase += (uint32_t)(s->ch[i].freq * (3579545.0 / sr) * dt * (1ULL<<32));
        uint8_t pos = (s->ch[i].phase >> 26) & 31;
        out += s->ch[i].wave[pos] / 128.0f * s->ch[i].volume / 15.0f * 0.1f;
    }
    return out;
}

float ay8910_render(Ay8910 *a, double dt) {
    float out = 0.0f;
    double sr = 48000.0;
    for (int i = 0; i < 3; i++) {
        if (a->ch[i].period == 0) continue;
        a->ch[i].phase += (uint32_t)((1789772.0 / (a->ch[i].period * 16)) * dt * (1ULL<<32));
        if ((a->ch[i].phase >> 31) & 1) {
            if (a->ch[i].tone) out += a->ch[i].volume / 15.0f;
        }
    }
    if (a->noise.period > 0) {
        a->noise.lfsr += (uint32_t)((1789772.0 / (a->noise.period * 16)) * dt * (1ULL<<32));
        if (a->noise.lfsr & 0x10000) {
            a->noise.lfsr &= 0xFFFF;
            a->noise.lfsr ^= 0x14004;
        }
        for (int i = 0; i < 3; i++) {
            if (a->ch[i].noise && (a->noise.lfsr & 1)) out += a->ch[i].volume / 15.0f * 0.3f;
        }
    }
    return out * 0.1f;
}

float pc_speaker_render(PcSpeaker *p, double dt) {
    if (!p->gate || p->freq == 0) return 0.0f;
    float out = 0.0f;
    double sr = 48000.0;
    p->phase += (uint32_t)(p->freq * (1193180.0 / sr) * dt * (1ULL<<32));
    out = ((p->phase >> 31) & 1) ? 1.0f : -1.0f;
    return out * 0.2f;
}