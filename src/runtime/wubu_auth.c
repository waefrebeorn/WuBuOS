/*
 * wubu_auth.c -- polkit-equivalent authority (P3 §4.4).
 *
 * Subject -> action -> allow/deny/interactive rules, fail-closed default.
 * The agent slot resolves INTERACT decisions (the interactive-auth path).
 * `wubu_auth_check` returns the final decision: a hard DENY overrules; an
 * INTERACT rule asks the user through the agent (giving ALLOW or DENY); no
 * matching rule is DENY.
 * C11, runtime host, single-threaded.
 */
#include "wubu_auth.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t subject;
    char action[WUBU_AUTH_MAX_ACTION];
    wubu_auth_decision_t decision;
    char used;
} wubu_auth_rule_t;

struct wubu_auth {
    wubu_auth_rule_t rules[WUBU_AUTH_MAX_RULES];
    int n_rules;
    wubu_auth_agent_fn agent;
    void *agent_ud;
};

wubu_auth_t *wubu_auth_create(void) {
    return (wubu_auth_t *)calloc(1, sizeof(wubu_auth_t));
}

void wubu_auth_destroy(wubu_auth_t *auth) {
    if (auth) free(auth);
}

static int subject_matches(uint32_t rule_subject, uint32_t subject) {
    return rule_subject == WUBU_AUTH_UID_ANY || rule_subject == subject;
}

int wubu_auth_add_rule(wubu_auth_t *auth, uint32_t subject, const char *action,
                   wubu_auth_decision_t decision) {
    if (!auth || !action || !action[0]) return -1;
    size_t alen = strlen(action);
    if (alen >= WUBU_AUTH_MAX_ACTION) return -1;
    for (int i = 0; i < auth->n_rules; i++) {
        if (auth->rules[i].used &&
            auth->rules[i].subject == subject &&
            strcmp(auth->rules[i].action, action) == 0) {
            auth->rules[i].decision = decision;   /* overwrite */
            return 0;
        }
    }
    if (auth->n_rules >= WUBU_AUTH_MAX_RULES) return -1;
    wubu_auth_rule_t *r = &auth->rules[auth->n_rules++];
    r->used = 1;
    r->subject = subject;
    strncpy(r->action, action, WUBU_AUTH_MAX_ACTION - 1);
    r->action[WUBU_AUTH_MAX_ACTION - 1] = '\0';
    r->decision = decision;
    return 0;
}

wubu_auth_decision_t wubu_auth_check(wubu_auth_t *auth,
                                     uint32_t subject, const char *action,
                                     int *agent_enabled) {
    if (agent_enabled) *agent_enabled = auth && auth->agent ? 1 : 0;
    if (!auth || !action) return WUBU_AUTH_DENY;

    /* first pass: a hard DENY for {subject,action} is a definitive overrule */
    wubu_auth_rule_t *interact = NULL;
    wubu_auth_rule_t *allow = NULL;
    for (int i = 0; i < auth->n_rules; i++) {
        if (!auth->rules[i].used) continue;
        wubu_auth_rule_t *r = &auth->rules[i];
        if (strcmp(r->action, action) != 0 || !subject_matches(r->subject, subject))
            continue;
        if (r->decision == WUBU_AUTH_DENY) return WUBU_AUTH_DENY;
        if (r->decision == WUBU_AUTH_INTERACT && !interact) interact = r;
        if (r->decision == WUBU_AUTH_ALLOW) allow = r;
    }
    if (interact) {
        /* interactive rule: ask the user (needs an agent), else fail closed */
        if (auth->agent) {
            int verdict = auth->agent(auth, subject, action, auth->agent_ud);
            return (verdict == 0) ? WUBU_AUTH_ALLOW : WUBU_AUTH_DENY;
        }
        return WUBU_AUTH_DENY;
    }
    if (allow) return WUBU_AUTH_ALLOW;
    return WUBU_AUTH_DENY;   /* fail-closed default */
}

void wubu_auth_set_agent(wubu_auth_t *auth, wubu_auth_agent_fn fn, void *ud) {
    auth->agent = fn;
    auth->agent_ud = ud;
}

int wubu_auth_has_agent(const wubu_auth_t *auth) {
    return auth && auth->agent ? 1 : 0;
}

int wubu_auth_rule_count(const wubu_auth_t *auth) {
    return auth ? auth->n_rules : 0;
}

void wubu_auth_clear(wubu_auth_t *auth) {
    if (!auth) return;
    memset(auth->rules, 0, sizeof(auth->rules));
    auth->n_rules = 0;
}

int wubu_auth_rule_at(const wubu_auth_t *auth, int i,
                      uint32_t *subject, char *action, size_t action_sz,
                      wubu_auth_decision_t *decision) {
    if (!auth || i < 0 || i >= auth->n_rules) return -1;
    const wubu_auth_rule_t *r = &auth->rules[i];
    if (subject) *subject = r->subject;
    if (action && action_sz) {
        size_t n = strlen(r->action);
        if (n >= action_sz) n = action_sz - 1;
        memcpy(action, r->action, n);
        action[n] = '\0';
    }
    if (decision) *decision = r->decision;
    return 0;
}
