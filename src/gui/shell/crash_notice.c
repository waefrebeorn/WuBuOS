/*
 * crash_notice.c -- see the header. Small on purpose: the decision is a pure function, the state is a
 * single counter file, and the surface is the notification centre the shell already owns. The body
 * names the report directory, so the user can act on it without a second browser being written here.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "crash_notice.h"
#include "wubu_notify.h"
#include "../../runtime/wubu_crashcap.h"

/* Weak: this module is linked into graphs (and tests) that have no notification centre, and a crash
 * notice must never be the reason a boot fails to link. Same signature as the header's declaration. */
extern uint32_t wubu_notify_send(const Notification *n) __attribute__((weak));
/* Same weak treatment: the probe asks the centre what it holds, and a graph without the centre
 * (this module's own gate) must still link. */
extern int wubu_notify_active_count(void) __attribute__((weak));

int crash_notice_should_notify(int reports, int seen)
{
    if (reports <= 0) return 0;
    if (seen < 0) seen = 0;
    return reports > seen;
}

static void seen_path(char *out, size_t cap)
{
    snprintf(out, cap, "%s/.seen", wubu_crashcap_dir());
}

int crash_notice_seen_get(void)
{
    char p[600];
    seen_path(p, sizeof p);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    int n = 0;
    if (fscanf(f, "%d", &n) != 1) n = 0;
    fclose(f);
    return n < 0 ? 0 : n;
}

void crash_notice_seen_set(int n)
{
    char p[600];
    seen_path(p, sizeof p);
    FILE *f = fopen(p, "w");
    if (!f) return;
    fprintf(f, "%d\n", n < 0 ? 0 : n);
    fclose(f);
}

void crash_notice_on_boot(void)
{
    int reports = wubu_crashcap_count();
    int seen = crash_notice_seen_get();

    if (!crash_notice_should_notify(reports, seen)) {
        /* Nothing new. Keep the counter honest when the user deletes reports, so the next crash is
         * news again instead of being compared against a count that no longer exists. */
        if (reports < seen) crash_notice_seen_set(reports);
        return;
    }

    int fresh = reports - seen;
    /* WUBU_CRASH_PROBE makes "did we post, and did the surface take it" answerable from the log
     * instead of by counting pixels. */
    int probe = getenv("WUBU_CRASH_PROBE") != NULL;
    if (probe) fprintf(stderr, "[crash] reports=%d seen=%d fresh=%d sender=%s\n",
                       reports, seen, fresh, wubu_notify_send ? "present" : "NULL");
    if (wubu_notify_send) {
        Notification n;
        memset(&n, 0, sizeof n);
        snprintf(n.app_name, sizeof n.app_name, "WuBuOS");
        snprintf(n.summary, sizeof n.summary, "%d new crash report%s",
                 fresh, fresh == 1 ? "" : "s");
        snprintf(n.body, sizeof n.body,
                 "An application stopped unexpectedly. Reports are in %s",
                 wubu_crashcap_dir());
        n.urgency = NOTIFY_URGENCY_NORMAL;
        n.timeout = 10000;          /* ms; long enough to read a path, not permanent */
        uint32_t id = wubu_notify_send(&n);
        if (probe) fprintf(stderr, "[crash] notify id=%u summary='%s' centre_now=%d\n",
                           id, n.summary,
                           wubu_notify_active_count ? wubu_notify_active_count() : -1);
    }
    crash_notice_seen_set(reports);
}
