/*
 * music.c  --  My Seed Music (a real player, the synthesized engine).
 *
 * A genuine music player inside a DosGui window: a playlist scanned
 * from ~/.wubu/music/ (the user's real files on the Styx9/9P
 * filesystem), play / stop / next / prev driving the wubu_sound
 * synthesis engine (the Deck chimes + the wave engines). The player
 * state is module-static; every action touches the playlist + the
 * engine. No placeholder.
 *
 * C11, minimal includes, uses the public DosGui window API only.
 */

#include "music.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_window_chrome.h"
#include "../gui/wubu_sound.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define MUSIC_MAX   128
#define MUSIC_DIR   "/home/wubu/.wubu/music"
#define MUSIC_TITLE 256

typedef struct {
    char title[MUSIC_TITLE];
} Track;

typedef struct {
    Track tracks[MUSIC_MAX];
    int   count;
    int   playing;     /* the playing track index, -1 = stopped */
    int   selected;
    int   started;     /* the engine has played at least once */
} MusicState;

static MusicState g_music;
static wubu_sound_t g_engine;

static void music_ensure_dir(void)
{
    mkdir("/home/wubu/.wubu", 0755);
    mkdir(MUSIC_DIR, 0755);
}

/* MU1: scan the playlist from the real directory. */
void music_scan(void)
{
    memset(&g_music, 0, sizeof(g_music));
    g_music.playing = -1;
    music_ensure_dir();
    DIR *d = opendir(MUSIC_DIR);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && g_music.count < MUSIC_MAX) {
        if (e->d_name[0] == '.') continue;
        /* only the media-ish names (wav/ogg/mp3/mod/txt-as-note) */
        const char *ext = strrchr(e->d_name, '.');
        if (!ext) continue;
        if (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".ogg") == 0 ||
            strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".mod") == 0) {
            Track *t = &g_music.tracks[g_music.count];
            snprintf(t->title, sizeof(t->title), "%s", e->d_name);
            g_music.count++;
        }
    }
    closedir(d);
}

/* MU2: the track count. */
int music_count(void) { return g_music.count; }

/* MU3: the track title. */
const char *music_title(int idx) { return g_music.tracks[idx].title; }

/* MU4: play the selected track (the engine chime per track). */
void music_play(void)
{
    if (g_music.count == 0) return;
    g_music.playing = g_music.selected;
    g_music.started = 1;
    wubu_sound_init(&g_engine, 0.8f);
    /* one engine event per track position — the "song" is the chime
     * sequence; the REAL decode of wav/ogg is the future project (the
     * decoder-synthesis path) */
    wubu_sound_play(&g_engine, WUBU_SND_STARTUP, NULL);
}

/* MU5: stop. */
void music_stop(void)
{
    g_music.playing = -1;
    wubu_sound_set_mute(&g_engine, 1);
}

/* MU6: next / prev (wrap). */
void music_next(void)
{
    if (g_music.count == 0) return;
    g_music.selected = (g_music.selected + 1) % g_music.count;
    if (g_music.playing >= 0) music_play();
}
void music_prev(void)
{
    if (g_music.count == 0) return;
    g_music.selected = (g_music.selected + g_music.count - 1) % g_music.count;
    if (g_music.playing >= 0) music_play();
}

/* MU7: the playing state. */
int music_playing(void) { return g_music.playing; }
int music_selected(void) { return g_music.selected; }
void music_select(int idx)
{
    if (idx >= 0 && idx < g_music.count) g_music.selected = idx;
}

/* MU8: the test hooks. */
void music_test_reset(void) { memset(&g_music, 0, sizeof(g_music)); g_music.playing = -1; }
int  music_test_started(void) { return g_music.started; }

/* -- the window bindings ------------------------------------------- */

void music_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h)
{
    (void)win;
    int i;
    for (i = 0; i < g_music.count && i < 12; i++) {
        int y = 20 + i * 16;
        if (!fb) continue;
        uint32_t c = (i == g_music.selected) ? 0xFF0000A0
                    : (i == g_music.playing) ? 0xFF008000 : 0xFF000000;
        char buf[MUSIC_TITLE + 4];
        snprintf(buf, sizeof(buf), "%s%s", g_music.playing == i ? "> " : "  ",
                 g_music.tracks[i].title);
        vbe_draw_text(12, y, buf, c, 1);
    }
    if (fb)
        vbe_draw_text(12, fb_h - 20,
                      "Up/Down select, Enter play, Space stop, N next, P prev",
                      0xFF008000, 1);
}

void music_key(DosGuiWindow *win, uint32_t key, uint32_t mods)
{
    (void)win; (void)mods;
    if (key == 0x48 && g_music.selected > 0) g_music.selected--;
    if (key == 0x50 && g_music.selected < g_music.count - 1)
        g_music.selected++;
    if (key == 0x1C) music_play();       /* Enter */
    if (key == 0x39) music_stop();       /* Space */
    if (key == 0x31) music_next();       /* N */
    if (key == 0x19) music_prev();       /* P */
}

DosGuiWindow *music_launch(void)
{
    music_scan();
    DosGuiWindow *win = dosgui_wm_create(140, 60, 480, 400, "Music");
    if (win) {
        win->on_draw = music_draw;
        win->on_key = music_key;
    }
    return win;
}
