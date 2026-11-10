/*
 * wubu_lock_screen.c -- Win+L lock screen.
 *
 * See wubu_lock_screen.h.  The overlay is opaque and drawn above every window
 * and panel; the WM asks the lock screen to consume keys before dispatching
 * them anywhere, so a locked session delivers no input to any app.
 */

#include "wubu_lock_screen.h"
#include "../wm.h"
#include "../wm_internal.h"
#include "../kernel/vbe.h"
#include "../wubu_theme.h"
#include "../kernel/input.h"

#include <stdio.h>
#include <time.h>

static bool g_locked = false;

void wubu_lock_screen_lock(void)   { g_locked = true; }
void wubu_lock_screen_unlock(void) { g_locked = false; }
bool wubu_lock_screen_is_locked(void) { return g_locked; }

bool wubu_lock_screen_handle_key(uint32_t key, uint32_t mods) {
    if (!g_locked) return false;

    /* Enter or Win+L unlocks.  Everything else is swallowed -- that is the
     * whole point of a lock screen. */
    if (key == '\r' || key == '\n' ||
        ((mods & MOD_WIN) && (key == 0x26 || key == 'l' || key == 'L'))) {
        g_locked = false;
    }
    return true;
}

void wubu_lock_screen_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_locked) return;

    const WubuThemeColors *c = tc();

    /* Opaque surface: nothing behind the lock is visible. */
    vbe_fill_rect(0, 0, fb_w, fb_h, c->desktop_bg);

    /* Clock, centred. */
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char clock[32];
    snprintf(clock, sizeof(clock), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    int cw = (int)strlen(clock) * 8;
    vbe_draw_text((fb_w - cw) / 2, fb_h / 2 - 40, clock, 0xFFFFFF, 3);

    /* Prompt. */
    const char *prompt = "Locked -- press Enter to resume";
    int pw = (int)strlen(prompt) * 8;
    vbe_draw_text((fb_w - pw) / 2, fb_h / 2 + 20, prompt, c->btn_text, 1);
}
