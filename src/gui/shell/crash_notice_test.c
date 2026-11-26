/*
 * crash_notice_test.c -- being told about a crash exactly once per new crash.
 *
 * The failure this guards is not "no notification" but the wrong ones: a notice on every boot
 * (noise that trains people to dismiss the tray), or no notice after the count changes because the
 * counter was compared against a stale number. The decision is a pure function and the state is one
 * counter file, so both are driven here without a desktop.
 *
 * C11.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "crash_notice.h"
#include "../../runtime/wubu_crashcap.h"

static int g_pass, g_fail;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) { g_pass++; }                                               \
        else { g_fail++; fprintf(stderr, "  FAIL: %s\n", (msg)); }            \
    } while (0)

static void rmrf(const char *p)
{
    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", p);
    if (system(cmd) != 0) { /* best effort */ }
}

static void write_report(const char *dir, const char *name)
{
    char p[600];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "w");
    if (f) { fprintf(f, "wubuos crash report\n"); fclose(f); }
}

int main(void)
{
    char base[] = "/tmp/wubu-crashnotice-XXXXXX";
    char *root = mkdtemp(base);
    if (!root) { fprintf(stderr, "mkdtemp failed\n"); return 1; }
    setenv("WUBU_CRASH_DIR", root, 1);

    printf("=== crash_notice_test (told once per new crash, never on every boot) ===\n");

    /* 1. The decision, in isolation. */
    CHECK(crash_notice_should_notify(0, 0) == 0, "no reports, nothing to say");
    CHECK(crash_notice_should_notify(0, 5) == 0, "no reports is never news");
    CHECK(crash_notice_should_notify(3, 0) == 1, "reports the user never saw are news");
    CHECK(crash_notice_should_notify(3, 3) == 0, "the same reports are not news twice");
    CHECK(crash_notice_should_notify(4, 3) == 1, "one more report is news again");
    CHECK(crash_notice_should_notify(2, 9) == 0, "fewer reports than seen is not news");

    /* 2. The counter file round-trips. */
    CHECK(crash_notice_seen_get() == 0, "a missing counter reads as zero");
    crash_notice_seen_set(7);
    CHECK(crash_notice_seen_get() == 7, "the counter round-trips");

    /* 3. Boot with no reports leaves the counter at zero and says nothing. */
    crash_notice_seen_set(0);
    crash_notice_on_boot();
    CHECK(crash_notice_seen_get() == 0, "a boot with no reports records no count");

    /* 4. Two reports appear: the boot hook records them (the notification itself needs a shell, so
     *    what is asserted here is the state transition the notification keys off). */
    write_report(root, "crash-1-1.txt");
    write_report(root, "crash-2-2.txt");
    CHECK(wubu_crashcap_count() == 2, "the reports are visible to the counter");
    crash_notice_on_boot();
    CHECK(crash_notice_seen_get() == 2, "the boot hook recorded the two reports");
    CHECK(crash_notice_should_notify(wubu_crashcap_count(), crash_notice_seen_get()) == 0,
          "the next boot is not news again");

    /* 5. A third crash is news; deleting them all re-arms the count. */
    write_report(root, "crash-3-3.txt");
    CHECK(crash_notice_should_notify(wubu_crashcap_count(), crash_notice_seen_get()) == 1,
          "a new report after a recorded boot is news");
    rmrf(root);
    crash_notice_on_boot();
    CHECK(crash_notice_seen_get() == 0,
          "deleting every report resets the counter so the next crash is news");

    printf("\n Results: %d/%d passed, %d failed\n", g_pass, g_pass + g_fail, g_fail);
    return g_fail ? 1 : 0;
}
