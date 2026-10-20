/*
 * wubu_audit.h -- hash-chained kernel audit log with tamper detection.
 *
 * The kernel's trust spine (SECURITY #775): every security-relevant event
 * is a record whose SHA-256 hash chains to its predecessor, so any
 * after-the-fact alteration of a record (or its order) breaks the chain
 * and wubu_audit_verify() reports it. The chain uses the kernel's own
 * FIPS 180-4 core (wubu_sha256.h) -- no third party.
 *
 * Owner boundaries: wubu_cap_audit.c owns the CAPABILITY-RESOLUTION denial
 * ring (who was told no by the cap core); wubu_ns_audit.c owns the
 * NAMESPACE/9P transaction log (/n/audit/log). THIS module owns the
 * KERNEL events themselves (irq storms, task lifecycle, faults, security
 * decisions) -- the ring a kernel operator audits first.
 *
 * Ring semantics: the most recent WUBU_AUDIT_CAP records are retained.
 * The chain keeps linking across eviction, so verification recomputes
 * every retained link and the head; the oldest retained record's
 * prev_hash refers to an evicted predecessor by design (the chain head --
 * exported by wubu_audit_digest() -- is what gets sealed into the
 * measured-boot record).
 *
 * C11, freestanding-compatible (no malloc, no hosted APIs).
 */
#ifndef WUBU_AUDIT_H
#define WUBU_AUDIT_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_AUDIT_CAP     512
#define WUBU_AUDIT_MSG_SZ  128
#define WUBU_AUDIT_HASH_SZ 32   /* SHA-256 */

/* Event classes (the subsystem that produced the record). */
enum {
    WUBU_AUDIT_GENERIC = 0,
    WUBU_AUDIT_IRQ,
    WUBU_AUDIT_TASK,
    WUBU_AUDIT_FS,
    WUBU_AUDIT_SECURITY,
    WUBU_AUDIT_AGI,
    WUBU_AUDIT_HW
};

/* Visible record struct (as wubu_cap_audit_rec_t): the operator -- and
 * the tamper test -- must be able to read a record directly. */
typedef struct {
    uint64_t seq;                            /* monotonic, never reused */
    uint64_t ts;                             /* caller-supplied clock */
    uint32_t event;
    uint32_t pid;
    char     msg[WUBU_AUDIT_MSG_SZ];
    uint8_t  prev_hash[WUBU_AUDIT_HASH_SZ];  /* predecessor's hash */
    uint8_t  hash[WUBU_AUDIT_HASH_SZ];       /* sha256(prev || fields) */
} wubu_audit_rec_t;

/* Reset the log to empty (seq restarts at 1). Safe before init. */
void   wubu_audit_reset(void);

/* Append a record. msg is copied (truncated at WUBU_AUDIT_MSG_SZ-1).
 * Returns the record's seq, or 0 on bad args. */
uint64_t wubu_audit_record(uint32_t event, uint32_t pid, uint64_t ts,
                           const char *msg);

/* Live record count (<= WUBU_AUDIT_CAP). */
size_t wubu_audit_count(void);

/* Record i in seq order (i = 0 is the OLDEST retained). NULL when
 * i >= count. The pointer is valid until the next record() call. */
const wubu_audit_rec_t *wubu_audit_at(size_t i);

/* The current chain head (the hash of the newest record; zeros when
 * empty). Sealing this into a measured-boot record makes the whole
 * retained log tamper-evident across reboots. */
void wubu_audit_head(uint8_t out[WUBU_AUDIT_HASH_SZ]);

/* Tamper detection: recompute every retained record's hash and each
 * link. 0 = chain intact; -1 = broken (a record or its order was
 * altered after the fact). */
int  wubu_audit_verify(void);

#endif /* WUBU_AUDIT_H */
