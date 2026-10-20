/*
 * wubu_ns_audit.h -- namespace audit log: every 9P transaction, the
 * capability-denial trace, and rotation with deterministic replay.
 *
 * The namespace's trust spine. The 9P server (styxfs callbacks) records
 * each transaction here; refusals record the path + the rights that were
 * missing. Records live in sealed-able segments with a running FNV-1a-64
 * chain: rotation seals the active segment and starts a fresh one without
 * breaking the chain, replay walks all segments in sequence order and
 * reproduces the same digest every time, and verification recomputes the
 * chain so any altered record is caught.
 *
 * Owner boundaries: wubu_cap_audit.c owns the CAPABILITY-RESOLUTION denial
 * ring (pid/kind/check -- who was told no by the cap core); THIS module
 * owns the NAMESPACE view (which 9P op, on which path, and the
 * /n/audit/log + /n/audit/denied read-back surfaces). Those are the
 * canonical read paths; the byte-offset reads below are what a 9P Tread
 * on those files returns.
 *
 * NAMESPACE #581, #585, #594.
 *
 * C11, opaque struct, no dynamic allocation.
 */
#ifndef WUBU_NS_AUDIT_H
#define WUBU_NS_AUDIT_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_NS_AUDIT_SEGS      4     /* sealed segments + the active one */
#define WUBU_NS_AUDIT_CAP       256   /* records per segment */
#define WUBU_NS_AUDIT_TX_LEN    16    /* "Tversion", "Tremove", ... */
#define WUBU_NS_AUDIT_PATH_LEN  256

typedef struct wubu_ns_audit wubu_ns_audit_t;

wubu_ns_audit_t *wubu_ns_audit_new(void);
void wubu_ns_audit_free(wubu_ns_audit_t *a);

/* Record a 9P transaction. tx is the message name ("Twalk", "Tread", ...);
 * status is the op's result (0 = ok, negative = error code). Fail-closed:
 * -1 bad args, -2 segment full (rotate first). */
int  wubu_ns_audit_tx(wubu_ns_audit_t *a, const char *tx, uint64_t fid,
                      const char *path, int status);

/* Record a capability denial at a namespace path: the rights the caller
 * needed and did not have (#585). Same return codes. */
int  wubu_ns_audit_denial(wubu_ns_audit_t *a, const char *path,
                          uint64_t missing_rights);

/* The /n/audit/log + /n/audit/denied read paths: copy formatted records
 * into buf starting at byte offset `off` (9P Tread semantics). Returns
 * the bytes copied (0 at end of file). */
size_t wubu_ns_audit_log_read(const wubu_ns_audit_t *a, size_t off,
                              char *buf, size_t n);
size_t wubu_ns_audit_denied_read(const wubu_ns_audit_t *a, size_t off,
                                char *buf, size_t n);

/* Live counts. */
size_t wubu_ns_audit_tx_count(const wubu_ns_audit_t *a);
size_t wubu_ns_audit_denial_count(const wubu_ns_audit_t *a);

/* Seal the active segment and start a fresh one (#594). The hash chain
 * continues across the boundary. 0 ok; -1 bad args; -2 all segments
 * sealed (the log refuses to wrap silently). */
int  wubu_ns_audit_rotate(wubu_ns_audit_t *a);
/* How many segments are sealed. */
int  wubu_ns_audit_sealed_count(const wubu_ns_audit_t *a);

/* Deterministic replay: the digest over every record, sealed and active,
 * in sequence order. Equal logs -> equal digests, on every call, across
 * rotations. */
uint64_t wubu_ns_audit_replay_digest(const wubu_ns_audit_t *a);

/* Verify the hash chain end to end: 0 = intact, -1 = broken (a record was
 * altered after the fact). */
int  wubu_ns_audit_verify(const wubu_ns_audit_t *a);

#endif /* WUBU_NS_AUDIT_H */
