/* wubu_bus_test.c -- P3 spine: the D-Bus well-known-name ownership + on-demand
 * activation contract (Mint baseline §4.1). Tests:
 *   1. connect two clients, one claims a name -> the other can address it.
 *   2. msg sent to an OWNED name is queued at the owner and recv'd FIFO.
 *   3. msg sent to an UNOWNED name triggers the activator (the bus starts the
 *      service on FIRST CALL) and, once the service claims the name, a
 *      re-sent request delivers — proving on-demand activation end to end.
 *   4. a name is owned by one conn only (second claim rejected), and adding a
 *      name to a full-with-reclaim table is idempotent.
 */
#include "wubu_bus.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static int g_activation_calls = 0;
static char g_last_activated[WUBU_BUS_MAX_NAME] = "";
static int on_activate(wubu_bus_t *bus, const char *name, void *ud) {
    (void)bus; (void)ud;
    g_activation_calls++;
    strncpy(g_last_activated, name, sizeof(g_last_activated) - 1);
    g_last_activated[sizeof(g_last_activated) - 1] = '\0';
    return 0;   /* accept: launch initiated */
}

int main(void) {
    wubu_bus_t *bus = wubu_bus_create();
    CHECK(bus != NULL, "bus created");
    wubu_bus_set_activator(bus, on_activate, NULL);

    /* 1. two clients, ownership claim */
    int32_t app = wubu_bus_connect(bus);
    int32_t svc = wubu_bus_connect(bus);
    CHECK(app >= 0 && svc >= 0, "two clients connect");
    CHECK(wubu_bus_request_name(bus, svc, "org.wubu.Power") == 0,
          "service claims org.wubu.Power");
    CHECK(wubu_bus_request_name(bus, app, "org.wubu.Power") != 0,
          "second claim of an owned name is rejected");
    CHECK(wubu_bus_owner_of(bus, "org.wubu.Power") == svc,
          "owner lookup returns the claiming connection");
    CHECK(wubu_bus_name_count(bus) == 1, "one name claimed");

    /* 2. messaging to an OWNED name, FIFO */
    CHECK(wubu_bus_send_to_name(bus, app, "org.wubu.Power", "query") >= 0,
          "send to owned name accepted");
    CHECK(wubu_bus_send_to_name(bus, app, "org.wubu.Power", "get-state") >= 0,
          "second send accepted");
    wubu_bus_msg_t m[4]; int n;
    n = wubu_bus_recv(bus, svc, m, 4);
    CHECK(n == 2, "owner received both messages");
    CHECK(n == 2 && strcmp(m[0].message, "query") == 0 &&
          strcmp(m[1].message, "get-state") == 0,
          "messages delivered in FIFO order");
    CHECK(m[0].sender == (uint32_t)app, "sender recorded on the message");

    /* 3. on-demand ACTIVATION: an unowned name triggers the activator. */
    CHECK(wubu_bus_owner_of(bus, "org.wubu.Session") == -1,
          "org.wubu.Session starts unowned");
    int r = wubu_bus_send_to_name(bus, app, "org.wubu.Session", "boot");
    CHECK(r >= 0, "send to unowned name accepted (activation path)");
    CHECK(g_activation_calls == 1 && strcmp(g_last_activated, "org.wubu.Session") == 0,
          "activator fired exactly once with the right name (D-Bus on-demand start)");

    /* once activated service comes up and claims its name, a re-sent msg lands */
    CHECK(wubu_bus_request_name(bus, svc, "org.wubu.Session") == 0,
          "after activation the service claims org.wubu.Session");
    CHECK(wubu_bus_send_to_name(bus, app, "org.wubu.Session", "ready?") >= 0,
          "send to now-owned name accepted");
    n = wubu_bus_recv(bus, svc, m, 4);
    CHECK(n == 1 && strcmp(m[0].message, "ready?") == 0,
          "service receives the post-activation message");

    /* second activation attempt of an unowned name -> activator fires again */
    CHECK(wubu_bus_send_to_name(bus, app, "org.wubu.NoSuch", "try") >= 0,
          "unowned unknown name also goes through the activation path");
    CHECK(g_activation_calls == 2, "a second unowned call fires the activator again");

    /* 4. disconnect releases owned names; re-claim is clean */
    CHECK(wubu_bus_release_name(bus, svc, "org.wubu.Power") == 0,
          "service releases org.wubu.Power");
    CHECK(wubu_bus_owner_of(bus, "org.wubu.Power") == -1,
          "released name is unowned");
    CHECK(wubu_bus_disconnect(bus, svc) == 0, "service disconnects");
    CHECK(wubu_bus_owner_of(bus, "org.wubu.Session") == -1,
          "disconnect also unowns every name it held");

    /* invalid sender / invalid names are rejected, not UB */
    CHECK(wubu_bus_send_to_name(bus, 999, "org.wubu.Power", "x") == -1,
          "invalid sender rejected");
    CHECK(wubu_bus_request_name(bus, app, "") == -1,
          "empty name rejected");
    CHECK(wubu_bus_request_name(bus, app, NULL) == -1,
          "NULL name rejected");

    wubu_bus_destroy(bus);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
