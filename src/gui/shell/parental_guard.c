/*
 * parental_guard.c -- see parental_guard.h.
 *
 * C11.
 */
#include "parental_guard.h"
#include "wubu_lock_screen.h"
#include "../../runtime/wubu_parental.h"

#include <string.h>

static time_t g_last_check;
static bool   g_locked_for_limit;
static bool   g_reenable_host_lock;   /* the session's own lock state before a limit required one */

void wubu_parental_guard_reset(void)
{
    g_last_check = 0;
    g_locked_for_limit = false;
}

bool wubu_parental_enforce(time_t now, int interval_s)
{
    if (interval_s < 1) interval_s = 1;
    if (g_last_check != 0 && now - g_last_check < interval_s) return false;
    g_last_check = now;

    WubuParentalPolicy p;
    if (wubu_parental_load(&p) != 0 || !p.enabled) {
        /* Turning the limit off releases a lock it put there, and only that lock: a session the user
         * locked with Win+L stays locked. */
        if (g_locked_for_limit && wubu_lock_screen_is_locked()) wubu_lock_screen_unlock();
        if (g_reenable_host_lock) {
            wubu_lock_screen_set_enabled(false);   /* hosted sessions are locked by the host, not us */
            g_reenable_host_lock = false;
        }
        g_locked_for_limit = false;
        return true;
    }

    struct tm local;
    memset(&local, 0, sizeof local);
    localtime_r(&now, &local);

    char why[160];
    if (!wubu_parental_allowed(&p, &local, why, sizeof why)) {
        if (!wubu_lock_screen_is_locked()) {
            /* A family limit is the guest's own rule, so the overlay must work even in a session whose
             * lock screen is normally the host's: remembered, and put back when the limit lifts. */
            if (!wubu_lock_screen_is_enabled()) {
                wubu_lock_screen_set_enabled(true);
                g_reenable_host_lock = true;
            }
            wubu_lock_screen_set_reason(why);
            wubu_lock_screen_lock();
            g_locked_for_limit = true;
        }
    } else if (g_locked_for_limit) {
        /* Back inside the window: a limit that keeps the machine locked after the allowed hours begin
         * is indistinguishable from a broken machine. */
        if (wubu_lock_screen_is_locked()) wubu_lock_screen_unlock();
        if (g_reenable_host_lock) {
            wubu_lock_screen_set_enabled(false);
            g_reenable_host_lock = false;
        }
        g_locked_for_limit = false;
    }
    return true;
}
