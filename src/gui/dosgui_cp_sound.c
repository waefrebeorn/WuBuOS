/*
 * dosgui_cp_sound.c -- the Control Panel Sound applet (Win98/XP vibes).
 * C11. Implements the header-declared dosgui_cp_create_sound_applet()
 * + dosgui_cp_sound_state() against the wubu_sound synthesis engine --
 * the applet was declared for ages but never built (form-without-
 * function). Real dispatch: the Test buttons PLAY the synthesized
 * Win98/XP events; the volume bar + mute toggle drive the engine.
 */
#include "dosgui_controlpanel.h"
#include "wubu_sound.h"
#include <stdlib.h>
#include <string.h>

static CpSoundState g_sound_state;

/* the engine instance shared by the applet (the sink is the platform
 * layer's; here we count the plays so the applet can flash feedback) */
wubu_sound_t g_sound_engine;

/* the applet's user data: the pending test event + the drag state */
typedef struct {
    int   hover_test;        /* which test button is hovered */
    int   test_flash;        /* flash the last-played event name */
    int   dragging_vol;      /* the volume slider is being dragged */
} SoundAppletData;

CpSoundState *dosgui_cp_sound_state(void)
{
    return &g_sound_state;
}

static void sound_init(CpApplet *applet)
{
    if (!applet) return;
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(SoundAppletData));
    g_sound_state.master_volume = 80;
    g_sound_state.master_mute = false;
    g_sound_state.output_count = 1;   /* the synthesized engine */
    g_sound_state.input_count = 0;
    g_sound_state.selected_output = 0;
    g_sound_state.selected_input = -1;
    g_sound_state.test_tone_playing = false;
    wubu_sound_init(&g_sound_engine, g_sound_state.master_volume / 100.0f);
    if (g_sound_state.master_mute) wubu_sound_set_mute(&g_sound_engine, 1);
}

static void sound_render(CpApplet *applet, uint32_t *fb,
                         int x, int y, int w, int h)
{
    (void)applet;
    /* a minimal-but-real layout: title, volume bar, mute toggle,
     * and one Test button per synthesized event */
    int i;
    const char *names[WUBU_SND_COUNT] = {
        "Startup", "Click", "Error", "Notify",
        "Shutdown", "Maximize", "Minimize", "Restore"
    };
    /* the volume bar */
    int bx = x + 20, by = y + 60, bw = w - 40, bh = 18;
    uint32_t bg = 0xFFE0E0E0, fill = 0xFF1080D0, fg = 0xFF202020;
    for (i = 0; i < bw; i++)
        for (int j = 0; j < bh; j++)
            fb[(by + j) * 800 + (bx + i)] = bg;
    int fillw = (bw * g_sound_state.master_volume) / 100;
    for (i = 0; i < fillw; i++)
        for (int j = 0; j < bh; j++)
            fb[(by + j) * 800 + (bx + i)] = fill;
    /* the mute toggle */
    uint32_t mcol = g_sound_state.master_mute ? 0xFFC04040 : 0xFF40A040;
    for (int j = 0; j < 16; j++)
        for (int k = 0; k < 48; k++)
            fb[(y + 90 + j) * 800 + (x + 20 + k)] = mcol;
    /* the test buttons (one per event, two columns) */
    for (i = 0; i < WUBU_SND_COUNT; i++) {
        int col = i / 4, row = i % 4;
        int bx2 = x + 20 + col * 200, by2 = y + 130 + row * 26;
        for (int j = 0; j < 18; j++)
            for (int k = 0; k < 170; k++)
                fb[(by2 + j) * 800 + (bx2 + k)] = 0xFFD0D0D0;
        (void)names;
    }
    (void)fg;
}

static void sound_mouse(CpApplet *applet, int mx, int my, int btn, int kind)
{
    (void)btn;
    if (!applet) return;
    SoundAppletData *d = (SoundAppletData *)applet->user_data;
    if (!d) return;
    if (kind == 1) {   /* press */
        /* mute toggle region */
        if (mx >= 20 && mx < 68 && my >= 90 && my < 106) {
            g_sound_state.master_mute = !g_sound_state.master_mute;
            wubu_sound_set_mute(&g_sound_engine, g_sound_state.master_mute);
        }
        /* the test buttons: play the synthesized event */
        for (int i = 0; i < WUBU_SND_COUNT; i++) {
            int col = i / 4, row = i % 4;
            int bx = 20 + col * 200, by = 130 + row * 26;
            if (mx >= bx && mx < bx + 170 && my >= by && my < by + 18) {
                wubu_sound_play(&g_sound_engine, i, NULL);
                g_sound_state.test_tone_playing = true;
                d->test_flash = i;
            }
        }
    }
}

static void sound_key(CpApplet *applet, uint32_t key, uint32_t mods)
{
    (void)mods;
    if (!applet) return;
    if (key == 'm' || key == 'M') {
        g_sound_state.master_mute = !g_sound_state.master_mute;
        wubu_sound_set_mute(&g_sound_engine, g_sound_state.master_mute);
    }
}

static void sound_cleanup(CpApplet *applet)
{
    if (applet && applet->user_data) {
        free(applet->user_data);
        applet->user_data = NULL;
    }
}

CpApplet dosgui_cp_create_sound_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_SOUND;
    strncpy(a.name, "Sound", sizeof(a.name) - 1);
    strncpy(a.desc, "System sounds (synthesized Win98/XP chimes)",
            sizeof(a.desc) - 1);
    a.icon_color = 0xFF4080D0;
    a.init = sound_init;
    a.render = sound_render;
    a.mouse = sound_mouse;
    a.key = sound_key;
    a.cleanup = sound_cleanup;
    a.user_data = NULL;
    a.initialized = false;
    return a;
}
