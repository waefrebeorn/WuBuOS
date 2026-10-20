/*
 * wubu_ns_pid.h -- per-task namespace subtree at /n/pid/{tid}.
 *
 * The process-facing registry of per-task /n views (NAMESPACE #598):
 * each task gets an isolated subtree /n/pid/<tid> bound to its own
 * storage base, implemented as a wubu_ns_root_t view created and
 * destroyed with the task's lifecycle. Task A's view cannot see task
 * B's subtree -- isolation comes from the root view, not from
 * convention.
 *
 * Owner boundaries: wubu_ns_root.c owns the view/grant mechanics (this
 * module only creates + registers views); wubu_ns_mount.c owns the
 * shared bind table; wubu_ns_fs.c owns the on-disk /n tree. The
 * canonical path shape /n/pid/{tid} is defined HERE.
 *
 * C11, opaque registry, hosted-portable.
 */
#ifndef WUBU_NS_PID_H
#define WUBU_NS_PID_H

#include <stdint.h>
#include "wubu_ns_root.h"

#define WUBU_NS_PID_MAX_TASKS 64

typedef struct wubu_ns_pid_registry wubu_ns_pid_registry_t;

/* Reset the registry (destroys every view it holds). */
void wubu_ns_pid_reset(void);

/* Register task `tid`: create an isolated view and grant it its storage
 * base at /n/pid/<tid>. Duplicate tid is rejected (-2); full registry
 * -3; bad args -1. Returns 0 on success. */
int  wubu_ns_pid_register(uint64_t tid, const char *src_base);

/* The task's view (NULL when absent). Lookup through the view resolves
 * /n/pid/<tid>/... paths to the task's own storage. */
wubu_ns_root_t *wubu_ns_pid_lookup(uint64_t tid);

/* Unregister (task exit): destroys the view. 0 removed, -1 no such tid. */
int  wubu_ns_pid_unregister(uint64_t tid);

/* Live task subtrees. */
int  wubu_ns_pid_count(void);

/* The canonical subtree path for a tid: "/n/pid/<tid>" written into
 * out (must hold >= 32 bytes). NULL on bad args. */
const char *wubu_ns_pid_path(uint64_t tid, char *out, size_t cap);

#endif /* WUBU_NS_PID_H */
