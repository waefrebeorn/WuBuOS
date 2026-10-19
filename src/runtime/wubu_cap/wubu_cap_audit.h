/*
 * wubu_cap_audit.h -- denial ring for capability resolution (SECURITY #910).
 *
 * Every refusal carries a reason. The ring keeps the most recent
 * WUBU_CAP_AUDIT_CAPACITY denials with caller pid, cap kind, requested rights
 * and the failing check, so an operator can answer "who was told no, and why"
 * without reproducing the run.
 */
#ifndef WUBU_CAP_AUDIT_H
#define WUBU_CAP_AUDIT_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_CAP_AUDIT_CAPACITY 256

typedef enum {
    WUBU_CAP_CHECK_AUDIENCE = 1,  /* caller is not in the object's audience */
    WUBU_CAP_CHECK_RIGHTS   = 2,  /* audience ok, requested rights exceed grant */
    WUBU_CAP_CHECK_REVOKED  = 3   /* object deleted or generation stale */
} wubu_cap_check_t;

typedef struct {
    int32_t  pid;
    uint16_t kind;
    uint8_t  check;
    uint8_t  _pad;
    uint64_t requested;
    uint64_t seq;      /* monotonic; shows drop order once the ring wraps */
} wubu_cap_audit_rec_t;

void        wubu_cap_audit_reset(void);
void        wubu_cap_audit_record(int32_t pid, uint16_t kind,
                                  uint64_t requested, uint8_t check);
size_t      wubu_cap_audit_count(void);
uint64_t    wubu_cap_audit_total(void);
/* 0 = oldest retained record. NULL when i >= count. */
const wubu_cap_audit_rec_t *wubu_cap_audit_at(size_t i);

#endif /* WUBU_CAP_AUDIT_H */