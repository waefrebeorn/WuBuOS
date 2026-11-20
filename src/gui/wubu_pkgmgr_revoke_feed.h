/*
 * wubu_pkgmgr_revoke_feed.h -- interface to the revocation-feed parser leaf.
 *
 * This leaf owns ONLY the feed grammar.  It performs no crypto and no DB
 * access: the policy owner (wubu_pkgmgr.c) verifies the admin signature over
 * the exact feed bytes and applies the parsed entries atomically.
 */
#ifndef WUBU_PKGMGR_REVOKE_FEED_H
#define WUBU_PKGMGR_REVOKE_FEED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WUBU_REVOKE_FEED_REVOKE = 0,
    WUBU_REVOKE_FEED_UNREVOKE = 1
} wubu_revoke_feed_op_t;

typedef struct {
    wubu_revoke_feed_op_t op;
    char key[65];              /* canonical lowercase 64-hex + NUL */
} wubu_revoke_feed_entry_t;

/* Strict bounded parse of `len` exact feed bytes into out[].  Returns false
 * on ANY malformed line, bad key hex, duplicate/conflicting entry, or when
 * the entry count would exceed max_entries.  *out_n is set only on success.
 * err/errsz receive a human-readable reason (may be NULL/0). */
bool wubu_pkg_revoke_feed_parse(const uint8_t *data, size_t len,
                                wubu_revoke_feed_entry_t *out,
                                size_t max_entries, size_t *out_n,
                                char *err, size_t errsz);

/* Same strict parse, but also extracts the OPTIONAL signed generation
 * header.  A generation line is the FIRST meaningful line and has the exact
 * form {"seq":<positive decimal integer <= INT64_MAX>}.  It is part of the
 * exact signed bytes, so it cannot be altered without invalidating the
 * detached signature.  *out_seq receives 0 when no generation line is
 * present (a legacy feed without a counter; accepted for compatibility).
 * A second generation line, or one appearing after an entry, rejects the
 * WHOLE feed. */
bool wubu_pkg_revoke_feed_parse_seq(const uint8_t *data, size_t len,
                                    wubu_revoke_feed_entry_t *out,
                                    size_t max_entries, size_t *out_n,
                                    uint64_t *out_seq,
                                    char *err, size_t errsz);

#endif /* WUBU_PKGMGR_REVOKE_FEED_H */
