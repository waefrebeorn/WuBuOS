/*
 * wubu_lock_screen.h -- Win+L lock screen
 *
 * The desktop had no way to lock: walk away and the session is fully live.
 * Win+L engages a full-screen overlay showing the clock; while locked the WM
 * routes keys to the lock screen and swallows everything else, so no app
 * receives input.  Enter (or Win+L again) unlocks.
 *
 * There is no authentication yet -- this is the lock/unlock surface, which is
 * the part a user feels.  The credential check plugs into
 * wubu_lock_screen_handle_key().
 */

#ifndef WUBU_LOCK_SCREEN_H
#define WUBU_LOCK_SCREEN_H

#include <stdint.h>
#include <stdbool.h>

void wubu_lock_screen_lock(void);
void wubu_lock_screen_unlock(void);
bool wubu_lock_screen_is_locked(void);

/* Master switch: a hosted session is locked by the host desktop, not by the
 * guest.  Disabled, Win+L is inert and the overlay never draws. */
void wubu_lock_screen_set_enabled(bool enabled);
bool wubu_lock_screen_is_enabled(void);

/* Returns true when the key was consumed by the lock screen (i.e. the caller
 * must not dispatch it to any window). */
bool wubu_lock_screen_handle_key(uint32_t key, uint32_t mods);

void wubu_lock_screen_render(uint32_t *fb, int fb_w, int fb_h);

#endif /* WUBU_LOCK_SCREEN_H */
