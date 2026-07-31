#ifndef WUBU_GDPR_AGE_H
#define WUBU_GDPR_AGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * wubu_gdpr_age.h — GDPR-compliant age assurance for WuBuOS.
 *
 * GDPR age of consent: 16 (Article 8). EU requires "reasonable efforts" to
 * verify age without collecting excessive data. This implementation uses
 * a **self-declaration + knowledge question** flow (no ID, no facial
 * recognition, no biometric data, no third-party verification).
 *
 * Triple-DA:
 *   Decision: no biometric data, no ID upload, no third-party.
 *   Design:   single checkbox + one knowledge question (user knows the
 *             system is AGI, so must be adult-level cognitive) → log to
 *             audit trail via EDR UUIDv7 timestamp.
 *   Robustness: persists consent state, fails closed (deny) if no state.
 *
 * Ohio single-developer disclaimer:
 *   This is a hobbyist OS by a single developer in Ohio, USA. EU users who
 *   require formal age-assurance compliance should not use this software.
 *   US COPPA (13+) and GDPR (16+) both apply; we default to 16.
 *
 * Legal basis: GDPR Art 6(1)(f) — legitimate interest (operating the OS),
 *   plus Art 8(1) parental consent exemption for 16+. Self-declaration is
 *   the "knowledge question" approach (EFF/EDPB guidance: self-declaration
 *   is NOT an appropriate high-privacy method, but for an open-source OS
 *   with no data collection, it is the only viable option).
 */

/* Age verification result */
typedef enum {
    WUBU_AGE_UNKNOWN = 0,   /* consent not yet given */
    WUBU_AGE_CONSENTED = 1, /* >= 16, self-declared + knowledge verified */
    WUBU_AGE_DENIED = 2,    /* under 16, or explicitly declined */
} wubu_age_status_t;

/* Result of age verification attempt */
typedef struct {
    wubu_age_status_t status;
    int declared_age;       /* what user declared (0 if none) */
    char session_uuid[37];  /* UUIDv7 of this verification attempt */
    uint64_t consent_ts_ms; /* timestamp of consent */
} wubu_age_result_t;

/* State file path (in user's config dir) */
#define WUBU_GDPR_STATE_PATH  "/wubu/state/gdpr_age"

/* Run the age verification screen (interactive on terminal).
 * Returns 0 if user consented, 1 if denied, -1 on error.
 * Presents:
 *   1. Legal disclaimer (Ohio single-developer, no funds for full GDPR)
 *   2. Age checkbox (must be >= 16)
 *   3. Knowledge question (proves adult cognitive level)
 *   4. No data collected — just sets a consent cookie file
 */
int wubu_gdpr_age_verify(void);

/* Check if user has already consented (non-interactive, for boot path).
 * Returns CONSENTED/DENIED/UNKNOWN. */
wubu_age_status_t wubu_gdpr_age_check(void);

/* Get the consent result (for EDR logging / UI display). */
wubu_age_result_t wubu_gdpr_age_result(void);

/* Persist consent state to disk. */
int wubu_gdpr_age_persist(wubu_age_status_t status, int age);

/* Reset consent state (forces re-verification on next boot). */
void wubu_gdpr_age_reset(void);

/* Get legal disclaimer text (Ohio single-developer GDPR disclaimer +
 * EU ban notice for those requiring formal compliance). Returns static string. */
const char *wubu_gdpr_disclaimer_text(void);

/* Get age-of-consent for the current jurisdiction. Returns 16 (GDPR) as
 * the safe default. */
int wubu_gdpr_age_of_consent(void);

#ifdef __cplusplus
}
#endif
#endif /* WUBU_GDPR_AGE_H */
