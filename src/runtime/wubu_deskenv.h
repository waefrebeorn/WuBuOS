/*
 * wubu_deskenv.h -- the /n/background/de DE-daemon framework surface (Mint
 * baseline §154 / §5).
 *
 * The DE-background framework wraps two ownership roles the desktop depends on:
 *   - the TRAY / status-icon OWNER: xapp-sn-watcher / XApp.StatusIcon — an app
 *     that claims a named tray slot owns its icon+menu. A single owner holds a
 *     slot (like claiming a bus name); a late claimant is refused.
 *   - the DESKTOP OWNER: nemo-desktop — the single process that owns
 *     desktop-icons + the context menu. Exactly one owner at a time.
 *
 * This module models that ownership and publishes it as /n/background/de:
 *   /n/background/de/tray           -> "<slot> <owner>" per line (claimed)
 *   /n/background/de/traycount      -> how many slots are claimed
 *   /n/background/de/desktop        -> the desktop owner (or "none")
 *
 * It is an OWNERSHIP MODEL + control surface, decoupled from the GUI painter
 * (systray renders; this records who OWNS each slot — the XApp
 * contract). The GUI registers its tray/desktop claims here so the owning
 * process is visible through /n regardless of who draws it.
 *
 * C11, runtime host, single-threaded. Links only wubu_ns_fs.c for publish.
 */
#ifndef WUBU_NS_DE_H
#define WUBU_NS_DE_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_DE_MAX_SLOTS  32
#define WUBU_DE_MAX_STR    64

typedef struct wubu_deskenv wubu_deskenv_t;

wubu_deskenv_t *wubu_deskenv_create(void);
void wubu_deskenv_destroy(wubu_deskenv_t *de);

/* Tray slot ownership (XApp.StatusIcon contract) --------------------------- */
/* Claim `slot` for `owner`. Returns 0 on success, -1 if the slot is already
 * owned (by a different owner) or the table is full. Re-claim by the SAME
 * owner is a no-op success (idempotent). */
int wubu_deskenv_tray_claim(wubu_deskenv_t *de, const char *slot, const char *owner);
/* Release `slot` if it belongs to `owner`. 0 on success, -1 otherwise. */
int wubu_deskenv_tray_release(wubu_deskenv_t *de, const char *slot, const char *owner);
/* Who owns `slot`? NULL if unowned. */
const char *wubu_deskenv_tray_owner(const wubu_deskenv_t *de, const char *slot);
/* How many tray slots are claimed? */
int wubu_deskenv_tray_count(const wubu_deskenv_t *de);

/* Desktop ownership (nemo-desktop role) ------------------------------------ */
/* Set the desktop owner. Returns 0; overrides any prior owner. */
int wubu_deskenv_desktop_set(wubu_deskenv_t *de, const char *owner);
/* Clear the desktop owner. */
void wubu_deskenv_desktop_clear(wubu_deskenv_t *de);
/* The current desktop owner, or NULL. */
const char *wubu_deskenv_desktop_owner(const wubu_deskenv_t *de);

/* Control surface ---------------------------------------------------------- */
/* Publish /n/background/de/tray + /n/background/de/desktop. 0 on success. */
int wubu_ns_publish_deskenv(wubu_deskenv_t *de);

#endif /* WUBU_NS_DE_H */
