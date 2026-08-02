/*
 * wubu_sound.c -- WuBuOS system-sound engine (Win98/XP vibes). C11.
 * Pure PCM synthesis -- no assets, no third-party.
 */
#include "wubu_sound.h"
#include <math.h>
#include <string.h>

/* one note: (frequency, duration-samples, waveform, volume) */
typedef struct {
    float freq;
    int   dur;
    int   wave;
    float vol;
} note_t;

/* The event tables (the Win98/XP heritage sequences). */
static const note_t g_startup[] = {
    { 523.25f, 9000, WUBU_WAVE_SINE, 0.5f },   /* C5  */
    { 659.25f, 9000, WUBU_WAVE_SINE, 0.5f },   /* E5  */
    { 783.99f, 12000, WUBU_WAVE_SINE, 0.55f }, /* G5  */
    { 1046.5f, 16000, WUBU_WAVE_SINE, 0.6f },  /* C6  */
};
static const note_t g_click[] = {
    { 900.0f, 700, WUBU_WAVE_SQUARE, 0.25f },
};
static const note_t g_error[] = {
    { 196.0f, 6000, WUBU_WAVE_SAW, 0.5f },
    { 155.0f, 8000, WUBU_WAVE_SAW, 0.5f },
};
static const note_t g_notify[] = {
    { 659.25f, 4000, WUBU_WAVE_SINE, 0.4f },   /* E5 */
    { 830.61f, 5000, WUBU_WAVE_SINE, 0.45f },  /* G#5 */
    { 987.77f, 7000, WUBU_WAVE_SINE, 0.5f },   /* B5 */
};
static const note_t g_shutdown[] = {
    { 1046.5f, 9000, WUBU_WAVE_SINE, 0.5f },   /* C6 */
    { 783.99f, 9000, WUBU_WAVE_SINE, 0.5f },   /* G5 */
    { 523.25f, 14000, WUBU_WAVE_SINE, 0.55f }, /* C5 */
};
static const note_t g_maximize[] = {
    { 440.0f, 2000, WUBU_WAVE_TRIANGLE, 0.3f },
    { 660.0f, 2500, WUBU_WAVE_TRIANGLE, 0.3f },
};
static const note_t g_minimize[] = {
    { 660.0f, 2000, WUBU_WAVE_TRIANGLE, 0.3f },
    { 440.0f, 2500, WUBU_WAVE_TRIANGLE, 0.3f },
};
static const note_t g_restore[] = {
    { 550.0f, 2200, WUBU_WAVE_TRIANGLE, 0.3f },
};

static const note_t *g_events[WUBU_SND_COUNT];
static int           g_event_n[WUBU_SND_COUNT];

static void build_tables(void)
{
    static int built = 0;
    if (built) return;
    g_events[WUBU_SND_STARTUP]  = g_startup;
    g_event_n[WUBU_SND_STARTUP] = (int)(sizeof(g_startup) / sizeof(note_t));
    g_events[WUBU_SND_CLICK]    = g_click;
    g_event_n[WUBU_SND_CLICK]   = (int)(sizeof(g_click) / sizeof(note_t));
    g_events[WUBU_SND_ERROR]    = g_error;
    g_event_n[WUBU_SND_ERROR]   = (int)(sizeof(g_error) / sizeof(note_t));
    g_events[WUBU_SND_NOTIFY]   = g_notify;
    g_event_n[WUBU_SND_NOTIFY]  = (int)(sizeof(g_notify) / sizeof(note_t));
    g_events[WUBU_SND_SHUTDOWN] = g_shutdown;
    g_event_n[WUBU_SND_SHUTDOWN] = (int)(sizeof(g_shutdown) / sizeof(note_t));
    g_events[WUBU_SND_MAXIMIZE] = g_maximize;
    g_event_n[WUBU_SND_MAXIMIZE] = (int)(sizeof(g_maximize) / sizeof(note_t));
    g_events[WUBU_SND_MINIMIZE] = g_minimize;
    g_event_n[WUBU_SND_MINIMIZE] = (int)(sizeof(g_minimize) / sizeof(note_t));
    g_events[WUBU_SND_RESTORE]  = g_restore;
    g_event_n[WUBU_SND_RESTORE] = (int)(sizeof(g_restore) / sizeof(note_t));
    built = 1;
}

static float wave(int w, float phase)
{
    switch (w) {
    case WUBU_WAVE_SINE:     return sinf(phase);
    case WUBU_WAVE_SQUARE:   return phase < 3.14159265f ? 1.0f : -1.0f;
    case WUBU_WAVE_TRIANGLE: return (float)(2.0 / 3.14159265) * asinf(sinf(phase));
    case WUBU_WAVE_SAW:      return (float)(2.0 / 3.14159265) * atanf(tanf(phase * 0.5f));
    default:                 return 0;
    }
}

int wubu_sound_init(wubu_sound_t *s, float volume)
{
    if (!s) return -1;
    memset(s, 0, sizeof(*s));
    s->volume = (volume < 0) ? 0 : (volume > 1 ? 1 : volume);
    build_tables();
    return 0;
}

int wubu_sound_render(const wubu_sound_t *s, int event,
                      int16_t *out, int cap)
{
    if (!s || !out || event < 0 || event >= WUBU_SND_COUNT) return -1;
    if (cap <= 0) return 0;
    build_tables();
    const note_t *notes = g_events[event];
    int n = g_event_n[event];

    int total = 0;
    /* render each note sequentially with a 3ms release gap */
    for (int k = 0; k < n && total < cap; k++) {
        const note_t *nt = &notes[k];
        float phase = 0;
        int dur = nt->dur;
        if (total + dur > cap) dur = cap - total;
        for (int i = 0; i < dur; i++) {
            float ph = phase;
            /* ADSR: fast attack, slight decay, release at the tail */
            float t = (float)i / (float)(dur > 0 ? dur : 1);
            float env = (t < 0.02f) ? (t / 0.02f)
                      : (t > 0.85f) ? ((1.0f - t) / 0.15f)
                      : 1.0f;
            float v = wave(nt->wave, ph) * nt->vol * env;
            int16_t samp = (int16_t)(v * 32000.0f * s->volume);
            out[total++] = samp;
            phase += 2.0f * 3.14159265f * nt->freq / WUBU_SND_SAMPLE_RATE;
        }
        if (total < cap) out[total++] = 0;   /* the release gap */
    }
    int actual = total;   /* the REAL sample count (notes + gaps) */
    /* zero-pad the tail (deterministic output); return the actual
     * count, NOT the padded cap (DA: the old code returned cap). */
    for (; total < cap; total++) out[total] = 0;
    return actual;
}

int wubu_sound_play(wubu_sound_t *s, int event, wubu_sound_sink_t sink)
{
    if (!s || event < 0 || event >= WUBU_SND_COUNT) return -1;
    if (s->muted) return 0;
    int dur = wubu_sound_duration(event);
    static int16_t buf[65536];
    if (dur > 65536) dur = 65536;
    int n = wubu_sound_render(s, event, buf, dur);
    if (sink) sink(buf, n);
    s->events_played[event]++;
    return n;
}

int wubu_sound_set_volume(wubu_sound_t *s, float volume)
{
    if (!s) return -1;
    s->volume = (volume < 0) ? 0 : (volume > 1 ? 1 : volume);
    return 0;
}

int wubu_sound_set_mute(wubu_sound_t *s, int muted)
{
    if (!s) return -1;
    s->muted = muted ? 1 : 0;
    return 0;
}

int wubu_sound_duration(int event)
{
    if (event < 0 || event >= WUBU_SND_COUNT) return 0;
    build_tables();
    int total = 0;
    for (int k = 0; k < g_event_n[event]; k++)
        total += g_events[event][k].dur + 1;   /* + the release gap */
    return total;
}
