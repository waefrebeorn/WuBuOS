/*
 * wubu_session_boot.h -- P3 session-boot orchestrator: the cinnamon-session
 * analogue that OWNs the message bus and RUNS the XDG autostart .desktop apps
 * at login (Mint baseline §3.2 / §1: "cinnamon-session ... start all
 * applications" defined in the session or autostart dirs).
 *
 * It ties together the two P3 primitives:
 *   - wubu_xdg_autostart scans the autostart dirs into launchable entries
 *   - wubu_bus owns each launched app's well-known name, so after boot the
 *     running background app is addressable on the bus (the DE can message it)
 * The launch is INJECTABLE: a caller supplies a `wubu_sboot_launch_fn` that
 * decides HOW to start the app (pure exec, or through wubu_exec / a container).
 * The boot logic itself is proven by recording which entries a fake launcher
 * was asked to start and in what order.
 *
 * C11, runtime host. Not thread-safe by design (call from one driver loop).
 */
#ifndef WUBU_SESSION_BOOT_H
#define WUBU_SESSION_BOOT_H

#include <stddef.h>
#include "wubu_bus.h"
#include "wubu_xdg_autostart.h"

typedef struct wubu_session_boot wubu_session_boot_t;

/* Called for each launchable autostart entry at session start. Return 0 on
 * success (the app started); nonzero if the boot should record a failure.
 * `entry` is owned by the boot; `argv` is a parsed exec (NULL-terminated,
 * valid only during the call — copy if you need it longer). */
typedef int (*wubu_sboot_launch_fn)(wubu_session_boot_t *boot,
                                    const wubu_xdga_entry_t *entry,
                                    char **argv, void *ud);

/* Lifecycle --------------------------------------------------------------- */
wubu_session_boot_t *wubu_session_boot_create(void);
void wubu_session_boot_destroy(wubu_session_boot_t *boot);

/* Configuration ----------------------------------------------------------- */
void wubu_session_boot_set_launcher(wubu_session_boot_t *boot,
                                    wubu_sboot_launch_fn fn, void *ud);
/* The bus the boot owns (for callers that want to publish other names into
 * the same session bus, or read ownership afterwards). */
wubu_bus_t *wubu_session_boot_bus(wubu_session_boot_t *boot);

/* Session start ----------------------------------------------------------- */
/* Scan the autostart dirs (post-add config). Returns the count of launchable
 * apps recorded, or -1 if nothing could be scanned. */
int wubu_session_boot_scan(wubu_session_boot_t *boot,
                           char *const *dirs, int n_dirs);
/* For each launchable entry: claim its well-known bus name, build the argv,
 * and call the launcher. Returns how many apps were successfully started
 * (launcher returned 0). -1 if boot has no launcher set or no entries. */
int wubu_session_boot_start_all(wubu_session_boot_t *boot);

/* Inspection (post-scan) -------------------------------------------------- */
int wubu_session_boot_count(const wubu_session_boot_t *boot);
const wubu_xdga_entry_t *wubu_session_boot_entry(const wubu_session_boot_t *boot,
                                                 int i);

#endif /* WUBU_SESSION_BOOT_H */
