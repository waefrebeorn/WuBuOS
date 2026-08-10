/*
 * music.h  --  the Music app (the real player).
 */
#include <stdint.h>

/* the window type (forward) */
struct DosGuiWindow;

#ifndef WUBUOS_MUSIC_H
#define WUBUOS_MUSIC_H

/* MU1: scan the playlist from ~/.wubu/music/. */
void music_scan(void);

/* MU2: the track count. */
int music_count(void);

/* MU3: the track title. */
const char *music_title(int idx);

/* MU4: play the selected track (the engine). */
void music_play(void);

/* MU5: stop. */
void music_stop(void);

/* MU6: next / prev (wrap). */
void music_next(void);
void music_prev(void);

/* MU7: the playing state. */
int music_playing(void);
int music_selected(void);
void music_select(int idx);

/* MU8: the test hooks. */
void music_test_reset(void);
int  music_test_started(void);

/* the window bindings */
void music_draw(struct DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void music_key(struct DosGuiWindow *win, uint32_t key, uint32_t mods);
struct DosGuiWindow *music_launch(void);

#endif
