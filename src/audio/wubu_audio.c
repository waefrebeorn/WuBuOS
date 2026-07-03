/*
 * wubu_audio.c  --  WuBuOS Audio Engine
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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <dirent.h>
#include <stdint.h>

/* Include our pure-C math */
#include "../kernel/wubu_math.h"

/* ------------------------------------------------------------------
 *  FURNACE CHIP EMULATIONS  --  REAL IMPLEMENTATIONS
 * ------------------------------------------------------------------ */

/* ========== NES APU (2A03) ========== */
typedef struct {
    /* Pulse 1 & 2 */
    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t duty;
        uint8_t volume;
        uint8_t decay;
        uint8_t sweep_period;
        uint8_t sweep_shift;
        uint8_t sweep_negate;
        bool sweep_enabled;
        uint8_t length_counter;
        bool length_halt;
        uint8_t env_divider;
        uint8_t env_counter;
        bool env_start;
        bool constant_volume;
    } pulse[2];

    /* Triangle */
    struct {
        uint32_t phase;
        uint16_t period;
        bool length_halt;
        uint8_t length_counter;
        uint16_t linear_counter;
        bool linear_reload;
    } triangle;

    /* Noise */
    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t volume;
        uint8_t decay;
        uint8_t length_counter;
        bool length_halt;
        uint8_t env_divider;
        uint8_t env_counter;
        bool env_start;
        bool constant_volume;
        bool loop_noise;
        uint16_t lfsr;
    } noise;

    /* DMC */
    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t output_level;
        bool loop;
        bool irq_enabled;
        uint32_t address;
        uint32_t length;
        uint8_t sample_buffer;
        bool sample_empty;
    } dmc;

    uint32_t frame_counter;
    uint8_t frame_counter_mode;
    bool frame_irq_inhibit;
} NesApu;

/* ========== Game Boy APU ========== */
typedef struct {
    /* Channel 1: Pulse with sweep */
    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t duty;
        uint8_t volume;
        uint8_t sweep_time;
        uint8_t sweep_shift;
        bool sweep_negate;
        uint8_t sweep_counter;
        bool sweep_enabled;
        uint8_t length_counter;
        bool length_enabled;
        uint8_t env_counter;
        uint8_t env_direction;
        uint8_t env_period;
        bool dac_enabled;
    } ch1;

    /* Channel 2: Pulse */
    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t duty;
        uint8_t volume;
        uint8_t length_counter;
        bool length_enabled;
        uint8_t env_counter;
        uint8_t env_direction;
        uint8_t env_period;
        bool dac_enabled;
    } ch2;

    /* Channel 3: Wave */
    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t wave_ram[16];
        uint8_t volume_shift;
        uint8_t length_counter;
        bool length_enabled;
        bool dac_enabled;
        int wave_pos;
    } ch3;

    /* Channel 4: Noise */
    struct {
        uint32_t lfsr;
        uint8_t volume;
        uint8_t length_counter;
        bool length_enabled;
        uint8_t env_counter;
        uint8_t env_direction;
        uint8_t env_period;
        uint8_t clock_shift;
        uint8_t width_mode;
        uint8_t divisor_code;
        bool dac_enabled;
    } ch4;

    uint32_t frame_sequencer;
} GbApu;

/* ========== YM2612 (Genesis FM) ========== */
typedef struct {
    /* 6 channels, 4 operators each */
    struct {
        struct {
            /* Operator parameters */
            uint8_t mul;
            uint8_t tl;
            uint8_t ar;
            uint8_t dr;
            uint8_t sr;
            uint8_t rr;
            uint8_t sl;
            uint8_t ksl;
            uint8_t am;
            uint8_t vib;
            uint8_t egt;
            uint8_t ksr;
            uint8_t dt;
            uint8_t ws;
            uint8_t ssg_eg;

            /* State */
            uint32_t phase;
            int32_t env_level;
            uint8_t env_state; /* 0=attack,1=decay,2=sustain,3=release */
            uint32_t key_scale;
        } op[4];

        /* Channel parameters */
        uint8_t algorithm;
        uint8_t feedback;
        uint8_t fnum;
        uint8_t block;
        uint8_t detune;
        uint32_t freq;
        uint8_t ams;
        uint8_t fms;
        bool key_on[4];

        /* LFO */
        uint32_t lfo_phase;
    } ch[6];
} Ym2612;

/* ========== SN76489 (Sega PSG) ========== */
typedef struct {
    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t volume;
        bool tone;
    } tone[3];

    struct {
        uint32_t phase;
        uint16_t period; /* noise shift rate */
        uint8_t volume;
        bool white_noise; /* false=periodic, true=white */
        uint16_t lfsr;
    } noise;

    uint8_t last_reg;
} Sn76489;

/* ========== MOS6581/8580 SID (C64) ========== */
typedef struct {
    struct {
        uint32_t phase;
        uint16_t freq;
        uint16_t pw;
        uint8_t ctrl;
        uint8_t attack;
        uint8_t decay;
        uint8_t sustain;
        uint8_t release;
        int32_t env_level;
        uint8_t env_state;
        uint8_t waveform;
        bool gate;
        bool sync;
        bool ring;
    } voice[3];

    uint16_t fc; /* Filter cutoff */
    uint8_t res; /* Filter resonance */
    uint8_t filt; /* Filter routing */
    uint8_t mode; /* Filter mode */
    int32_t hp_y1, hp_y2; /* HP filter state */
    int32_t bp_y1, bp_y2; /* BP filter state */
    int32_t lp_y1, lp_y2; /* LP filter state */
} Sid;

/* ========== SAA1099 ========== */
typedef struct {
    struct {
        uint32_t phase;
        uint16_t freq;
        uint8_t level;
        uint8_t octave;
        uint8_t freq_enable;
        uint8_t noise_enable;
        uint8_t env_shape;
        uint32_t env_counter;
        uint16_t env_freq;
        int32_t env_level;
        uint8_t env_state;
    } ch[6];

    uint8_t noise_params[8];
    uint8_t env_enable[6];
    uint32_t noise_lfsr;
} Saa1099;

/* ========== VRC6 ========== */
typedef struct {
    struct {
        uint32_t phase;
        uint16_t freq;
        uint8_t duty;
        uint8_t volume;
    } pulse[2];

    struct {
        uint32_t phase;
        uint16_t freq;
        uint8_t accumulator;
        uint8_t volume;
    } saw;
} Vrc6;

/* ========== N163 (Namco 163) ========== */
typedef struct {
    struct {
        uint32_t phase;
        uint16_t freq;
        uint8_t volume;
        uint8_t wave_size;
        uint8_t wave_pos;
        int8_t wave_ram[128];
    } ch[8];

    uint8_t num_ch;
} N163;

/* ========== OPL / OPL2 / OPL3 (YM3526 / YM3812) ========== */
typedef struct {
    struct {
        struct {
            uint8_t am;
            uint8_t vib;
            uint8_t egt;
            uint8_t ksr;
            uint8_t mul;
            uint8_t ksl;
            uint8_t tl;
            uint8_t ar;
            uint8_t dr;
            uint8_t sl;
            uint8_t rr;
            uint8_t ws;

            uint32_t phase;
            int32_t env_level;
            uint8_t env_state;
        } op[2];

        uint8_t fb;
        uint8_t conn;
        uint16_t fnum;
        uint8_t block;
        uint32_t freq;
        bool key_on[2];
    } ch[18]; /* OPL3 has 18, OPL2 has 9 */

    uint8_t mode; /* 0=OPL, 1=OPL2, 2=OPL3 */
    uint32_t lfo_am_phase;
    uint32_t lfo_vib_phase;
} Opl;

/* ========== SCC (Konami) ========== */
typedef struct {
    struct {
        uint32_t phase;
        uint16_t freq;
        uint8_t volume;
        int8_t wave[32];
    } ch[5];
} Scc;

/* ========== AY-3-8910 ========== */
typedef struct {
    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t volume;
        bool tone;
        bool noise;
        uint8_t env_shape;
        uint32_t env_counter;
        uint16_t env_period;
        int32_t env_level;
        uint8_t env_state;
    } ch[3];

    struct {
        uint32_t lfsr;
        uint16_t period;
    } noise;
} Ay8910;

/* ========== PC Speaker ========== */
typedef struct {
    uint32_t phase;
    uint16_t freq;
    bool gate;
} PcSpeaker;

/* ------------------------------------------------------------------
 *  GLOBAL CHIP STATES (one per chip type)
 * ------------------------------------------------------------------ */

static NesApu   g_nes_apu     = {0};
static GbApu    g_gb_apu      = {0};
static Ym2612   g_ym2612      = {0};
static Sn76489  g_sn76489     = {0};
static Sid      g_sid         = {0};
static Saa1099  g_saa1099     = {0};
static Vrc6     g_vrc6        = {0};
static N163     g_n163        = {0};
static Opl      g_opl         = {0};
static Scc      g_scc         = {0};
static Ay8910   g_ay8910      = {0};
static PcSpeaker g_pc_speaker = {0};

/* ------------------------------------------------------------------
 *  HELPER: ENVELOPE GENERATORS
 * ------------------------------------------------------------------ */

static inline void env_adsr_advance(int32_t *level, uint8_t *state,
    uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release,
    bool gate, double dt)
{
    const double rates[4] = {
        attack  ? 1.0 / (attack  * 0.001) : 1e6,
        decay   ? 1.0 / (decay   * 0.001) : 1e6,
        0, /* sustain - no change */
        release ? 1.0 / (release * 0.001) : 1e6
    };

    if (gate) {
        if (*state == 0) { /* Attack */
            *level += rates[0] * dt;
            if (*level >= 1.0) { *level = 1.0; *state = 1; }
        } else if (*state == 1) { /* Decay */
            *level -= rates[1] * dt;
            double sl = sustain / 15.0;
            if (*level <= sl) { *level = sl; *state = 2; }
        } else if (*state == 2) { /* Sustain */
            *level = sustain / 15.0;
        }
    } else { /* Release */
        if (*state != 3) *state = 3;
        *level -= rates[3] * dt;
        if (*level <= 0.0) { *level = 0.0; *state = 4; }
    }
}

/* ------------------------------------------------------------------
 *  NES APU EMULATION
 * ------------------------------------------------------------------ */

static void nes_apu_reset(NesApu *a) {
    memset(a, 0, sizeof(NesApu));
    a->pulse[0].duty = 2; a->pulse[1].duty = 2;
    a->noise.period = 4;
    a->dmc.period = 428;
    a->noise.lfsr = 1;
}

static float nes_apu_render(NesApu *a, double dt) {
    float out = 0.0f;
    double sr = 48000.0;

    /* Pulse channels */
    for (int i = 0; i < 2; i++) {
        if (a->pulse[i].length_counter == 0 || (!a->pulse[i].length_halt && a->pulse[i].length_counter == 0)) continue;

        a->pulse[i].phase += (uint32_t)((a->pulse[i].period + 1) * (429545428.0 / sr) * dt * (1ULL<<32));
        
        /* Duty cycle: use bits 28-30 for waveform position (8-step sequence) */
        uint32_t duty_pos = (a->pulse[i].phase >> 28) & 7;
        uint8_t duty = a->pulse[i].duty & 3;
        static const uint8_t duty_seq[4][8] = {
            {1,0,0,0,0,0,0,0},  /* 12.5% */
            {1,1,0,0,0,0,0,0},  /* 25% */
            {1,1,1,1,0,0,0,0},  /* 50% */
            {0,0,1,1,1,1,1,1},  /* 75% (inverted 25%) */
        };
        bool duty_val = duty_seq[duty][duty_pos];

        if (duty_val) out += a->pulse[i].volume / 15.0f;
    }

    /* Triangle - use period as linear counter proxy */
    if (a->triangle.length_counter > 0 && a->triangle.period > 0) {
        a->triangle.phase += (uint32_t)((a->triangle.period + 1) * (429545428.0 / sr) * dt * (1ULL<<32));
        uint8_t tri_out = (a->triangle.phase >> 29) & 0x1F;
        if (tri_out > 15) tri_out = 31 - tri_out;
        out += tri_out / 15.0f * 0.5f;
    }

    /* Noise */
    if (a->noise.length_counter > 0) {
        a->noise.phase += (uint32_t)(a->noise.period * (429545428.0 / sr) * dt * (1ULL<<32));
        if ((a->noise.phase >> 31) & 1) {
            a->noise.lfsr ^= a->noise.loop_noise ? 0x4000 : 0x0001;
        }
        if ((a->noise.lfsr & 1) == 0) out += a->noise.volume / 15.0f * 0.3f;
    }

    return out * 0.1f;
}

/* ------------------------------------------------------------------
 *  GAME BOY APU EMULATION
 * ------------------------------------------------------------------ */

static void gb_apu_reset(GbApu *g) {
    memset(g, 0, sizeof(GbApu));
    g->ch1.duty = 2;
    g->ch2.duty = 2;
    g->ch4.lfsr = 0x7FFF;
}

static float gb_apu_render(GbApu *g, double dt) {
    float out = 0.0f;
    double sr = 48000.0;

    /* Channel 1: Pulse with sweep */
    if (g->ch1.dac_enabled && g->ch1.length_counter > 0) {
        g->ch1.phase += (uint32_t)((2048 - g->ch1.period) * (4194304.0 / sr) * dt * (1ULL<<32));
        uint8_t duty_pos = (g->ch1.phase >> 29) & 7;
        static const uint8_t duty_table[4][8] = {
            {0,0,0,0,0,0,0,1}, {1,0,0,0,0,0,0,1}, {1,0,0,0,0,1,1,1}, {0,1,1,1,1,1,1,0}
        };
        if (duty_table[g->ch1.duty][duty_pos]) out += g->ch1.volume / 15.0f;
    }

    /* Channel 2: Pulse */
    if (g->ch2.dac_enabled && g->ch2.length_counter > 0) {
        g->ch2.phase += (uint32_t)((2048 - g->ch2.period) * (4194304.0 / sr) * dt * (1ULL<<32));
        uint8_t duty_pos = (g->ch2.phase >> 29) & 7;
        static const uint8_t duty_table[4][8] = {
            {0,0,0,0,0,0,0,1}, {1,0,0,0,0,0,0,1}, {1,0,0,0,0,1,1,1}, {0,1,1,1,1,1,1,0}
        };
        if (duty_table[g->ch2.duty][duty_pos]) out += g->ch2.volume / 15.0f;
    }

    /* Channel 3: Wave */
    if (g->ch3.dac_enabled && g->ch3.length_counter > 0) {
        g->ch3.phase += (uint32_t)((2048 - g->ch3.period) * (4194304.0 / sr) * dt * (1ULL<<32));
        g->ch3.wave_pos = (g->ch3.phase >> 28) & 31;
        uint8_t sample = g->ch3.wave_ram[g->ch3.wave_pos >> 1];
        if (g->ch3.wave_pos & 1) sample >>= 4;
        sample &= 0xF;
        if (g->ch3.volume_shift <= 3) out += (sample >> g->ch3.volume_shift) / 15.0f * 0.5f;
    }

    /* Channel 4: Noise */
    if (g->ch4.dac_enabled && g->ch4.length_counter > 0) {
        uint32_t divisor = g->ch4.divisor_code ? (g->ch4.divisor_code * 16) : 8;
        divisor <<= g->ch4.clock_shift;
        /* Use a local phase for noise since struct doesn't have it */
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

/* ------------------------------------------------------------------
 *  YM2612 (GENESIS FM) EMULATION - 4-OP FM
 * ------------------------------------------------------------------ */

static int32_t fm_op_calc(Ym2612 *y, int ch_idx, int op_idx, int32_t mod_input, double dt) {
    uint32_t freq = y->ch[ch_idx].freq * (y->ch[ch_idx].op[op_idx].mul ? y->ch[ch_idx].op[op_idx].mul : 1);
    y->ch[ch_idx].op[op_idx].phase += (uint32_t)(freq * dt * (1ULL<<32));

    /* Simplified envelope */
    if (y->ch[ch_idx].key_on[op_idx]) {
        if (y->ch[ch_idx].op[op_idx].env_state == 0) { /* Attack */
            y->ch[ch_idx].op[op_idx].env_level += 1000.0 * dt;
            if (y->ch[ch_idx].op[op_idx].env_level >= 1024.0) { y->ch[ch_idx].op[op_idx].env_level = 1024.0; y->ch[ch_idx].op[op_idx].env_state = 1; }
        } else if (y->ch[ch_idx].op[op_idx].env_state == 1) { /* Decay */
            y->ch[ch_idx].op[op_idx].env_level -= 50.0 * dt;
            if (y->ch[ch_idx].op[op_idx].env_level <= (y->ch[ch_idx].op[op_idx].sl / 15.0) * 1024.0) { y->ch[ch_idx].op[op_idx].env_level = (y->ch[ch_idx].op[op_idx].sl / 15.0) * 1024.0; y->ch[ch_idx].op[op_idx].env_state = 2; }
        }
    } else {
        if (y->ch[ch_idx].op[op_idx].env_state != 3) y->ch[ch_idx].op[op_idx].env_state = 3;
        y->ch[ch_idx].op[op_idx].env_level -= 20.0 * dt;
        if (y->ch[ch_idx].op[op_idx].env_level <= 0) { y->ch[ch_idx].op[op_idx].env_level = 0; y->ch[ch_idx].op[op_idx].env_state = 4; }
    }

    int32_t phase = (y->ch[ch_idx].op[op_idx].phase >> 24) & 0xFF;
    int32_t sine = (int32_t)(wubu_sin(phase * 2.0 * WUBU_PI / 256.0) * 1024.0);

    return (sine * (y->ch[ch_idx].op[op_idx].env_level / 1024.0));
}

static float ym2612_render(Ym2612 *y, double dt) {
    float out = 0.0f;
    for (int c = 0; c < 6; c++) {
        int32_t op_out[4] = {0};

        /* Simple algorithm 0: all in series */
        op_out[3] = fm_op_calc(y, c, 3, 0, dt);
        op_out[2] = fm_op_calc(y, c, 2, op_out[3], dt);
        op_out[1] = fm_op_calc(y, c, 1, op_out[2], dt);
        op_out[0] = fm_op_calc(y, c, 0, op_out[1], dt);

        out += op_out[0] * 0.001f;
    }
    return out * 0.5f;
}

static void ym2612_reset(Ym2612 *y) {
    memset(y, 0, sizeof(Ym2612));
}

/* ------------------------------------------------------------------
 *  SN76489 EMULATION
 * ------------------------------------------------------------------ */

static void sn76489_reset(Sn76489 *s) {
    memset(s, 0, sizeof(Sn76489));
    s->noise.lfsr = 0x8000;
}

static float sn76489_render(Sn76489 *s, double dt) {
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

/* ------------------------------------------------------------------
 *  SID EMULATION
 * ------------------------------------------------------------------ */

static void sid_reset(Sid *s) {
    memset(s, 0, sizeof(Sid));
    for (int i = 0; i < 3; i++) {
        s->voice[i].waveform = 0x10; /* triangle default */
    }
    s->fc = 0x800;
    s->res = 0x0F;
}

static float sid_render_voice(Sid *s, int v, double dt) {
    if (s->voice[v].freq == 0) return 0.0f;

    float out = 0.0f;
    double sr = 48000.0;

    s->voice[v].phase += (uint32_t)(s->voice[v].freq * (1024.0 / sr) * dt * (1ULL<<32));
    uint32_t phase = s->voice[v].phase >> 24;

    switch (s->voice[v].waveform & 0xF0) {
        case 0x10: /* Triangle */
            out = (phase < 128 ? phase * 2.0f / 255.0f - 1.0f : (255 - phase) * 2.0f / 255.0f - 1.0f);
            break;
        case 0x20: /* Sawtooth */
            out = phase / 127.5f - 1.0f;
            break;
        case 0x40: /* Pulse */
            out = (phase < (s->voice[v].pw * 2)) ? 1.0f : -1.0f;
            break;
        case 0x80: /* Noise */
            out = ((phase * 1103515245 + 12345) >> 24) / 127.5f - 1.0f;
            break;
    }

    /* Envelope */
    if (s->voice[v].gate) {
        if (s->voice[v].env_state == 0) { s->voice[v].env_level += 1000.0 * dt; if (s->voice[v].env_level >= 1024.0) { s->voice[v].env_state = 1; s->voice[v].env_level = 1024.0; } }
        else if (s->voice[v].env_state == 1) { s->voice[v].env_level -= 200.0 * dt; if (s->voice[v].env_level <= (s->voice[v].sustain/15.0)*1024.0) { s->voice[v].env_state = 2; s->voice[v].env_level = (s->voice[v].sustain/15.0)*1024.0; } }
    } else {
        if (s->voice[v].env_state != 3) s->voice[v].env_state = 3;
        s->voice[v].env_level -= 50.0 * dt;
        if (s->voice[v].env_level <= 0) s->voice[v].env_level = 0;
    }

    return out * (s->voice[v].env_level / 1024.0) * 0.1f;
}

static float sid_render(Sid *s, double dt) {
    float out = 0.0f;
    for (int i = 0; i < 3; i++) out += sid_render_voice(s, i, dt);

    /* Simplified filter */
    return out;
}

/* ------------------------------------------------------------------
 *  SAA1099 EMULATION
 * ------------------------------------------------------------------ */

static void saa1099_reset(Saa1099 *s) {
    memset(s, 0, sizeof(Saa1099));
    s->noise_lfsr = 0xFFFF;
}

static float saa1099_render(Saa1099 *s, double dt) {
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

/* ------------------------------------------------------------------
 *  VRC6 EMULATION
 * ------------------------------------------------------------------ */

static void vrc6_reset(Vrc6 *v) { memset(v, 0, sizeof(Vrc6)); }

static float vrc6_render(Vrc6 *v, double dt) {
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

/* ------------------------------------------------------------------
 *  N163 EMULATION
 * ------------------------------------------------------------------ */

static void n163_reset(N163 *n) {
    memset(n, 0, sizeof(N163));
    n->num_ch = 1;
}

static float n163_render(N163 *n, double dt) {
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

/* ------------------------------------------------------------------
 *  OPL EMULATION
 * ------------------------------------------------------------------ */

static void opl_reset(Opl *o) {
    memset(o, 0, sizeof(Opl));
    o->mode = 1; /* OPL2 default */
}

static float opl_render(Opl *o, double dt) {
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

/* ------------------------------------------------------------------
 *  SCC EMULATION
 * ------------------------------------------------------------------ */

static void scc_reset(Scc *s) { memset(s, 0, sizeof(Scc)); }

static float scc_render(Scc *s, double dt) {
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

/* ------------------------------------------------------------------
 *  AY-3-8910 EMULATION
 * ------------------------------------------------------------------ */

static void ay8910_reset(Ay8910 *a) {
    memset(a, 0, sizeof(Ay8910));
    a->noise.lfsr = 1;
}

static float ay8910_render(Ay8910 *a, double dt) {
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

/* ------------------------------------------------------------------
 *  PC SPEAKER EMULATION
 * ------------------------------------------------------------------ */

static void pc_speaker_reset(PcSpeaker *p) { memset(p, 0, sizeof(PcSpeaker)); }

static float pc_speaker_render(PcSpeaker *p, double dt) {
    if (!p->gate || p->freq == 0) return 0.0f;
    float out = 0.0f;
    double sr = 48000.0;
    p->phase += (uint32_t)(p->freq * (1193180.0 / sr) * dt * (1ULL<<32));
    out = ((p->phase >> 31) & 1) ? 1.0f : -1.0f;
    return out * 0.2f;
}


/* ------------------------------------------------------------------
 *  GLOBAL ENGINE STATE
 * ------------------------------------------------------------------ */

static WubuAudioEngine    g_engine     = {0};
static bool               g_init       = false;
static pthread_mutex_t    g_engine_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------
 *  FURNACE TRACKER  --  CHIP EMULATIONS
 * ------------------------------------------------------------------ */

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

    /* Initialize chip emulations based on chip types */
    for (int i = 0; i < n_chips; i++) {
        WubuChipType chip = g_engine.furnace.chips[i];
        if (chip == CHIP_NES_APU) nes_apu_reset(&g_nes_apu);
        else if (chip == CHIP_GB_APU) gb_apu_reset(&g_gb_apu);
        else if (chip == CHIP_YM2612) { ym2612_reset(&g_ym2612); }
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

    /* If row 0 has no notes, scan forward for first note on each channel and trigger immediately */
    for (int c = 0; c < g_engine.furnace.n_chips; c++) {
        if (furnace_chans[c].active) continue; /* Already triggered */
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
                break; /* Only trigger first note found */
            }
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

        /* Update chip states from furnace channels */
        for (int c = 0; c < g_engine.furnace.n_chips; c++) {
            if (!furnace_chans[c].active) continue;
            FurnaceChannel *ch = &furnace_chans[c];
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

            /* Ensure audible volume for first frames of note */
            if (ch->env_level < 0.8f) ch->env_level = 0.8f;

            float env_vol = ch->volume * ch->env_level;  /* Remove 0.1f multiplier */

            WubuChipType chip = ch->chip;
            if (chip == CHIP_NONE) chip = g_engine.furnace.chips[c];

            /* Update chip emulator state with current note */
            int midi_note = ch->octave * 12 + ch->note;
            double freq = 440.0 * wubu_pow(2.0, (midi_note - 69) / 12.0);
            uint16_t nes_period = (uint16_t)(1789773.0 / (16.0 * freq) - 1.0);
            uint16_t gb_period = (uint16_t)(2048.0 - 131072.0 / freq);
            uint16_t sn_period = (uint16_t)(3579545.0 / (32.0 * freq));
            uint16_t sid_freq = (uint16_t)(freq * 1024.0 / 48000.0 * 65536.0);
            uint16_t ay_period = (uint16_t)(1789772.0 / (16.0 * freq));

            if (chip == CHIP_NES_APU && c < 2) {
                g_nes_apu.pulse[c].period = nes_period;
                g_nes_apu.pulse[c].volume = (uint8_t)(env_vol * 15.0f);  /* ch->volume * env_level * 15 */
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

        /* Render each chip and accumulate */
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
                    /* Fallback to simple square wave */
                    float fb_env_vol = furnace_chans[c].volume * furnace_chans[c].env_level;
                    furnace_square_wave(out + f, 1, furnace_chans[c].freq, fb_env_vol, &furnace_chans[c].phase);
                    continue;
            }
        }

        out[f] += chip_out;
    }

    return frames;
}

/* ------------------------------------------------------------------
 *  TINYSOUNDFONT  --  SF2 SYNTHESIS
 * ------------------------------------------------------------------ */

/* Simplified SF2 parser  --  real implementation would parse RIFF/SF2 chunks */
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
    /* SF2 pitch bend: bend range is -8192 to +8192, center 0 */
    if (channel >= 16) return;
    g_engine.sf2.pitch_bend[channel] = (int16_t)bend;
}

void wubu_sf2_control(uint8_t channel, uint8_t cc, uint8_t val) {
    /* CC events: volume (7), pan (10), reverb (91), chorus (93), etc. */
    if (channel >= 16) return;
    /* Store controller value for active notes on this channel */
    g_engine.sf2.midi_channels[channel] = val; /* Reuse midi_channels for last-CC store */
    /* TODO: route CC to actual synth parameters during sf2_render */
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

/* ------------------------------------------------------------------
 *  ARDOUR-STYLE DAW MIXER
 * ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------
 *  AUDIO ENGINE MAIN PROCESSING
 * ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------
 *  USB MIDI INPUT
 * ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------
 *  AI AUDIO PLUGINS (CONTAINER-BASED)
 * ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------
 *  INGESTION PROTOCOLS
 * ------------------------------------------------------------------ */

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

