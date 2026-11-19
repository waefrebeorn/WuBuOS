/*
 * wubu_ns_guests.h -- /n/guests: containers as first-class namespace citizens.
 *
 * THE PLAY: every other OS lists/launches containers through a DIFFERENT
 * mechanism (docker CLI + socket, systemd-machined + D-Bus, lxc command set).
 * WuBuOS does it through the SAME uniform filesystem namespace as everything
 * else under /n:
 *
 *   docker ps                ->  cat /n/guests/list
 *   docker inspect <name>    ->  cat /n/guests/<name>/state
 *   docker run <name> <cmd>  ->  echo '<cmd>' > /n/guests/<name>/launch
 *
 * list   = one line per RUNNING guest: name, pid, root, personality, state.
 * state  = live state snapshot of one guest (state/pid/root/personality/styx).
 * launch = write-to-launch: a 9P write of a command line starts a guest with
 *          that name through the SAME validated exec path as the in-process
 *          API (wubu_ct_create + wubu_ct_set_cmd + wubu_ct_start -- execv,
 *          never /bin/sh -c on the raw string).
 *
 * Single owner: the live-container registry in wubu_host_exec.c (where ct
 * state already lives). This module renders from its accessors on every
 * sync -- no private mirror of container state. Containers launched THROUGH
 * the namespace are owned by this module (reaped + destroyed on exit).
 *
 * C11, minimal includes. Links only the FS core (wubu_ns_fs.o) + the
 * container lifecycle (wubu_host_exec.o) -- no archd/bottles needed.
 */
#ifndef WUBU_NS_GUESTS_H
#define WUBU_NS_GUESTS_H

#include <stdint.h>
#include <stddef.h>
#include "wubu_host_exec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Render /n/guests from the LIVE registry: guests/list + per-guest
 * {state,launch}; remove entries for guests that are no longer running;
 * reap (destroy) namespace-launched guests that have exited.
 * Returns 0 on success, -1 if the namespace root is unset. */
int wubu_ns_guests_sync(void);

/* Launch a guest named `name` running `cmdline` through the existing
 * container exec path. cmdline is split quote-aware into argv (execv
 * semantics -- no shell interprets the raw string). Refuses invalid names,
 * empty commands, and names already running. Returns 0 on success, -1 on
 * refusal/failure. This is what a 9P write to guests/<name>/launch calls. */
int wubu_ns_guests_launch(const char *name, const char *cmdline);

/* -- 9P seams (real definitions here; weak no-ops in the callers) -------- */

/* Reachability probe for the Styx walk callback: returns 1 and sets
 * *qid_type when `path` is a synthetic guests control path that need not
 * exist on disk (guests, guests/<name>, guests/<name>/{state,launch} for a
 * valid name). Returns 0 otherwise. Lets a client walk to the launch file
 * of a guest that is not running yet. */
int wubu_ns_guests_probe(const char *path, uint8_t *qid_type);

/* Control-write dispatch for the Styx write callback: returns 1 when the
 * write was handled (a launch), -1 when it was handled and REFUSED, 0 when
 * the path is not a guests control file (caller falls through to disk). */
int wubu_ns_guests_write(const char *path, const uint8_t *data,
                         uint32_t count, uint32_t *nwritten);

/* Ownership hook: wubu_ct_destroy() calls this BEFORE freeing a ct so the
 * module drops any owned pointer to it (no dangling reap). */
void wubu_ns_guests_forget(WubuCt *ct);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_NS_GUESTS_H */
