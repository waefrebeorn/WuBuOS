/*
 * wubu_auth.h -- the polkit authorization contract (Mint baseline §4.4 / §154).
 *
 * polkit: "the Authority on the system bus answers 'is this request
 * authorized?' for privileged mechanisms serving unprivileged clients ...
 * a per-session Authentication Agent prompts the user when interactive auth is
 * required." This is the WuBuOS authority model as SUBJECT -> ACTION ->
 * allow/deny rules, with a per-session AGENT REGISTRATION slot so interactive
 * (prompt the user) auth can be surfaced.
 *
 * Model:
 *   - a rule grants or denies {subject, action}. subject is a uid; the special
 *     uid WUBU_AUTH_UID_ANY is a wildcard (matches any subject).
 *   - an explicit deny always wins over any grant (a deny is a hard overrule).
 *   - if no rule matches, the DECISION is DENY (fail-closed, like polkit's
 *     default-deny for an absent policy).
 *   - an agent slot: the interactive auth path registers a handler that, when
 *     a request requires user confirmation, prompts and returns a final 0/1.
 *     A rule with WUBU_AUTH_REQ_INTERACT signals "ask the user" instead of a
 *     static allow.
 *
 * This is the authority wubu_logind's power-op gate should consult (§4.3 says
 * power "funnel[s] through it"), and it publishes as /n/background/auth (§154).
 * C11, runtime host, single-threaded, opaque struct.
 */
#ifndef WUBU_AUTH_H
#define WUBU_AUTH_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_AUTH_UID_ANY     ((uint32_t)~0u)  /* wildcard subject */
#define WUBU_AUTH_MAX_RULES   128
#define WUBU_AUTH_MAX_ACTION  64

typedef enum {
    WUBU_AUTH_DENY = 0,
    WUBU_AUTH_ALLOW,
    WUBU_AUTH_INTERACT      /* ask the user (via the registered agent) */
} wubu_auth_decision_t;

typedef struct wubu_auth wubu_auth_t;

/* Lifecycle --------------------------------------------------------------- */
wubu_auth_t *wubu_auth_create(void);
void wubu_auth_destroy(wubu_auth_t *auth);

/* Add/replace a rule {subject, action} with a decision. A later add with the
 * same (subject,action) overwrites. Returns 0 on success, -1 on bad action or
 * a full table. */
int wubu_auth_add_rule(wubu_auth_t *auth, uint32_t subject, const char *action,
                   wubu_auth_decision_t decision);

/* The current decision for {subject, action}: a hard DENY overrules, then an
 * explicit rule, then default DENY (fail-closed). agent_enabled tells the
 * caller a user prompt is possible (an agent is registered). `auth` is
 * non-const because an INTERACT decision may invoke the agent, which can
 * modify the authority (e.g. cache the interactive verdict). */
wubu_auth_decision_t wubu_auth_check(wubu_auth_t *auth,
                                     uint32_t subject, const char *action,
                                     int *agent_enabled);

/* Agent slot: interactive auth. Return 0 = user approved, 1 = user denied,
 * -1 = no agent available / could not prompt. */
typedef int (*wubu_auth_agent_fn)(wubu_auth_t *auth, uint32_t subject,
                                  const char *action, void *ud);
void wubu_auth_set_agent(wubu_auth_t *auth, wubu_auth_agent_fn fn, void *ud);

/* Does the authority have an agent registered? */
int wubu_auth_has_agent(const wubu_auth_t *auth);

/* Rule count / clear (for reloading policy). */
int wubu_auth_rule_count(const wubu_auth_t *auth);
void wubu_auth_clear(wubu_auth_t *auth);

/* Enumerate the i-th rule (0..rule_count-1) so /n/background/auth can publish
 * the policy. Sets *subject, *action, *decision. Returns 0 on success, -1 out
 * of range. */
int wubu_auth_rule_at(const wubu_auth_t *auth, int i,
                      uint32_t *subject, char *action, size_t action_sz,
                      wubu_auth_decision_t *decision);

#endif /* WUBU_AUTH_H */
