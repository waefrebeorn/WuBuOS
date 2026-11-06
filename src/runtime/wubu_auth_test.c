/* wubu_auth_test.c -- P3 polkit-equivalent authority (Mint baseline §4.4/§154).
 * Tests the SUBJECT -> ACTION -> allow/deny/interactive decision model:
 *   - a hard DENY overrules any allow for the same {subject,action}
 *   - an explicit ALLOW returns allowed; a WUBU_AUTH_UID_ANY rule matches any
 *   - an INTERACT rule prompts the user via the registered agent and resolves
 *     ALLOW/DENY from the verdict; with no agent it fails CLOSED (deny)
 *   - a subject/action with no matching rule defaults to DENY (fail-closed)
 *   - rule() replaces, never duplicates; clear() resets the table
 */
#include "wubu_auth.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static int g_agent_approve = 1;   /* agent verdict */
static int g_agent_calls = 0;
static int agent(wubu_auth_t *auth, uint32_t subject, const char *action, void *ud) {
    (void)auth; (void)ud;
    g_agent_calls++;
    if (strcmp(action, "mount-tripwire") == 0) return 1;  /* always deny this */
    return g_agent_approve ? 0 : 1;
}

int main(void) {
    wubu_auth_t *auth = wubu_auth_create();
    CHECK(auth != NULL, "authority created");

    int agent_enabled = 0;

    /* fail-closed default: no rules -> deny */
    CHECK(wubu_auth_check(auth, 1000, "reboot", &agent_enabled) == WUBU_AUTH_DENY,
          "unknown action defaults to deny (fail-closed)");
    CHECK(agent_enabled == 0, "no agent yet -> agent_enabled=0");

    /* explicit allow for a specific uid */
    CHECK(wubu_auth_add_rule(auth, 1000, "reboot", WUBU_AUTH_ALLOW) == 0,
          "allow rule for uid 1000 reboot installed");
    CHECK(wubu_auth_check(auth, 1000, "reboot", NULL) == WUBU_AUTH_ALLOW,
          "uid 1000 reboot now allowed");
    CHECK(wubu_auth_check(auth, 1001, "reboot", NULL) == WUBU_AUTH_DENY,
          "other uid still denied rebound (rule is uid-specific)");

    /* wildcard allows anyone */
    CHECK(wubu_auth_add_rule(auth, WUBU_AUTH_UID_ANY, "suspend", WUBU_AUTH_ALLOW) == 0,
          "wildcard allow for suspend installed");
    CHECK(wubu_auth_check(auth, 1000, "suspend", NULL) == WUBU_AUTH_ALLOW,
          "wildcard allows uid 1000 suspend");
    CHECK(wubu_auth_check(auth, 4242, "suspend", NULL) == WUBU_AUTH_ALLOW,
          "wildcard allows ANY uid suspend");

    /* hard deny overrules an allow on the same {subject,action} */
    wubu_auth_add_rule(auth, 1000, "suspend", WUBU_AUTH_DENY);
    CHECK(wubu_auth_check(auth, 1000, "suspend", NULL) == WUBU_AUTH_DENY,
          "explicit deny overrules the earlier allow for uid 1000");
    /* but the wildcard still lets OTHER uids suspend (deny was uid-specific) */
    CHECK(wubu_auth_check(auth, 4242, "suspend", NULL) == WUBU_AUTH_ALLOW,
          "wildcard still allows a DIFFERENT uid after the specific deny");

    /* interactive rule: FIRST no agent -> fails closed */
    wubu_auth_add_rule(auth, 1000, "format-disk", WUBU_AUTH_INTERACT);
    CHECK(wubu_auth_has_agent(auth) == 0, "no agent yet");
    CHECK(wubu_auth_check(auth, 1000, "format-disk", &agent_enabled) == WUBU_AUTH_DENY,
          "interactive rule with no agent fails closed (deny)");
    CHECK(g_agent_calls == 0, "no agent -> agent never consulted");

    /* now register the agent: interactive resolves via it */
    g_agent_approve = 1;
    wubu_auth_set_agent(auth, agent, NULL);
    CHECK(wubu_auth_has_agent(auth) == 1, "agent registered");
    wubu_auth_check(auth, 1000, "format-disk", &agent_enabled);
    CHECK(agent_enabled == 1, "agent_enabled reflects the registered agent");
    CHECK(wubu_auth_check(auth, 1000, "format-disk", NULL) == WUBU_AUTH_ALLOW,
          "interactive rule + approving agent -> allow");
    CHECK(g_agent_calls == 2, "agent consulted once per prompt (2 prompts so far)");

    /* agent denies -> interactive resolves to deny */
    g_agent_approve = 0;
    CHECK(wubu_auth_check(auth, 1000, "format-disk", NULL) == WUBU_AUTH_DENY,
          "interactive rule + denying agent -> deny");
    CHECK(g_agent_calls == 3, "agent consulted again on the third prompt");

    /* clear() empties the rule table but keeps the agent slot */
    wubu_auth_clear(auth);
    CHECK(wubu_auth_rule_count(auth) == 0, "clear() empties the rule table");
    CHECK(wubu_auth_has_agent(auth) == 1, "clear() keeps the agent registration");

    /* rule() replaces, does not duplicate */
    wubu_auth_clear(auth);
    wubu_auth_add_rule(auth, 1000, "reboot", WUBU_AUTH_ALLOW);
    wubu_auth_add_rule(auth, 1000, "reboot", WUBU_AUTH_DENY);
    CHECK(wubu_auth_rule_count(auth) == 1, "re-adding the same {uid,action} replaces (no dup)");
    CHECK(wubu_auth_check(auth, 1000, "reboot", NULL) == WUBU_AUTH_DENY,
          "the replacement (deny) is what applies");

    wubu_auth_destroy(auth);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
