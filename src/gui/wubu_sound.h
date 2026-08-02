/*
 * wubu_sound.h -- WuBuOS system-sound engine (Win98/XP vibes). C11.
 *
 * Everything is SYNTHESIZED in pure C (no wav assets, no third-party):
 * a tiny PCM synthesizer (additive sine/square/triangle/saw + ADSR)
 * plays the classic OS events. The design follows the Material sound
 * guidance (feedback, not decoration; short, repeatable, understated).
 *
 * Events (Win98/XP heritage):
 *   - WUBU_SND_STARTUP: the ascending chord (the "Microsoft Sound" feel)
 *   - WUBU_SND_CLICK:   the short UI click
 *   - WUBU_SND_ERROR:   the low sawtooth buzz
 *   - WUBU_SND_NOTIFY:  the three-note chime
 *   - WUBU_SND_SHUTDOWN: the descending chord
 *   - WUBU_SND_MAXIMIZE/MINIMIZE/RESTORE: the XP window whoosh-ish blips
 */
#ifndef WUBU_SOUND_H
#define WUBU_SOUND_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_SND_SAMPLE_RATE 22050
#define WUBU_SND_MAX_EVENTS 8

enum {
    WUBU_SND_STARTUP = 0,
    WUBU_SND_CLICK,
    WUBU_SND_ERROR,
    WUBU_SND_NOTIFY,
    WUBU_SND_SHUTDOWN,
    WUBU_SND_MAXIMIZE,
    WUBU_SND_MINIMIZE,
    WUBU_SND_RESTORE,
    WUBU_SND_COUNT
};

/* The engine state. */
typedef struct {
    int    muted;
    float  volume;          /* 0..1 */
    int    events_played[WUBU_SND_COUNT];
} wubu_sound_t;

/* A synthesized event's note: (freq, duration-samples, waveform). */
enum { WUBU_WAVE_SINE = 0, WUBU_WAVE_SQUARE, WUBU_WAVE_TRIANGLE, WUBU_WAVE_SAW };

int wubu_sound_init(wubu_sound_t *s, float volume);

/* Synthesize an event into a 16-bit mono PCM buffer.
 * Returns the sample count (<= cap). The buffer is fully written. */
int wubu_sound_render(const wubu_sound_t *s, int event,
                      int16_t *out, int cap);

/* The engine: play = render + deliver to the registered sink. The
 * sink is optional (NULL = just render); a real sink (host audio,
 * beeper, wav-writer) is wired by the platform layer. */
typedef void (*wubu_sound_sink_t)(const int16_t *pcm, int n);

int wubu_sound_play(wubu_sound_t *s, int event,
                    wubu_sound_sink_t sink);
int wubu_sound_set_volume(wubu_sound_t *s, float volume);
int wubu_sound_set_mute(wubu_sound_t *s, int muted);

/* The per-event duration (samples) so the UI can budget for it. */
int wubu_sound_duration(int event);

#endif
