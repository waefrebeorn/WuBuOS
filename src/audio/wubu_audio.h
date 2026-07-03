/*
 * wubu_audio.h  --  WuBuOS Audio Engine
 *
 * Cell 401: Combined DAW + Tracker + Synthesizer engine.
 *
 * Architecture (rip the best from each):
 *
 *  ARDOUR (ardour.org):
 *    - Non-destructive multitrack recording/editing
 *    - JACK audio routing (low-latency)
 *    - Send/return buses, aux, master
 *    - Automation lanes
 *    - Region-based editing
 *    - Plugin chain per track (LV2, VST, CLAP)
 *
 *  FURNACE (github.com/tildearrow/furnace):
 *    - Multi-system chiptune tracker
 *    - DefleMask-compatible .dmf format
 *    - 30+ sound chips: NES APU, Game Boy, Genesis YM2612,
 *      SN76489, SID, PC Speaker, SAA1099, VRC6, N163, etc.
 *    - Real-time pattern editing
 *    - Effects: arpeggio, pitch slide, vibrato, portamento
 *
 *  TinySoundFont (github.com/schellingb/TinySoundFont):
 *    - SF2 (SoundFont 2) sample-based synthesis
 *    - Real-time MIDI playback
 *    - Preset/instrument/zone hierarchy
 *    - Built-in reverb + chorus
 *    - Works with sf2repo + sf2create soundfonts
 *
 *  PIPELINE:
 *
 *    MIDI Input (USB/evdev) → Furnace Tracker → Chip Sound
 *                        → TinySoundFont → Sample Synth
 *                        → Ardour Mixer →Master Bus
 *                        → ALSA/JACK/PipeWire → Speakers
 *
 *    AI Plugin Container → DSP Processing → Insert on any track
 *
 *  USB MIDI:
 *    - ALSA sequencer (/dev/snd/seq)
 *    - Raw MIDI (/dev/midi*, /dev/snd/midi*)
 *    - evdev for MIDI keyboards that show up as HID
 *    - Furnace can receive MIDI input for live playback
 *    - TinySoundFont plays MIDI in real-time
 *
 *  TRACKER + DAW INTEGRATION:
 *    - Furnace patterns → export as audio → Ardour track
 *    - Ardour regions → import into Furnace as samples
 *    - SF2 instruments used in both Furnace and Ardour
 *    - Single mixer (Ardour) for all sources
 *    - JACK routes between Furnace, SF2 synth, and Ardour
 *
 *  AI INGESTION:
 *    - AI models run as .wubu containers
 *    - Audio streamed via JACK/pipe
 *    - Models: source separation, mastering, transcription,
 *      style transfer, sample generation
 *    - Container gets audio buffer → GPU inference → output buffer
 *    - Plugin API: Ardour LV2 + custom WUBU plugin type
 */
#ifndef WUBU_AUDIO_H
#define WUBU_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* -- Audio Limits -------------------------------------------------- */

#define WUBU_AUDIO_MAX_TRACKS      64
#define WUBU_AUDIO_MAX_REGIONS     256
#define WUBU_AUDIO_MAX_PLUGINS     16
#define WUBU_AUDIO_MAX_BUSES       16
#define WUBU_AUDIO_MAX_FURNACE_CHANS 32
#define WUBU_AUDIO_MAX_SF2_LAYERS  16
#define WUBU_AUDIO_MAX_PATTERNS    256
#define WUBU_AUDIO_MAX_ORDERS      256
#define WUBU_AUDIO_MAX_INSTRUMENTS 256
#define WUBU_AUDIO_MAX_AI_PLUGINS  8

/* -- Sample Rate / Buffer ------------------------------------------ */

typedef enum {
    SR_44100  = 44100,
    SR_48000  = 48000,
    SR_88200  = 88200,
    SR_96000  = 96000,
    SR_176400 = 176400,
    SR_192000 = 192000,
} WubuSampleRate;

#define WUBU_AUDIO_DEFAULT_SR     SR_48000
#define WUBU_AUDIO_DEFAULT_BUF    256
#define WUBU_AUDIO_DEFAULT_CH     2

/* -- Sound Chip Types (from Furnace) ------------------------------ */

typedef enum {
    CHIP_NONE        = 0,
    CHIP_NES_APU     = 1,    /* NES 2A03 */
    CHIP_GB_APU      = 2,    /* Game Boy DMG */
    CHIP_YM2612      = 3,    /* Genesis FM */
    CHIP_SN76489     = 4,    /* Genesis PSG */
    CHIP_SID         = 5,    /* MOS 6581/8580 */
    CHIP_SAA1099     = 6,    /* Philips SAA1099 */
    CHIP_PCSPEAKER   = 7,    /* PC Speaker */
    CHIP_VRC6        = 8,    /* Konami VRC6 */
    CHIP_N163        = 9,    /* Namco 163 */
    CHIP_OPLL        = 10,   /* Yamaha OPLL (MSX) */
    CHIP_OPL         = 11,   /* Yamaha OPL (Sound Blaster) */
    CHIP_OPL2        = 12,   /* Yamaha OPL2 */
    CHIP_OPL3        = 13,   /* Yamaha OPL3 */
    CHIP_SCC         = 14,   /* Konami SCC */
    CHIP_AY8910      = 15,   /* AY-3-8910 */
    CHIP_TIA         = 16,   /* Atari TIA */
    CHIP_POKEY       = 17,   /* Atari POKEY */
    CHIP_C64         = 18,   /* C64 SID variant */
    CHIP_COUNT
} WubuChipType;

/* -- Furnace Tracker ---------------------------------------------- */

typedef struct {
    uint8_t note;        /* 0=C-0, 12=C-1, etc; 255=empty */
    uint8_t octave;
    uint8_t instrument;  /* Instrument index */
    uint8_t volume;      /* 0-15 */
    uint8_t effect;
    uint8_t effect_val;
} WubuTrackerCell;

typedef struct {
    WubuTrackerCell cells[WUBU_AUDIO_MAX_FURNACE_CHANS];
} WubuTrackerRow;

typedef struct {
    WubuChipType chip;
    WubuTrackerRow rows[256];
    int           n_rows;
    char          name[32];
} WubuFurnacePattern;

typedef struct {
    WubuChipType chips[WUBU_AUDIO_MAX_FURNACE_CHANS];
    int          n_chips;

    WubuFurnacePattern patterns[WUBU_AUDIO_MAX_PATTERNS];
    int                n_patterns;

    uint8_t order[WUBU_AUDIO_MAX_ORDERS][WUBU_AUDIO_MAX_FURNACE_CHANS];
    int     n_orders;

    int     tempo;
    int     speed;
    int     highlight_major;
    int     highlight_minor;

    /* Playback */
    bool    playing;
    int     current_pattern;
    int     current_row;
    double  tick_accum;
} WubuFurnace;

/* -- TinySoundFont ------------------------------------------------ */

/* SF2 Preset */
typedef struct {
    char     name[32];
    uint16_t bank;
    uint16_t preset;
    uint8_t  layers;
    float    volume;         /* dB */
    float    pan;            /* -1 to 1 */
} WubuSF2Preset;

/* SF2 Synth */
typedef struct {
    /* Loaded SoundFont data */
    uint8_t  *sf2_data;
    size_t    sf2_size;

    /* Presets */
    WubuSF2Preset presets[256];
    int           n_presets;

    /* Playback */
    float    *render_buffer;
    int       render_frames;
    int       render_pos;

    /* Active voices */
    int       active_voices;
    int       max_voices;

    /* Effects */
    float     reverb_mix;    /* 0-1 */
    float     chorus_mix;    /* 0-1 */

    /* MIDI state */
    uint8_t   midi_channels[16]; /* Program per channel */
    int16_t   pitch_bend[16];    /* Pitch bend per channel (-8192 to +8191) */
} WubuSF2Synth;

/* -- Ardour-style DAW Track --------------------------------------- */

typedef enum {
    TRACK_AUDIO   = 0,
    TRACK_MIDI    = 1,
    TRACK_BUS     = 2,
    TRACK_MASTER  = 3,
    TRACK_FURNACE = 4,    /* Furnace tracker output */
    TRACK_SF2     = 5,    /* TinySoundFont output */
    TRACK_AI      = 6,    /* AI plugin output */
} WubuTrackType;

typedef struct {
    double   start_time;     /* Session time */
    double   duration;
    float   *samples;        /* Deinterleaved float */
    int      channels;
} WubuAudioRegion;

typedef struct {
    char         name[64];
    WubuTrackType type;
    float         volume;     /* Linear 0-1 */
    float         pan;        /* -1 to 1 */
    bool          mute;
    bool          solo;
    bool          record_arm;
    int           bus_send;   /* Which bus to send to (-1 = master) */

    /* Regions */
    WubuAudioRegion regions[WUBU_AUDIO_MAX_REGIONS];
    int             n_regions;

    /* Plugin chain */
    int           plugin_chain[WUBU_AUDIO_MAX_PLUGINS];
    int           n_plugins;

    /* Furnace (if TRACK_FURNACE) */
    WubuFurnace  *furnace;

    /* SF2 (if TRACK_SF2) */
    WubuSF2Synth *sf2;
} WubuDAWTrack;

/* -- Ardour-style DAW Bus ----------------------------------------- */

typedef struct {
    char     name[64];
    float    volume;
    bool     mute;
    int      input_track;    /* -1 = master mix */
    int      plugin_chain[8];
    int      n_plugins;
} WubuDAWBus;

/* -- AI Audio Plugin (runs in .wubu container) ------------------- */

typedef enum {
    AI_PLUGIN_SEPARATION  = 0,  /* Source separation (stems) */
    AI_PLUGIN_MASTERING   = 1,  /* Auto-mastering */
    AI_PLUGIN_TRANSCRIBE  = 2,  /* Audio → MIDI/MusicXML */
    AI_PLUGIN_STYLE       = 3,  /* Style transfer */
    AI_PLUGIN_GENERATE    = 4,  /* Sample generation */
    AI_PLUGIN_DENOISE     = 5,  /* Noise reduction */
    AI_PLUGIN_UPSCALE     = 6,  /* Audio super-resolution */
} WubuAIPluginType;

typedef struct {
    WubuAIPluginType type;
    char             name[64];
    char             model_path[256];
    int              container_id;  /* .wubu container PID */
    bool             active;

    /* Audio buffers */
    float           *input_buf;
    float           *output_buf;
    int              buf_frames;
    int              channels;

    /* Ingestion protocol */
    float           *model_input;   /* Preprocessed for model */
    float           *model_output;  /* Postprocessed from model */
    int              model_frames;  /* Frames per inference */
} WubuAIPlugin;

/* -- Complete Audio Engine ---------------------------------------- */

typedef struct {
    /* Backends */
    int              sample_rate;
    int              buffer_frames;
    int              channels;

    /* DAW */
    WubuDAWTrack     tracks[WUBU_AUDIO_MAX_TRACKS];
    int              n_tracks;
    WubuDAWBus       buses[WUBU_AUDIO_MAX_BUSES];
    int              n_buses;
    double           playhead;       /* Current play position */
    double           total_length;
    bool             playing;
    bool             recording;

    /* Furnace */
    WubuFurnace      furnace;
    bool             furnace_active;

    /* TinySoundFont */
    WubuSF2Synth     sf2;
    bool             sf2_active;
    uint8_t          sf2_midi_buf[1024];
    int              sf2_midi_len;

    /* AI Plugins */
    WubuAIPlugin     ai_plugins[WUBU_AUDIO_MAX_AI_PLUGINS];
    int              n_ai_plugins;

    /* USB MIDI */
    int              midi_fds[4];
    int              n_midi_fds;

    /* Output ring buffer */
    float           *master_buf;
    int              master_buf_size;
    int              master_r, master_w;
} WubuAudioEngine;

/* ==================================================================
 *  API: Audio Engine Lifecycle
 * ================================================================== */

int  wubu_audio_engine_create(int sample_rate, int buffer_frames, int channels);
void wubu_audio_engine_destroy(void);
WubuAudioEngine *wubu_audio_engine(void);

/* Start/stop audio processing */
int  wubu_audio_start(void);
void wubu_audio_stop(void);

/* Process one buffer (called by audio callback) */
void wubu_audio_process(float *output, int frames);

/* ==================================================================
 *  API: DAW Operations
 * ================================================================== */

/* Track management */
int  wubu_daw_add_track(const char *name, WubuTrackType type);
void wubu_daw_remove_track(int track);
int  wubu_daw_track_count(void);

/* Transport */
void wubu_daw_play(void);
void wubu_daw_stop(void);
void wubu_daw_seek(double time);
void wubu_daw_set_loop(double start, double end);

/* Recording */
void wubu_daw_record_start(int track);
void wubu_daw_record_stop(void);

/* ==================================================================
 *  API: Furnace Tracker
 * ================================================================== */

int  wubu_furnace_init(int n_chips, const WubuChipType *chips);
void wubu_furnace_shutdown(void);

/* Pattern editing */
void wubu_furnace_set_note(int pattern, int row, int chan,
                            uint8_t note, uint8_t octave);
void wubu_furnace_set_inst(int pattern, int row, int chan, uint8_t inst);
void wubu_furnace_set_effect(int pattern, int row, int chan,
                              uint8_t effect, uint8_t val);

/* Playback */
void wubu_furnace_play(void);
void wubu_furnace_stop(void);
void wubu_furnace_set_tempo(int tempo);

/* Export pattern as audio */
int  wubu_furnace_render_pattern(int pattern, float *out, int frames);

/* ==================================================================
 *  API: TinySoundFont
 * ================================================================== */

int  wubu_sf2_load(const uint8_t *data, size_t size);
int  wubu_sf2_load_file(const char *path);
void wubu_sf2_unload(void);

/* MIDI events */
void wubu_sf2_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
void wubu_sf2_note_off(uint8_t channel, uint8_t note);
void wubu_sf2_program_change(uint8_t channel, uint8_t program);
void wubu_sf2_pitch_bend(uint8_t channel, int bend);
void wubu_sf2_control(uint8_t channel, uint8_t cc, uint8_t val);

/* Render audio */
void wubu_sf2_render(float *out, int frames, int channels);

/* Preset info */
int  wubu_sf2_preset_count(void);
const WubuSF2Preset *wubu_sf2_preset(int idx);

/* ==================================================================
 *  API: USB MIDI Input
 * ================================================================== */

/* Open MIDI devices */
int  wubu_midi_open(const char *path);
void wubu_midi_close(int fd);

/* Read MIDI event (returns bytes read, 0 if none) */
int  wubu_midi_read(int fd, uint8_t *buf, int len);

/* Enumerate MIDI devices */
int  wubu_midi_enumerate(char paths[][256], char names[][64], int max);

/* ==================================================================
 *  API: AI Audio Plugins
 * ================================================================== */

/* Register an AI plugin (runs as .wubu container) */
int  wubu_ai_plugin_register(const char *name, WubuAIPluginType type,
                               const char *model_path);

/* Process audio through AI plugin */
int  wubu_ai_plugin_process(int plugin_idx,
                              const float *input, float *output,
                              int frames, int channels);

/* Start/stop AI container */
int  wubu_ai_plugin_start(int plugin_idx);
void wubu_ai_plugin_stop(int plugin_idx);

/* ==================================================================
 *  API: Ingestion Protocols
 * ==================================================================
 *
 *  Ingestion = getting audio/MIDI/data INTO the engine.
 *  Multiple protocols supported:
 *
 *  1. ALSA raw MIDI    → /dev/snd/midi*, /dev/snd/seq
 *  2. evdev HID        → /dev/input/event* (MIDI keyboards)
 *  3. USB bulk         → /dev/bus/usb/* (class-compliant MIDI)
 *  4. JACK MIDI        → JACK transport (Ardour integration)
 *  5. PipeWire         → modern Linux audio
 *  6. Network (RTP)    → RTP-MIDI / AVB
 *  7. File             → .mid, .dmf, .sf2, .wav, .flac, .ogg
 *  8. Container        → .wubu audio plugins via 9P namespace
 */

/* Open any ingestion source */
int  wubu_ingest_open(const char *uri);
/* uri format: "alsa:seq", "evdev:/dev/input/event5", "jack", "file:path" */

/* Read from ingestion source */
int  wubu_ingest_read(int handle, uint8_t *buf, int len);

/* Close ingestion source */
void wubu_ingest_close(int handle);

/* List available ingestion sources */
int  wubu_ingest_enumerate(char uris[][256], char names[][64], int max);

#endif /* WUBU_AUDIO_H */

/* Set volume for a cell (0-15) */
void wubu_furnace_set_volume(int pattern, int row, int chan, uint8_t volume);
