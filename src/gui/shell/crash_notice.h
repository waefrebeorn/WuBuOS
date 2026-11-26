/*
 * crash_notice.h -- tell the user a crash happened, instead of burying it in a directory.
 *
 * wubu_crashcap writes a report; nobody reads a directory. The reference desktop surfaces crashes in
 * a notification and offers to open the details, so this closes the loop: on boot, if there are MORE
 * reports than the user has already been told about, post one notification with an action that opens
 * the report directory in Explorer. Being told once per new crash matters -- a notification on every
 * boot would be the kind of noise that trains people to dismiss the tray.
 */
#ifndef WUBU_CRASH_NOTICE_H
#define WUBU_CRASH_NOTICE_H

/* The decision, as a pure function so it can be tested without a desktop:
 * notify when there are reports and more of them than the last count the user saw. */
int crash_notice_should_notify(int reports, int seen);

/* The last count the user was told about (0 when never told / unreadable). */
int  crash_notice_seen_get(void);
void crash_notice_seen_set(int n);

/* Boot hook: read the count, post at most one notification, record what was told. */
void crash_notice_on_boot(void);

#endif /* WUBU_CRASH_NOTICE_H */
