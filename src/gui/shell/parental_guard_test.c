/*
 * parental_guard_test.c -- the guard is what the user FEELS: the session locks when the window closes,
 * says why, and unlocks when it opens again. The lock screen is stubbed so the gate measures the guard's
 * decisions, not the overlay's pixels (that overlay has its own gate).
 *
 * C11.
 */
#include "parental_guard.h"
#include "../../runtime/wubu_parental.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

/* -- stub lock screen ------------------------------------------------------------------------------ */
static bool s_locked;
static bool s_enabled = true;
static char s_reason[160];
static int  s_lock_calls, s_unlock_calls, s_enable_calls;

void wubu_lock_screen_lock(void) { s_locked = true; s_lock_calls++; }
void wubu_lock_screen_unlock(void) { s_locked = false; s_reason[0] = '\0'; s_unlock_calls++; }
bool wubu_lock_screen_is_locked(void) { return s_locked; }
void wubu_lock_screen_set_enabled(bool e) { s_enabled = e; s_enable_calls++; }
bool wubu_lock_screen_is_enabled(void) { return s_enabled; }
void wubu_lock_screen_set_reason(const char *why) { snprintf(s_reason, sizeof s_reason, "%s", why ? why : ""); }
const char *wubu_lock_screen_reason(void) { return s_reason; }

static time_t midnightish(int hour, int min, int wday)
{
    /* Build a real time_t whose localtime has the requested hour and weekday is not required: the
     * policy check uses localtime, so the gate sets the file to allow every day. */
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = 0;
    (void)wday;
    return mktime(&t);
}

static void write_policy(const char *conf, int enabled, const char *hours)
{
    FILE *f = fopen(conf, "w");
    if (!f) return;
    fprintf(f, "enabled=%d\nhours=%s\ndays=mon,tue,wed,thu,fri,sat,sun\n", enabled, hours);
    fclose(f);
}

int main(void)
{
    char root[] = "/tmp/pgtestXXXXXX";
    if (!mkdtemp(root)) { printf("mkdtemp failed\n"); return 1; }
    char conf[400];
    snprintf(conf, sizeof conf, "%s/parental.conf", root);
    setenv("WUBU_PARENTAL_CONF", conf, 1);

    printf("=== Test: no policy, no lock ===\n");
    wubu_parental_guard_reset();
    remove(conf);
    CHECK(wubu_parental_enforce(midnightish(3, 0, 3), 0), "the first call checks");
    CHECK(!s_locked, "a machine with no policy is never locked by a limit");
    CHECK(s_lock_calls == 0, "the lock screen was not touched");

    printf("\n=== Test: outside the window the session locks, with a reason ===\n");
    write_policy(conf, 1, "08:00-20:00");
    wubu_parental_guard_reset();
    time_t night = midnightish(3, 0, 3);
    CHECK(wubu_parental_enforce(night, 0), "the check runs");
    CHECK(s_locked, "03:00 outside 08:00-20:00 locks the session");
    CHECK(s_lock_calls == 1, "it locks once");
    CHECK(strstr(s_reason, "allowed hours") != NULL, "the lock screen is told why");
    CHECK(strstr(s_reason, "08:00-20:00") != NULL, "the reason names the window");

    printf("\n=== Test: the throttle answers 'did not look', not 'allowed' ===\n");
    CHECK(!wubu_parental_enforce(night + 5, 30), "a call inside the interval does not check");
    CHECK(wubu_parental_enforce(night + 31, 30), "a call past the interval does");
    CHECK(s_locked, "and the session stays locked");

    printf("\n=== Test: back inside the window, the limit's own lock releases ===\n");
    time_t day = midnightish(12, 0, 3);
    CHECK(wubu_parental_enforce(day, 0), "the check runs");
    CHECK(!s_locked, "noon is allowed, so the session unlocks");
    CHECK(s_unlock_calls >= 1, "the unlock actually happened");

    printf("\n=== Test: the user's own lock is not the limit's to release ===\n");
    s_locked = true;                       /* the user pressed Win+L */
    s_reason[0] = '\0';
    wubu_parental_guard_reset();
    CHECK(wubu_parental_enforce(day, 0), "the check runs");
    CHECK(s_locked, "a user's lock survives a policy that allows the current hour");

    printf("\n=== Test: a limit in a session whose lock screen is the host's ===\n");
    s_locked = false;
    s_enabled = false;                     /* hosted default: the host owns the lock */
    s_enable_calls = 0;
    wubu_parental_guard_reset();
    CHECK(wubu_parental_enforce(night, 0), "the check runs");
    CHECK(s_enabled, "the limit enables the guest lock it needs");
    CHECK(s_locked, "and locks");
    CHECK(s_enable_calls == 1, "it enables once");

    printf("\n=== Test: lifting the limit puts the session's own setting back ===\n");
    CHECK(wubu_parental_enforce(day, 0), "the check runs");
    CHECK(!s_locked, "the session unlocks");
    CHECK(!s_enabled, "the host-owned lock setting is restored, not left on");

    printf("\n=== Test: turning the policy off releases a limit lock, and only that ===\n");
    s_locked = false; s_enabled = true; wubu_parental_guard_reset();
    write_policy(conf, 1, "08:00-20:00");
    CHECK(wubu_parental_enforce(night, 0), "locked by the limit");
    CHECK(s_locked, "the session is locked");
    write_policy(conf, 0, "08:00-20:00");
    CHECK(wubu_parental_enforce(night + 40, 30), "the check runs after the policy changed");
    CHECK(!s_locked, "disabling the limit releases the lock it placed");

    char rm[400];
    snprintf(rm, sizeof rm, "rm -rf '%s'", root);
    if (system(rm) != 0) { /* best effort */ }

    printf("\n  parental_guard: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
