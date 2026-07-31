/*
 * wubu_realm.c -- Mega OS realm abstraction (Phase B + D).
 * Lifecycle via the supervisor (N1-N4/N8/N9); boundary EDR via an immutable
 * trace store. bytropix kernel schema is checked fail-closed (DA-2).
 */
#include "wubu_realm.h"
#include "wubu_archd_svc.h"   /* supervisor reuse */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct wubu_realm {
    char name[64];
    wubu_realm_personality_t pers;
    wubu_realm_backend_t backend;
    wubu_trace_store_t *trace;     /* boundary EDR observation (DA-1/DA-2) */
    wubu_svc_supervisor_t *sup;   /* lifecycle (N1-N4/N8/N9) */
    bool ready;
    int kernel_major;              /* from bytropix; -1 = not yet negotiated */
};

wubu_realm_t *wubu_realm_create(const char *name,
                                 wubu_realm_personality_t pers,
                                 wubu_realm_backend_t backend) {
    if (!name || pers >= REALM_COUNT || backend >= REALM_BACKEND_COUNT) return NULL;
    wubu_realm_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    strncpy(r->name, name, sizeof(r->name) - 1);
    r->pers = pers;
    r->backend = backend;
    r->kernel_major = -1;
    r->ready = false;
    /* One trace store per realm; realm_id = stable hash of name. */
    int realm_id = (int)(0x9e37 * (unsigned)(name[0] ? name[0] : 1)
                         + strlen(name)) % 100000;
    r->trace = wubu_trace_create(realm_id);
    if (!r->trace) { free(r); return NULL; }
    r->sup = wubu_svc_supervisor_create();
    if (!r->sup) { wubu_trace_destroy(r->trace); free(r); return NULL; }
    return r;
}

void wubu_realm_destroy(wubu_realm_t *r) {
    if (!r) return;
    wubu_svc_supervisor_destroy(r->sup);
    wubu_trace_destroy(r->trace);
    free(r);
}

int wubu_realm_start(wubu_realm_t *r, const char *exec, char *const argv[]) {
    if (!r || !exec) return -1;
    if (r->kernel_major != -1 && r->kernel_major != WUBU_TRACE_SCHEMA_MAJOR)
        return -1;   /* DA-2: fail closed on schema mismatch */
    /* Supervisor fork/exec is the realm's process; Type=notify default => truthful. */
    wubu_svc_supervisor_set_type(r->sup, "/", r->name, SVC_TYPE_NOTIFY);
    int rc = wubu_svc_supervisor_add(r->sup, "/", r->name, exec, argv);
    if (rc != 0) return -1;
    rc = wubu_svc_supervisor_start(r->sup, "/", r->name);
    if (rc != 0) return -1;
    /* Boundary EDR: log the spawn at the realm boundary (observer outside realm). */
    char buf[256];
    snprintf(buf, sizeof(buf), "realm %s (%d) spawned %s backend=%d",
             r->name, r->pers, exec, r->backend);
    wubu_realm_observe(r, WUBU_TRACE_PROCESS, 0, buf);
    return 0;
}

int wubu_realm_stop(wubu_realm_t *r) {
    if (!r) return -1;
    return wubu_svc_supervisor_stop(r->sup, "/", r->name);
}

int wubu_realm_status(wubu_realm_t *r, int *state_out, int *pid_out) {
    if (!r) return -1;
    WubuArchService st;
    memset(&st, 0, sizeof(st));
    int rc = wubu_svc_supervisor_status(r->sup, "/", r->name, &st);
    if (state_out) *state_out = st.state;
    if (pid_out)   *pid_out   = st.pid;
    return rc;
}

void wubu_realm_notify_ready(wubu_realm_t *r) {
    if (!r) return;
    wubu_svc_supervisor_notify_ready(r->sup, "/", r->name);
    r->ready = true;
    wubu_realm_observe(r, WUBU_TRACE_AGENT, 0, "realm reported READY (truthful)");
}

int wubu_realm_observe(wubu_realm_t *r, wubu_trace_kind_t kind, uint64_t parent,
                        const char *payload) {
    if (!r) return -1;
    /* IMMUTABLE append at the boundary; the realm cannot rewrite this. */
    return wubu_trace_append(r->trace, kind, parent, payload);
}

const wubu_trace_store_t *wubu_realm_trace(const wubu_realm_t *r) {
    return r ? r->trace : NULL;
}

int wubu_realm_purge_traces(wubu_realm_t *r) {
    return r ? wubu_trace_purge(r->trace) : 0;
}
void wubu_realm_set_frozen(wubu_realm_t *r, bool frozen) {
    if (r) wubu_trace_set_frozen(r->trace, frozen);
}

void wubu_realm_set_kernel_schema(wubu_realm_t *r, int kernel_major) {
    if (r) r->kernel_major = kernel_major;
}
int wubu_realm_kernel_schema_ok(const wubu_realm_t *r) {
    if (!r) return 0;
    return (r->kernel_major == -1) || (r->kernel_major == WUBU_TRACE_SCHEMA_MAJOR);
}
