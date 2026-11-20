/*
 * wubu_pkgmgr_revoke_sync.h -- bounded signed cross-device revocation-feed sync.
 *
 * This leaf owns ONLY transport + trust policy.  It never mutates the
 * revocation DB itself: after fetching the EXACT feed bytes and the detached
 * admin signature it calls wubu_pkgmgr_key_feed_import(), which verifies the
 * signature over those exact bytes BEFORE applying and applies every entry in
 * ONE transaction.  No crypto or DB code is duplicated here.
 *
 * Wire format:
 *   <feed_url>        exact NDJSON revocation-feed bytes (see
 *                     src/gui/wubu_pkgmgr_revoke_feed.c for the grammar)
 *   <feed_url> + ".sig"  detached signature: 128 lowercase hex chars (a
 *                     64-byte Ed25519 signature) over the domain-separated
 *                     WUBU-PKG-REVOCATION-v1/feed hash of the exact feed bytes.
 *
 * Fail-closed rules (all enforced BEFORE the importer is called, so every
 * refusal leaves the DB untouched):
 *   - only http:// and https:// URLs whose FULL text matches a trusted URL;
 *   - https uses the OCI HTTP client, which requires TLS chain + hostname
 *     verification (a verify failure is a hard refusal, never a downgrade);
 *   - HTTP 3xx/4xx/5xx refused -- redirects are never followed;
 *   - feed or signature body over the byte cap refused;
 *   - unreachable endpoint, missing/invalid signature refused.
 */
#ifndef WUBU_PKGMGR_REVOKE_SYNC_H
#define WUBU_PKGMGR_REVOKE_SYNC_H

#include <stdbool.h>
#include <stddef.h>

#define WUBU_PKGMGR_MAX_SYNC_URLS 16u
#define WUBU_PKGMGR_SYNC_URL_MAX  512u

typedef struct {
    char urls[WUBU_PKGMGR_MAX_SYNC_URLS][WUBU_PKGMGR_SYNC_URL_MAX];
    int  n;
} wubu_pkg_sync_config_t;

/* Empty the trusted-URL set. */
void wubu_pkg_sync_config_init(wubu_pkg_sync_config_t *cfg);

/* Load trusted sync URLs from `path` (one `trusted_sync_url=<url>` per line;
 * blank and '#' lines are ignored), then append comma-separated URLs from the
 * WUBU_PKGMGR_SYNC_URLS environment variable, from `extra` (CLI), and finally
 * from the GUI settings store key `trusted_sync_url`.  A missing file is fine
 * (no URLs); a malformed line/bad URL returns -1.  Returns the resulting
 * trusted URL count (bounded). */
int wubu_pkg_sync_config_load(const char *path, const char *extra,
                              wubu_pkg_sync_config_t *cfg);

/* True only for an exact full-text match with a configured trusted URL. */
bool wubu_pkg_sync_url_trusted(const wubu_pkg_sync_config_t *cfg, const char *url);

/* Fetch `url`, authenticate through the existing importer, apply atomically.
 * On success *out_applied receives the number of entries applied.  On any
 * refusal err/errsz receive a reason and no database write has occurred. */
bool wubu_pkgmgr_key_feed_sync(const char *url,
                               const wubu_pkg_sync_config_t *cfg,
                               size_t *out_applied, char *err, size_t errsz);

#endif /* WUBU_PKGMGR_REVOKE_SYNC_H */
