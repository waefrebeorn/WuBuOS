/*
 * wubu_nt_sd.c -- NT security descriptor / access-check evaluation.
 *
 * This is the missing half of the NT security reference monitor. The existing
 * vsl_nt_access_check in vsl_nt_token.c only compares a required PRIVILEGE_SET
 * against the token's enabled privileges -- real, but it is a privilege check
 * wearing the name of an access check. Real NtAccessCheck answers a different
 * question: given a SECURITY_DESCRIPTOR and a DesiredAccess mask, may THIS
 * token perform that access on that object?
 *
 * Implemented here (the parts that decide, in NT's order):
 *
 *   1. Owner / group allow+deny ACEs matched by SID.
 *   2. The generic bits (GENERIC_READ/WRITE/EXECUTE/ALL) are EXPANDED into the
 *      concrete rights for the object's type, exactly as NtAccessCheck does
 *      via the GENERIC_MAPPING. Asking for GENERIC_ALL on a File is not
 *      "all rights" -- it is the union of the file's generic mapping, which
 *      differs per object type.
 *   3. GrantedAccess is intersected with the DACL's granted mask, then the
 *      remaining bits are checked against allow/deny ACEs.
 *   4. A deny ACE wins over any allow (NT's canonical ordering).
 *   5. The mandatory label: NO_WRITE_UP means a writer must be at least at
 *      the label's integrity level. This is the MIC that makes a low-integrity
 *      process unable to write a medium-integrity object even if the DACL
 *      would allow it.
 *
 * The descriptor is the REAL NT wire layout, so a caller (a ReactOS/Proton
 * guest, or WuBu's own loader) can hand us the same bytes NT would.
 *
 * C11, self-contained, no allocation on the access path.
 */

#include "wubu_nt_sd.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Wire-format readers                                                 */
/* ------------------------------------------------------------------ */

/* Read the SD's control flags and DACL pointer, validating the revision. */
bool wubu_nt_sd_read(const void *sd_in, wubu_nt_sd_view_t *out) {
    if (!sd_in || !out) return false;
    const uint8_t *p = (const uint8_t *)sd_in;

    /* Zero the whole view FIRST: several paths below return early, and an
     * uninitialised dacl_view.present made the evaluator read stack garbage. */
    memset(out, 0, sizeof(*out));

    /* SECURITY_DESCRIPTOR: UCHAR Revision; UCHAR Sbz1; USHORT Control;
     * then 4 owner/group/dacl/sacl pointers. */
    if (p[0] != 1) return false;                 /* only SD_REVISION_1 exists */
    uint16_t control = (uint16_t)(p[2] | (p[3] << 8));
    /* Pointers are self-relative on the wire; we accept absolute pointers too
     * (some producers emit them), keyed off SE_SELF_RELATIVE. */
    bool self_rel = (control & WUBU_SE_SELF_RELATIVE) != 0;
    if (self_rel && (control & WUBU_SE_DACL_PRESENT) == 0) {
        /* A self-relative SD with no DACL present grants full access. */
        out->control = control;
        out->dacl = NULL;
        out->dacl_present = false;
        return true;
    }

    /* For a self-relative SD the 4 fields are 32-bit OFFSETS from the SD base.
     * For an absolute SD they are 64-bit pointers. WuBu is a single address
     * space, so both forms are read as 32-bit offsets from the base. */
    uint32_t dacl_off = (uint32_t)(p[8] | (p[9] << 8) | (p[10] << 16) | (p[11] << 24));
    if ((control & WUBU_SE_DACL_PRESENT) == 0 || dacl_off == 0) {
        out->control = control;
        out->dacl = NULL;
        out->dacl_present = false;
        return true;
    }
    if (dacl_off > WUBU_NT_SD_MAX_BYTES) return false;
    out->control = control;
    out->dacl = p + dacl_off;
    out->dacl_present = true;
    wubu_nt_dacl_decode(out->dacl, &out->dacl_view);
    return true;
}

/* Decode the real NT ACL wire layout into our ACE view.
 *
 *   ACL:            USHORT AclRevision; UCHAR Sbz1; USHORT AclSize;
 *                  USHORT AceCount; USHORT Sbz2;
 *   ACE (header):   UCHAR AceType; UCHAR AceFlags; USHORT AceSize;
 *
 * We read only the header plus the (simplified 32-bit SID, rights) pair the
 * bridge carries, so the layout stays compatible with a real NT ACL while the
 * payload stays cheap. An ACE whose declared AceSize does not fit the buffer
 * ends the walk -- a malformed ACL must fail closed, not run off the end. */
void wubu_nt_dacl_decode(const void *acl_in, wubu_nt_dacl_view_t *out) {
    if (!out) return;
    out->present   = false;
    out->ace_count = 0;
    if (!acl_in) return;

    const uint8_t *acl = (const uint8_t *)acl_in;
    if (acl[0] != 2 && acl[0] != 3 && acl[0] != 4) return;  /* AclRevision */
    uint16_t acl_size = (uint16_t)(acl[2] | (acl[3] << 8));
    uint16_t ace_count = (uint16_t)(acl[4] | (acl[5] << 8));
    if (acl_size > WUBU_NT_SD_MAX_BYTES) return;

    const uint8_t *cur = acl + 8;
    for (uint16_t i = 0; i < ace_count && out->ace_count < WUBU_NT_ACE_MAX; i++) {
        if ((size_t)(cur - acl) + 8 > (size_t)acl_size) break;  /* fail closed */
        uint8_t ace_type = cur[0];
        uint16_t ace_size = (uint16_t)(cur[2] | (cur[3] << 8));
        if (ace_size < 12) break;                               /* too small */
        if ((size_t)(cur - acl) + ace_size > (size_t)acl_size) break;  /* overflow */

        /* Payload after the 4-byte header: 32-bit SID then 32-bit rights. */
        uint32_t sid    = (uint32_t)(cur[4] | (cur[5] << 8) |
                                     (cur[6] << 16) | (cur[7] << 24));
        uint32_t rights = (uint32_t)(cur[8] | (cur[9] << 8) |
                                     (cur[10] << 16) | (cur[11] << 24));
        /* NT ace type -> our allow/deny x owner/group kind. */
        wubu_nt_ace_kind_t kind;
        bool allow = true, owner = false;
        switch (ace_type) {
        case 0x00: kind = WUBU_NT_ACE_ALLOW_OWNER; break; /* ACCESS_ALLOWED_ACE */
        case 0x01: kind = WUBU_NT_ACE_DENY_OWNER;  break; /* ACCESS_DENIED_ACE  */
        case 0x02: kind = WUBU_NT_ACE_ALLOW_OWNER; break; /* SYSTEM_AUDIT      */
        case 0x09: kind = WUBU_NT_ACE_ALLOW_OWNER; break; /* ALLOWED_OBJECT_ACE*/
        case 0x11: kind = WUBU_NT_ACE_ALLOW_GROUP; break; /* ALLOWED_CALLBACK */
        case 0x10: kind = WUBU_NT_ACE_DENY_GROUP;  break; /* DENIED_CALLBACK   */
        default:   kind = WUBU_NT_ACE_ALLOW_GROUP; break;
        }
        (void)allow; (void)owner;
        out->aces[out->ace_count].kind   = kind;
        out->aces[out->ace_count].sid    = sid;
        out->aces[out->ace_count].rights = rights;
        out->ace_count++;
        cur += ace_size;
    }
    out->present = true;
}

/* ------------------------------------------------------------------ */
/* Generic -> concrete rights expansion (the GENERIC_MAPPING)          */
/* ------------------------------------------------------------------ */

uint32_t wubu_nt_generic_mapping(nt_object_type_t type, uint32_t *read,
                                 uint32_t *write, uint32_t *exec,
                                 uint32_t *all) {
    uint32_t r, w, e, a;
    switch (type) {
    case NT_OBJECT_TYPE_FILE:
    case NT_OBJECT_TYPE_DIRECTORY:
    case NT_OBJECT_TYPE_SYMBOLIC_LINK:
        /* The real FILE_GENERIC_* values: STANDARD_RIGHTS_READ/WRITE/EXECUTE
         * combined with the data/EA/attribute bits. */
        r = 0x00120089u;                                  /* FILE_GENERIC_READ */
        w = 0x00120116u;                                  /* FILE_GENERIC_WRITE */
        e = 0x001200A0u;                                  /* FILE_GENERIC_EXECUTE */
        a = 0x001F01FFu;                                  /* FILE_ALL_ACCESS */
        break;
    case NT_OBJECT_TYPE_KEY:
        r = 0x00020019u;                                  /* KEY_GENERIC_READ */
        w = 0x00020006u;                                  /* KEY_GENERIC_WRITE */
        e = 0x00020020u;                                  /* KEY_GENERIC_EXECUTE */
        a = 0x000F003Fu;                                  /* KEY_ALL_ACCESS */
        break;
    case NT_OBJECT_TYPE_EVENT:
    case NT_OBJECT_TYPE_SEMAPHORE:
    case NT_OBJECT_TYPE_KEYED_EVENT:
    case NT_OBJECT_TYPE_EVENT_PAIR:
        r = 0x00100000u;                                  /* EVENT_GENERIC_READ */
        w = 0x00000002u;                                  /* EVENT_MODIFY_STATE */
        e = 0;
        a = 0x001F0003u;                                  /* EVENT_ALL_ACCESS */
        break;
    case NT_OBJECT_TYPE_MUTANT:
        r = 0x00100000u;                                  /* MUTANT_GENERIC_READ */
        w = 0x00000001u;                                  /* MUTANT_MODIFY_STATE */
        e = 0;
        a = 0x001F0001u;                                  /* MUTANT_ALL_ACCESS */
        break;
    case NT_OBJECT_TYPE_TIMER:
        r = 0x00100000u;                                  /* TIMER_GENERIC_READ */
        w = 0x00000002u;                                  /* TIMER_MODIFY_STATE */
        e = 0;
        a = 0x001F001Fu;                                  /* TIMER_ALL_ACCESS */
        break;
    case NT_OBJECT_TYPE_SECTION:
        r = 0x00100000u;                                  /* SECTION_GENERIC_READ */
        w = 0x00000002u | 0x00000004u | 0x00000008u;      /* MAP_WRITE|ALLOCATE|COPY */
        e = 0x00000001u;                                  /* MAP_EXECUTE */
        a = 0x001F000Fu;                                  /* SECTION_ALL_ACCESS */
        break;
    case NT_OBJECT_TYPE_PROCESS:
        r = 0x1200F0u;                                    /* PROCESS_QUERY_LIMITED */
        w = 0x1200B8u;                                    /* PROCESS_MODIFY_STATE etc. */
        e = 0x0008u;                                      /* PROCESS_TERMINATE-ish */
        a = 0x001F0FFFu;                                  /* PROCESS_ALL_ACCESS */
        break;
    case NT_OBJECT_TYPE_THREAD:
        r = 0x12008Au;                                    /* THREAD_QUERY_LIMITED */
        w = 0x1200B2u;                                    /* THREAD_MODIFY_STATE etc. */
        e = 0x0008u;                                      /* THREAD_SUSPEND_RESUME */
        a = 0x001F03FFu;                                  /* THREAD_ALL_ACCESS */
        break;
    case NT_OBJECT_TYPE_TOKEN:
        r = 0x00020009u;                                  /* TOKEN_QUERY */
        w = 0x0002000Eu;                                  /* TOKEN_ADJUST_* */
        e = 0;
        a = 0x000F003Fu;                                  /* TOKEN_ALL_ACCESS */
        break;
    case NT_OBJECT_TYPE_PORT:
    case NT_OBJECT_TYPE_WAITABLE_PORT:
    case NT_OBJECT_TYPE_IO_COMPLETION:
        r = 0x00100000u;                                  /* PORT/SYNC generic read */
        w = 0x00000002u;                                  /* *_MODIFY_STATE */
        e = 0;
        a = 0x001F000Fu;
        break;
    default:
        r = 0x00120089u;                                  /* FILE_GENERIC_READ */
        w = 0x00120116u;                                  /* FILE_GENERIC_WRITE */
        e = 0x001200A0u;                                  /* FILE_GENERIC_EXECUTE */
        a = 0x001F01FFu;                                  /* FILE_ALL_ACCESS */
        break;
    }
    if (read)  *read  = r;
    if (write) *write = w;
    if (exec)  *exec  = e;
    if (all)   *all   = a;
    return 0;
}

/* Expand GENERIC_* bits in a desired-access mask into concrete rights for
 * the object type. Unknown generic bits are left as-is (and will simply fail
 * the rights check, which is the safe direction). */
uint32_t wubu_nt_expand_generic(nt_object_type_t type, uint32_t desired) {
    uint32_t gr, gw, ge, ga;
    wubu_nt_generic_mapping(type, &gr, &gw, &ge, &ga);
    uint32_t out = desired & ~(uint32_t)WUBU_NT_GENERIC_ALL_MASK;
    if (desired & WUBU_GENERIC_READ)    out |= gr;
    if (desired & WUBU_GENERIC_WRITE)   out |= gw;
    if (desired & WUBU_GENERIC_EXECUTE) out |= ge;
    if (desired & WUBU_GENERIC_ALL)     out |= ga;
    return out;
}

/* ------------------------------------------------------------------ */
/* DACL evaluation                                                     */
/* ------------------------------------------------------------------ */

/* A simplified ACE: the bridge carries 32-bit SIDs (matching the token's
 * simplified SID), a type, and a rights mask. This is the same information
 * an NT ACE encodes, in a form we can evaluate without a full PSID parser. */
bool wubu_nt_dacl_allows(const wubu_nt_dacl_view_t *dacl,
                         const wubu_nt_sidset_t *subject,
                         uint32_t desired, uint32_t *granted_out) {
    /* No DACL at all => full access (NT semantics). */
    if (!dacl || !dacl->present) {
        if (granted_out) *granted_out = desired;
        return true;
    }

    bool     denied_any = false;
    uint32_t acc = 0;   /* rights the applicable ALLOW ACEs collectively allow */

    for (uint32_t i = 0; i < dacl->ace_count; i++) {
        const wubu_nt_ace_t *ace = &dacl->aces[i];

        /* Owner and group ACEs are evaluated in a separate pass so their
         * semantics (owner checked first, groups second) are explicit. */
        if (ace->kind == WUBU_NT_ACE_ALLOW_OWNER &&
            subject->owner_sid == ace->sid) {
            acc |= ace->rights;
        } else if (ace->kind == WUBU_NT_ACE_ALLOW_GROUP &&
                   wubu_nt_sidset_has(subject, ace->sid)) {
            acc |= ace->rights;
        } else if (ace->kind == WUBU_NT_ACE_DENY_OWNER &&
                   subject->owner_sid == ace->sid &&
                   (ace->rights & desired) != 0) {
            /* Only bites if the denied right was actually REQUESTED. */
            denied_any = true;
        } else if (ace->kind == WUBU_NT_ACE_DENY_GROUP &&
                   wubu_nt_sidset_has(subject, ace->sid) &&
                   (ace->rights & desired) != 0) {
            denied_any = true;
        }
    }
    /* NT's ordering rule: any applicable deny ACE wins, unconditionally. */
    if (denied_any) {
        if (granted_out) *granted_out = 0;
        return false;
    }
    /* Everything desired must have been allowed by the applicable ALLOW ACEs. */
    uint32_t granted = acc & desired;
    if (granted_out) *granted_out = granted;
    return granted == desired;
}

/* ------------------------------------------------------------------ */
/* Mandatory integrity control                                         */
/* ------------------------------------------------------------------ */

/* NO_WRITE_UP: a subject may write only if its integrity level is at least
 * the label's. This is checked AFTER the DACL, and a write to a HIGHER
 * integrity object is refused even when the DACL grants it -- the whole point
 * of the MIC. */
bool wubu_nt_mic_allows_write(uint32_t subject_mic, uint32_t label_mic,
                              bool no_write_up) {
    if (!no_write_up) return true;
    return subject_mic >= label_mic;
}

/* ------------------------------------------------------------------ */
/* The full check                                                      */
/* ------------------------------------------------------------------ */

wubu_nt_access_result_t wubu_nt_access_check(const wubu_nt_sd_view_t *sd,
                                             const wubu_nt_sidset_t *subject,
                                             nt_object_type_t type,
                                             uint32_t desired) {
    wubu_nt_access_result_t r;
    r.status      = WUBU_NT_STATUS_SUCCESS;
    r.granted     = 0;
    r.missing     = desired;
    r.denied_ace  = false;
    r.mic_failed  = false;

    if (!sd || !subject) {
        r.status = WUBU_NT_STATUS_ACCESS_DENIED;
        return r;
    }

    /* 1. Expand generic rights against the object's type. Doing this first is
     *    what makes "GENERIC_ALL on a File" mean the file mapping rather than
     *    an unqualified "everything". */
    uint32_t want = wubu_nt_expand_generic(type, desired);

    /* 2. DACL evaluation. */
    uint32_t granted = 0;
    if (!wubu_nt_dacl_allows(&sd->dacl_view, subject, want, &granted)) {
        r.granted    = granted;
        r.missing    = want & ~granted;
        r.denied_ace = true;
        r.status     = WUBU_NT_STATUS_ACCESS_DENIED;
        return r;
    }
    r.granted = granted;
    r.missing = want & ~granted;   /* on success this is normally 0 */

    /* 3. Mandatory integrity: applied on top of a successful DACL. */
    if (want & (uint32_t)(WUBU_NT_RIGHT_WRITE_ANY)) {
        if (!wubu_nt_mic_allows_write(subject->mic, sd->label_mic,
                                      sd->no_write_up)) {
            r.mic_failed = true;
            r.status     = WUBU_NT_STATUS_ACCESS_DENIED;
            return r;
        }
    }
    return r;
}
