/*
 * wubu_realm.h -- Mega OS realm abstraction (Phase B + D of MEGA_COMPAT_PLANNING.md).
 *
 * A realm is a sandboxed execution context hosting ONE OS personality (win98/xp/dos/
 * templeos/mac/cpm/linux/agent) inside a microVM or gVisor Sentry. The hypervisor/
 * guest-kernel boundary IS the EDR (DA-1/DA-2): observation happens outside the
 * observed realm, so an escaped/adversarial realm cannot blind its own telemetry.
 *
 * Reuses the supervisor (N1-N4/N8/N9) for lifecycle. bytropix (kernel) emits the
 * boundary events; we consume them here. The contract (/n/trace schema) fails CLOSED
 * if bytropix's major version mismatches (DA-2).
 *
 * Opaque struct, C11, minimal includes.
 */
#ifndef WUBU_REALM_H
#define WUBU_REALM_H

#include "wubu_trace.h"

typedef enum {
    REALM_WIN98 = 0,
    REALM_XP,
    REALM_DOS,
    REALM_TEMPLEOS,
    REALM_MAC,
    REALM_CPM,
    REALM_LINUX,
    REALM_AGENT,
    REALM_COUNT
} wubu_realm_personality_t;

typedef enum {
    REALM_BACKEND_MICROVM = 0,  /* libkrun/Firecracker class (hardware boundary) */
    REALM_BACKEND_GVISOR,       /* user-space kernel boundary */
    REALM_BACKEND_INPROC,       /* native / co-resident (e.g. HolyD, host bin) */
    REALM_BACKEND_COUNT
} wubu_realm_backend_t;

typedef struct wubu_realm wubu_realm_t;

/* Create a realm. name is a short id (e.g. "win98-main"). Returns NULL on error. */
wubu_realm_t *wubu_realm_create(const char *name,
                                 wubu_realm_personality_t pers,
                                 wubu_realm_backend_t backend);
void wubu_realm_destroy(wubu_realm_t *r);

int  wubu_realm_start(wubu_realm_t *r, const char *exec, char *const argv[]);
int  wubu_realm_stop(wubu_realm_t *r);
/* Truthful status (N9): STARTING until ready, RUNNING only after notify. */
int  wubu_realm_status(wubu_realm_t *r, int *state_out, int *pid_out);
void wubu_realm_notify_ready(wubu_realm_t *r);

/* EDR boundary observation (DA-1/DA-2): record a boundary-crossing event into
 * this realm's IMMUTABLE trace. kind is the event class; payload is grepable text.
 * Returns 0 on success. */
int wubu_realm_observe(wubu_realm_t *r, wubu_trace_kind_t kind, uint64_t parent,
                        const char *payload);

/* Plain-text trace access for the operator/EDR verifier. */
const wubu_trace_store_t *wubu_realm_trace(const wubu_realm_t *r);

/* User ownership (DA-1): purge this realm's traces; freeze (DA-3). */
int  wubu_realm_purge_traces(wubu_realm_t *r);
void wubu_realm_set_frozen(wubu_realm_t *r, bool frozen);

/* bytropix contract (DA-2): the kernel publishes a schema major; if it differs
 * from ours the realm refuses to start (fail-closed, no silent telemetry gap). */
void wubu_realm_set_kernel_schema(wubu_realm_t *r, int kernel_major);
int  wubu_realm_kernel_schema_ok(const wubu_realm_t *r);

#endif /* WUBU_REALM_H */
