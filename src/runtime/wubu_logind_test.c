/* wubu_logindd_test.c -- P3 logind-equivalent (Mint baseline §4.3 / §153).
 * Register sessions (id/seat/type/class/state), query by id / by active state,
 * and run machine power ops gated by an injectable authorizer:
 *   - an authorized op is allowed (returns 0)
 *   - a denied op is refused (returns the authorizer's deny code)
 *   - no authorizer set / no active session -> power refused (-1)
 *   - the /n/session-wise inspection (id/seat/type/state) is correct
 */
#include "wubu_logind.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

/* authorizer: uid==1000 may suspend; NOTHING may reboot (deny always);
 * everything else denies. */
static int authorize(wubu_logindd_t *login,
                     const wubu_logindd_session_t *sess,
                     wubu_logindd_power_op_t op, void *ud) {
    (void)login; (void)ud;
    if (sess->uid == 1000 && op == WUBU_POWER_SUSPEND) return 0;  /* allow */
    return 42;  /* deny (arbitrary nonzero code) */
}

int main(void) {
    wubu_logindd_t *login = wubu_logindd_create();
    CHECK(login != NULL, "login registry created");

    /* no sessions yet */
    CHECK(wubu_logindd_session_count(login) == 0, "starts empty");
    CHECK(wubu_logindd_active(login) == NULL, "no active session initially");

    wubu_logindd_session_t s;
    memset(&s, 0, sizeof(s));
    s.id = 1;
    strcpy(s.seat, "seat0");
    s.state = WUBU_SESSION_STATE_ACTIVE;
    s.type = WUBU_SESSION_TYPE_X11;
    strcpy(s.class_, "user");
    s.uid = 1000;

    CHECK(wubu_logindd_add_session(login, &s) == 0, "active session registered");
    CHECK(wubu_logindd_session_count(login) == 1, "one session tracked");

    const wubu_logindd_session_t *byid = wubu_logindd_session_by_id(login, 1);
    CHECK(byid != NULL && strcmp(byid->seat, "seat0") == 0,
          "lookup by id finds seat0");
    CHECK(byid->type == WUBU_SESSION_TYPE_X11 && byid->uid == 1000,
          "session type x11, class user, uid 1000 recorded");

    const wubu_logindd_session_t *act = wubu_logindd_active(login);
    CHECK(act != NULL && act->id == 1, "active session is the registered one");

    /* idempotent re-add replaces, does not grow the registry */
    s.state = WUBU_SESSION_STATE_ONLINE;
    wubu_logindd_add_session(login, &s);
    CHECK(wubu_logindd_session_count(login) == 1, "re-adding same id replaces (no dup)");
    CHECK(wubu_logindd_active(login) == NULL, "session no longer active after update");

    /* power gating */
    CHECK(wubu_logindd_power(login, WUBU_POWER_SUSPEND) == -1,
          "power refused before an authorizer is set (no auth surface)");

    wubu_logindd_set_authorizer(login, authorize, NULL);
    CHECK(wubu_logindd_power(login, WUBU_POWER_SUSPEND) == -1,
          "power still refused: no ACTIVE session to authorize for");

    /* restore active state, then authorized suspend is allowed */
    s.state = WUBU_SESSION_STATE_ACTIVE;
    wubu_logindd_add_session(login, &s);
    CHECK(wubu_logindd_power(login, WUBU_POWER_SUSPEND) == 0,
          "authorized suspend is allowed (uid 1000 + suspend)");
    CHECK(wubu_logindd_power(login, WUBU_POWER_REBOOT) == 42,
          "denied reboot returns the authorizer's deny code (42)");
    CHECK(wubu_logindd_power(login, WUBU_POWER_SHUTDOWN) == 42,
          "denied shutdown also refused for this uid");

    /* a non-privileged uid session cannot suspend */
    s.uid = 1001;
    wubu_logindd_add_session(login, &s);
    CHECK(wubu_logindd_power(login, WUBU_POWER_SUSPEND) == 42,
          "uid 1001 is denied suspend (authorizer is uid-sensitive)");

    CHECK(wubu_logindd_session_at(login, 99) == NULL, "out-of-range session lookup is NULL");

    wubu_logindd_destroy(login);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
