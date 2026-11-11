/*
 * wubu_aur.h -- the AUR RPC client core.
 *
 * The AUR exposes an RPC over HTTP GET: info/multiinfo take a list of package
 * names, search/multi-search take a pattern and a field.  The client core is
 * the part that is testable without a network:
 *
 *   * URL encoding -- a package name containing '&', '+' or a space would
 *     otherwise become a SECOND query parameter or a corrupted one.  This is a
 *     request-injection bug, not a formatting nicety.
 *   * Request sizing -- the AUR caps a multiinfo request, so a long list must
 *     be SPLIT into several requests rather than truncated (truncation
 *     silently reports some packages as absent).
 *   * Envelope validation -- the response type distinguishes "results",
 *     "error" and "multisearch"; an error response parsed as results would
 *     report a package as missing when the real problem is a server fault.
 *   * resultcount must MATCH the number of results actually present.  A
 *     mismatch means a truncated body, and trusting the count would let a
 *     partial answer look complete.
 *
 * JSON is parsed with the tree's existing parser (gui/wubu_json.h); this module
 * adds no second JSON implementation.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_AUR_H
#define WUBU_AUR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WUBU_AUR_NAME_MAX   128
#define WUBU_AUR_URL_MAX    2048
#define WUBU_AUR_ERR_MAX    256
#define WUBU_AUR_MAX_NAMES  150     /* the AUR's own multiinfo cap */

typedef enum {
    WUBU_AUR_INFO = 0,
    WUBU_AUR_MULTIINFO,
    WUBU_AUR_SEARCH,
    WUBU_AUR_MULTISEARCH
} wubu_aur_type_t;

typedef enum {
    WUBU_AUR_BY_NAME = 0,
    WUBU_AUR_BY_NAME_DESC,
    WUBU_AUR_BY_MAINTAINER,
    WUBU_AUR_BY_DEPENDS,
    WUBU_AUR_BY_KEYWORDS
} wubu_aur_by_t;

const char *wubu_aur_type_name(wubu_aur_type_t t);

/* Percent-encode a string for a URL query.  Everything outside
 * [A-Za-z0-9-_.~] is escaped.  Returns NULL on success, else a static reason. */
const char *wubu_aur_urlencode(const char *in, char *out, size_t n);

/* Build an info/multiinfo request URL from a comma-separated name list.
 * Returns NULL on success, else a static reason.  A list longer than the cap is
 * an ERROR: the caller must SPLIT it, because truncation would silently report
 * the tail as absent. */
const char *wubu_aur_build_info(char *out, size_t n, const char *names,
                                wubu_aur_type_t type);

/* Build a search request. */
const char *wubu_aur_build_search(char *out, size_t n, const char *pattern,
                                  wubu_aur_by_t by);

/* How many requests a comma-separated list must be split into? */
uint32_t wubu_aur_split_count(const char *names);

/* -- the response envelope ---------------------------------------- */

typedef struct {
    uint32_t resultcount;
    uint32_t n_results;      /* results actually present in the body */
    char     type[32];
    bool     is_error;
    char     error[WUBU_AUR_ERR_MAX];
    bool     valid;
} wubu_aur_envelope_t;

/* Validate an AUR RPC response envelope.  Returns NULL on success, else a
 * static reason.  A body whose resultcount disagrees with the results present
 * is REFUSED -- that is a truncated body, and a partial answer must not look
 * complete. */
const char *wubu_aur_check_envelope(const char *json, wubu_aur_envelope_t *out);

#endif /* WUBU_AUR_H */
