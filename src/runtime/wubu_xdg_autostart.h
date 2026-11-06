/*
 * wubu_xdg_autostart.h -- XDG Autostart (.desktop) launcher for the P3
 * /n/background runtime.
 *
 * Implements the XDG Autostart contract from the Mint baseline study
 * (os-studies/linuxmint_background_baseline.md §3.2): "By placing an
 * application's .desktop file in one of the [XDG Autostart] directories the
 * application will be automatically launched during startup of the user's
 * desktop environment after the user has logged in." This is the piece that
 * turns `.desktop` files in the autostart dirs into running background apps,
 * the way cinnamon-session does on Mint.
 *
 * Scope here is the READ + LAUNCH decision (the .desktop parser + the Exec
 * extraction). It is deliberately NOT a full freedesktop implementation: only
 * the keys that determine WHETHER and HOW to autostart an app:
 *   Type, Name, Exec, Hidden, OnlyShowIn, NotShowIn, X-GNOME-Autostart-enabled
 * (X-GNOME-Autostart-enabled=0 is the "disabled in Startup Applications" out,
 * the most common override a user sets).
 *
 * Launch is returned as an argv (host-style: the Exec line split on the
 * field-code escapes the freedesktop spec defines — %f/%F/%u/%U/%d/%D/%n/%N/
 * %i/%c/%k removed-or-replaced). The caller chooses how to exec; a pure
 * fork/exec helper is provided but DECOUPLED so a caller can run it through
 * wubu_exec (a container/personality) instead. No shell escaping (Exec is a
 * full argv already, no /bin/sh involved).
 */
#ifndef WUBU_XDG_AUTOSTART_H
#define WUBU_XDG_AUTOSTART_H

#include <stddef.h>

#define WUBU_XDGA_MAX_ABS    64   /* autostart dirs scanned */
#define WUBU_XDGA_MAX_ENTRY  256  /* .desktop files per scan */

typedef struct {
    char name[512];    /* Name=  (human label) */
    char exec[1024];   /* Exec=  (the launch command line) */
    int  hidden;       /* Hidden=true -> never show (skip) */
    int  enabled;      /* X-GNOME-Autostart-enabled=false -> skip */
    char path[1040];   /* full path of the .desktop file (for provenance) */
} wubu_xdga_entry_t;

/* Scan `dir` (an existing autostart directory, e.g. "/etc/xdg/autostart")
 * for *.desktop files and record those that pass the autostart test
 * (Type=Application-or-absent, not Hidden, not disabled). `out` holds up to
 * `max` entries; returns how many were recorded (>=0), or -1 if dir missing. */
int wubu_xdga_scan_dir(const char *dir, wubu_xdga_entry_t *out, int max);

/* Scan a searched list of autostart dirs in order; a later dir does NOT
 * override an earlier one (the freedesktop autostart dirs are searched in
 * order and, unlike icons, later files do not shadow earlier ones here — we
 * keep the first launchable per basename the way cinnamon-session's merge
 * resolves XGnomeAutostartPhase). Returns the total recorded. */
int wubu_xdga_scan_paths(char *const *dirs, int n_dirs,
                         wubu_xdga_entry_t *out, int max);

/* Build the argv for `entry` as a NULL-terminated array. Field codes are
 * expanded per the freedesktop spec: %f/%F/%u/%U/%d/%D/%n/%N/%i/%c/%k are
 * removed (no file args in autostart context) and %% becomes a literal %.
 * Returns the number of tokens written into argv (argv must hold 65 entries).
 * argv is filled with pointers into parse-owned scratch stacked in argv[0..]
 * area -- declared as `char *argv[65]` by the caller. */
int wubu_xdga_build_argv(const wubu_xdga_entry_t *entry, char **argv);

/* Pure fork/exec of entry->exec as an autostart app (orphan it into its own
 * session so it outlives the caller — the way cinnamon-session starts both
 * the app and the DE daemons). Returns the child PID or -1. */
int wubu_xdga_launch(const wubu_xdga_entry_t *entry);

#endif /* WUBU_XDG_AUTOSTART_H */
