/*
 * wubu_secmon.h — the kernel's syscall camera (call interception for the AGI).
 *
 * Wine/proton run ON the kernel, but the AGI must SEE what they do.
 * This is the interception layer: a ptrace-syscall supervisor that
 * captures every syscall a game process makes and streams it into the
 * KV-FS at /kv/agent/sys_<pid>/<seq> as 6-float vectors.
 *
 * Vector layout (per syscall, floats):
 *   [0] = kind  (0=enter, 1=exit)
 *   [1] = syscall nr
 *   [2] = arg0 (packed: first 4 bytes of arg0 as float)
 *   [3] = arg1
 *   [4] = retval (on exit) / arg2 (on enter)
 *   [5] = pid
 *
 * The Brain reads /n/kv/agent/sys_* over 9P and learns the game's
 * behavior stream — every open/read/write/ioctl/socket/execve — the
 * sensory cortex of the AGI loop.
 *
 * The KV-FS itself is kernel-owned (src/kernel/wubu_kvfs.{c,h}); this
 * runtime module observes guest processes and streams their syscalls
 * into the kernel's KV tensor via the wubu_kvfs_write() API.
 *
 * C11, opaque struct, minimal includes.
 */
#ifndef WUBU_SECMON_H
#define WUBU_SECMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct wubu_secmon wubu_secmon_t;

/* Create a syscall monitor (no capture yet). Returns NULL on alloc failure. */
wubu_secmon_t *wubu_secmon_create(int realm_id);

/* Free the monitor (stops any active capture). */
void wubu_secmon_destroy(wubu_secmon_t *m);

/* Attach to a running process and intercept its syscalls.
 * Returns 0 on success, -1 if ptrace attach failed.
 * The monitor runs in the caller's thread: call wubu_secmon_poll
 * repeatedly (or wubu_secmon_wait) to service the interception. */
int wubu_secmon_attach(wubu_secmon_t *m, pid_t pid);

/* Service one intercepted event (non-blocking). Returns:
 *   1  = an event was captured and written
 *   0  = no event pending (child still running)
 *  -1  = child exited (capture complete for this pid)
 */
int wubu_secmon_poll(wubu_secmon_t *m);

/* Blocking run: service until the traced child exits.
 * Returns total spans captured. */
int wubu_secmon_wait(wubu_secmon_t *m);

/* Detach (stop tracing) without killing the child. */
int wubu_secmon_detach(wubu_secmon_t *m);

/* Total syscalls captured so far. */
uint64_t wubu_secmon_count(const wubu_secmon_t *m);

/* Number of spans written into the KV-FS. */
uint64_t wubu_secmon_kv_writes(const wubu_secmon_t *m);

/* Enable/disable KV-FS streaming (default: on if g_wubu_kvfs is live).
 * Set 0 to capture without writing (dry-run). */
void wubu_secmon_set_kv_stream(wubu_secmon_t *m, bool on);

#endif /* WUBU_SECMON_H */
