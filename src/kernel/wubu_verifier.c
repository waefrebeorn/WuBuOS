/*
 * wubu_verifier.c  --  WuBuOS Independent Verifier (DA-3 promotion gate)
 *
 * Deterministic, kernel-resident span scoring. NOT the agent under test
 * (DA-3: same-agent grading is a rubber stamp). See wubu_verifier.h.
 *
 * Freestanding: no malloc, no hosted APIs.
 */

#include "wubu_verifier.h"
#include "wubu_agi_kernel.h"

/* known emitters: a span must name one of these after the first token */
static const char *const k_emitters[] = {
    "bonzi.", "agent.", "supervisor.", "span", "sys.",
};

/* known semantic verbs (structure + intent signals) */
static const char *const k_verbs[] = {
    "heartbeat", "emit", "step", "region", "attest", "promote",
    "freeze", "unfreeze", "trace", "theme", "hid", "task",
};

/* kernel-local tokenizer: does `s` contain `tok` at a token boundary? */
static int has_token(const char *s, const char *tok)
{
    size_t tl = 0;
    while (tok[tl]) tl++;
    int ends_dot = (tl > 0 && tok[tl - 1] == '.');
    for (const char *p = s; *p; p++) {
        if (*p == *tok) {
            int match = 1;
            for (size_t i = 0; i < tl; i++)
                if (p[i] != tok[i]) { match = 0; break; }
            if (match) {
                char before = (p == s) ? ' ' : p[-1];
                char after = p[tl];
                int before_ok = (before < 'a' || before > 'z') &&
                                (before < '0' || before > '9');
                /* a trailing '.' is part of a dotted emitter name: the
                 * character after it starts a NEW word (always a boundary) */
                int after_ok = ends_dot || ((after < 'a' || after > 'z') &&
                                            (after < '0' || after > '9'));
                if (before_ok && after_ok)
                    return 1;
            }
        }
    }
    return 0;
}

float wubu_verifier_score(const char *payload, uint64_t ts_ms,
                          void *ud, bool *passed)
{
    (void)ts_ms; (void)ud;
    if (passed) *passed = false;
    if (!payload) return 0.0f;

    /* -- well-formedness ------------------------------------------ */
    size_t len = 0;
    for (const char *p = payload; *p; p++) {
        if ((unsigned char)*p < 32 || (unsigned char)*p > 126)
            return 0.0f;                  /* non-printable => reject */
        if (++len > WUBU_VERIFIER_MAX_LEN)
            return 0.0f;                  /* overlong => reject */
    }
    if (len < WUBU_VERIFIER_MIN_LEN)
        return 0.0f;                      /* noise => reject */

    /* -- emitter trust -------------------------------------------- */
    float score = 0.0f;
    int emitter_ok = 0;
    for (size_t i = 0; i < sizeof(k_emitters)/sizeof(k_emitters[0]); i++) {
        if (has_token(payload, k_emitters[i])) { emitter_ok = 1; break; }
    }
    if (!emitter_ok) return 0.0f;         /* unknown emitter => reject */
    score += 30.0f;

    /* -- semantic budget ------------------------------------------ */
    int verbs = 0;
    for (size_t i = 0; i < sizeof(k_verbs)/sizeof(k_verbs[0]); i++)
        if (has_token(payload, k_verbs[i])) verbs++;
    score += (verbs > 2) ? 40.0f : (verbs > 0) ? 25.0f : 5.0f;

    /* structure: balanced '=' and '%' presence (kv spans score higher) */
    int eq = 0, pct = 0;
    for (const char *p = payload; *p; p++) {
        if (*p == '=') eq++;
        if (*p == '%') pct++;
    }
    if (eq > 0) score += 15.0f;
    if (pct > 0) score += 10.0f;
    if (len > 60) score += 5.0f;          /* substantive payload */

    if (score > 100.0f) score = 100.0f;
    if (passed) *passed = (score >= WUBU_VERIFIER_THRESHOLD);
    return score;
}

void wubu_verifier_install(void)
{
    wubu_agi_kernel_t *k = wubu_agi_kernel_global();
    if (!k) return;
    /* Metal boots with no verifier (the loop is dormant); install ours.
     * Idempotent in practice: nothing else sets a verifier on metal. */
    wubu_agi_kernel_set_verifier(k, wubu_verifier_score, NULL);
}
