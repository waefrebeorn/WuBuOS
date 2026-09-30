/*
 * wubu_nt_sd.h -- NT security descriptor / access-check evaluation.
 *
 * The decision half of the NT security reference monitor: given a
 * SECURITY_DESCRIPTOR and a DesiredAccess mask, may this token do this?
 *
 * Complements vsl_nt_cap, which answers the same question for a live handle
 * ("may this agent use the authority it already holds?"). This module answers
 * it for an object the agent does NOT yet hold: the DACL decides whether the
 * open is permitted at all. Both feed the same object-type model
 * (wubu_nt_generic_mapping) so a granted mask means the same thing in either
 * path.
 *
 * C11. No allocation on the access path.
 */
#ifndef WUBU_NT_SD_H
#define WUBU_NT_SD_H

#include <stdint.h>
#include <stdbool.h>
#include "vsl_nt_bridge.h"   /* nt_object_type_t */

/* ---- NT access rights (the concrete bits, post generic-expansion) ---- */
#define WUBU_NT_STANDARD_RIGHTS_REQUIRED 0x000F0000u
#define WUBU_NT_SYNCHRONIZE              0x00100000u

/* ---- SECURITY_DESCRIPTOR control flags ---- */
#define WUBU_SE_OWNER_DEFAULTED       0x0001u
#define WUBU_SE_GROUP_DEFAULTED       0x0002u
#define WUBU_SE_DACL_PRESENT          0x0004u
#define WUBU_SE_DACL_DEFAULTED        0x0010u
#define WUBU_SE_SACL_PRESENT          0x0100u
#define WUBU_SE_SELF_RELATIVE         0x0800u
#define WUBU_SE_RM_CONTROL_VALID      0x4000u

/* ---- Generic access bits (pre-expansion) ---- */
#define WUBU_GENERIC_READ    0x80000000u
#define WUBU_GENERIC_WRITE   0x40000000u
#define WUBU_GENERIC_EXECUTE 0x20000000u
#define WUBU_GENERIC_ALL     0x10000000u
#define WUBU_NT_GENERIC_ALL_MASK 0xF0000000u

/* "Any right that mutates the object" -- what NO_WRITE_UP guards.
 * Only MUTATING rights belong here. The read rights (0x01 READ_DATA,
 * 0x80 READ_ATTRIBUTES, 0x20000 READ_CONTROL) are deliberately ABSENT: a
 * mandatory label gates writes, never reads, so including a read bit here
 * made NO_WRITE_UP silently refuse reads. */
#define WUBU_NT_RIGHT_WRITE_ANY                                              \
    (0x00000002u /* FILE_WRITE_DATA / KEY_SET_VALUE / *_MODIFY_STATE */     \
     | 0x00000004u /* FILE_APPEND_DATA / KEY_CREATE_SUB_KEY */              \
     | 0x00000010u /* FILE_WRITE_EA */                                       \
     | 0x00000020u /* FILE_EXECUTE (implying an image-map write) */          \
     | 0x00000040u /* FILE_DELETE_CHILD */                                   \
     | 0x00000100u /* FILE_WRITE_ATTRIBUTES */                               \
     | 0x00010000u /* DELETE */                                              \
     | 0x00040000u /* WRITE_DAC */                                           \
     | 0x00080000u /* WRITE_OWNER */                                         \
     | 0x00200000u /* TOKEN_ADJUST_PRIVILEGES */)

/* ---- NTSTATUS subset used here ---- */
#define WUBU_NT_STATUS_SUCCESS       0x00000000u
#define WUBU_NT_STATUS_ACCESS_DENIED 0xC0000022u   /* STATUS_ACCESS_DENIED */

/* Largest security descriptor we will accept, so a malformed offset can never
 * walk us off the end. */
#define WUBU_NT_SD_MAX_BYTES 4096u

/* ---- Simplified SID set (the bridge carries 32-bit SIDs) ---- */
#define WUBU_NT_SIDSET_MAX 64

typedef struct {
    uint32_t owner_sid;
    uint32_t mic;                              /* mandatory integrity level */
    uint32_t count;
    uint32_t sids[WUBU_NT_SIDSET_MAX];         /* group SIDs */
} wubu_nt_sidset_t;

static inline void wubu_nt_sidset_reset(wubu_nt_sidset_t *s) {
    s->owner_sid = 0; s->mic = 0; s->count = 0;
}
static inline bool wubu_nt_sidset_add(wubu_nt_sidset_t *s, uint32_t sid) {
    if (s->count >= WUBU_NT_SIDSET_MAX) return false;
    s->sids[s->count++] = sid;
    return true;
}
static inline bool wubu_nt_sidset_has(const wubu_nt_sidset_t *s, uint32_t sid) {
    for (uint32_t i = 0; i < s->count; i++) if (s->sids[i] == sid) return true;
    return false;
}

/* ---- Access Control Entries ---- */
#define WUBU_NT_ACE_MAX 32

typedef enum {
    WUBU_NT_ACE_ALLOW_OWNER = 0,
    WUBU_NT_ACE_ALLOW_GROUP = 1,
    WUBU_NT_ACE_DENY_OWNER  = 2,
    WUBU_NT_ACE_DENY_GROUP  = 3,
} wubu_nt_ace_kind_t;

typedef struct {
    wubu_nt_ace_kind_t kind;
    uint32_t sid;        /* simplified 32-bit SID this ACE matches */
    uint32_t rights;     /* concrete rights granted/denied */
} wubu_nt_ace_t;

typedef struct {
    bool           present;
    uint32_t       ace_count;
    wubu_nt_ace_t  aces[WUBU_NT_ACE_MAX];
} wubu_nt_dacl_view_t;

/* A parsed security descriptor: the DACL plus the mandatory label. */
typedef struct {
    uint16_t             control;
    const uint8_t       *dacl;                 /* raw, NT wire layout */
    bool                 dacl_present;
    uint32_t             label_mic;            /* mandatory integrity level */
    bool                 no_write_up;
    wubu_nt_dacl_view_t  dacl_view;             /* decoded DACL */
} wubu_nt_sd_view_t;

typedef struct {
    uint32_t status;      /* WUBU_NT_STATUS_* */
    uint32_t granted;     /* rights actually granted */
    uint32_t missing;     /* requested-but-not-granted */
    bool     denied_ace;  /* a deny ACE (or short grant) refused it */
    bool     mic_failed;  /* mandatory integrity refused the write */
} wubu_nt_access_result_t;

/* Parse the real NT SECURITY_DESCRIPTOR wire layout. Accepts a self-relative
 * SD (offsets from the base) -- the form NT itself emits. Also decodes the
 * DACL bytes into out->dacl_view. */
bool wubu_nt_sd_read(const void *sd_in, wubu_nt_sd_view_t *out);

/* Decode a raw NT ACL (AclRevision/AclSize/AceCount + variable-length ACEs)
 * into our ACE view. Fails closed on a malformed size. */
void wubu_nt_dacl_decode(const void *acl_in, wubu_nt_dacl_view_t *out);

/* The GENERIC_MAPPING for an object type: what GENERIC_READ/WRITE/EXECUTE/ALL
 * actually mean for that type. This is why GENERIC_ALL on a File differs from
 * GENERIC_ALL on a Key. */
uint32_t wubu_nt_generic_mapping(nt_object_type_t type, uint32_t *read,
                                 uint32_t *write, uint32_t *exec,
                                 uint32_t *all);

/* Expand GENERIC_* bits in `desired` into concrete rights for `type`. */
uint32_t wubu_nt_expand_generic(nt_object_type_t type, uint32_t desired);

/* Evaluate a decoded DACL. Access is granted iff the applicable ALLOW ACEs
 * cover every requested bit and no applicable DENY ACE names one -- this is
 * NT's SeAccessCheck ordering, with no separate "granted mask" AND. With no
 * DACL present, full access is granted (NT semantics). `granted_out` receives
 * the intersection of the requested rights with what the ACEs allowed. */
bool wubu_nt_dacl_allows(const wubu_nt_dacl_view_t *dacl,
                         const wubu_nt_sidset_t *subject,
                         uint32_t desired, uint32_t *granted_out);

/* NO_WRITE_UP mandatory integrity check. */
bool wubu_nt_mic_allows_write(uint32_t subject_mic, uint32_t label_mic,
                              bool no_write_up);

/* The full access check: expand generics, evaluate the DACL, then apply the
 * mandatory label. */
wubu_nt_access_result_t wubu_nt_access_check(const wubu_nt_sd_view_t *sd,
                                             const wubu_nt_sidset_t *subject,
                                             nt_object_type_t type,
                                             uint32_t desired);

#endif /* WUBU_NT_SD_H */
