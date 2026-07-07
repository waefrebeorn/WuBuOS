/*
 * wubu_audio_types.h  --  WuBuOS Audio Engine Private Type Definitions
 * Chip struct definitions and shared types for audio modules (internal only).
 * These are NOT in the public API - only for internal module sharing.
 */

#ifndef WUBU_AUDIO_TYPES_H
#define WUBU_AUDIO_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ====================================================================
 * CHIP EMULATION STRUCTS (private implementations, internal use only)
 * ==================================================================== */

/* ---------- NES APU (2A03) ---------- */
typedef struct {
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

    struct {
        uint32_t phase;
        uint16_t period;
        bool length_halt;
        uint8_t length_counter;
        uint16_t linear_counter;
        bool linear_reload;
    } triangle;

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

/* ---------- Game Boy APU ---------- */
typedef struct {
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

/* ---------- YM2612 (Genesis FM) ---------- */
typedef struct {
    struct {
        struct {
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

            uint32_t phase;
            int32_t env_level;
            uint8_t env_state;
            uint32_t key_scale;
        } op[4];

        uint8_t algorithm;
        uint8_t feedback;
        uint8_t fnum;
        uint8_t block;
        uint8_t detune;
        uint32_t freq;
        uint8_t ams;
        uint8_t fms;
        bool key_on[4];
        uint32_t lfo_phase;
    } ch[6];
} Ym2612;

/* ---------- SN76489 (Sega PSG) ---------- */
typedef struct {
    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t volume;
        bool tone;
    } tone[3];

    struct {
        uint32_t phase;
        uint16_t period;
        uint8_t volume;
        bool white_noise;
        uint16_t lfsr;
    } noise;

    uint8_t last_reg;
} Sn76489;

/* ---------- MOS6581/8580 SID (C64) ---------- */
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

    uint16_t fc;
    uint8_t res;
    uint8_t filt;
    uint8_t mode;
    int32_t hp_y1, hp_y2;
    int32_t bp_y1, bp_y2;
    int32_t lp_y1, lp_y2;
} Sid;

/* ---------- SAA1099 ---------- */
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

/* ---------- VRC6 ---------- */
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

/* ---------- N163 (Namco 163) ---------- */
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

/* ---------- OPL / OPL2 / OPL3 (YM3526 / YM3812) ---------- */
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
    } ch[18];

    uint8_t mode;
    uint32_t lfo_am_phase;
    uint32_t lfo_vib_phase;
} Opl;

/* ---------- SCC (Konami) ---------- */
typedef struct {
    struct {
        uint32_t phase;
        uint16_t freq;
        uint8_t volume;
        int8_t wave[32];
    } ch[5];
} Scc;

/* ---------- AY-3-8910 ---------- */
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

/* ---------- PC Speaker ---------- */
typedef struct {
    uint32_t phase;
    uint16_t freq;
    bool gate;
} PcSpeaker;

/* ====================================================================
 * FURNACE TRACKER TYPES (internal)
 * ==================================================================== */

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

#endif /* WUBU_AUDIO_TYPES_H */