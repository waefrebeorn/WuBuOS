/*
 * wubu_ns_background.h -- the /n/background control subtree (P3).
 *
 * Exposes the session-boot / message-bus / autostart state as files under
 * /n/background so the running session is OBSERVABLE and CONTROLLABLE through
 * the 9P namespace (the WuBuOS 'one namespace' thesis). Mirrors the
 * /n/ec pattern (wubu_ns_ec): a publish function writes the flat state files
 * via the shared ns_mkdir/ns_write primitives (wubu_ns_fs.c) — no separate
 * daemon; the session owner calls wubu_ns_publish_background() after a boot
 * and on state change.
 *
 * Tree:
 *   /n/background/session       the active session name (game|desktop)
 *   /n/background/bus/names     one well-known bus name per line (owned)
 *   /n/background/bus/nconn     number of live bus connections
 *   /n/background/autostart/N   each launchable app's Name= (N = 0,1,...)
 *   /n/background/autostart/count  how many launchable apps were found
 *
 * C11, runtime host. Links ONLY wubu_ns_fs.c + the caller's session modules
 * (see the memory-constrained rule: do NOT link wubu_ns_bridge.o — set
 * g_ns_root directly in the test).
 */
#ifndef WUBU_NS_BACKGROUND_H
#define WUBU_NS_BACKGROUND_H

#include <stddef.h>
#include "wubu_bus.h"
#include "wubu_session_boot.h"
#include "wubu_logind.h"
#include "wubu_auth.h"

/* Publish the /n/background tree from a session-boot's live bus + entries.
 * Writes the state snapshot. Returns 0 on success, -1 on any write failure. */
int wubu_ns_publish_background(wubu_session_boot_t *boot);

/* Publish /n/background/session state from the logind-equivalent registry:
 * session/active, session/seat, session/n  (the active session's seat/type via
 * short strings; plus the per-session N listing). See §153. */
int wubu_ns_publish_login(wubu_logindd_t *login);

/* Publish /n/background/auth from the polkit-equivalent authority (the
 * subject→action rules as `subject:action=allow|deny|interact` lines + a
 * `has_agent` file). See §154. */
int wubu_ns_publish_auth(wubu_auth_t *auth);

#endif /* WUBU_NS_BACKGROUND_H */
