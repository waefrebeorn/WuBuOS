/*
 * wubu_ns_bridge_test.c -- verify the Namespace Bridge control plane.
 *
 * Uses dependency-injected mock service ops so we verify the dispatch
 * routing (rip-off-systemd play) WITHOUT shelling out to arch-chroot/
 * systemctl. Real archd ops are exercised separately by wubu_archd_test.
 */

#define _GNU_SOURCE
#include "wubu_ns_bridge.h"
#include "wubu_bottles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else      { g_fail++; printf("  ❌ %s\n", msg); } \
} while (0)

/* -- Mock service ops (record which op was called) ---------------- */

static const char *g_last_root = NULL;
static const char *g_last_svc  = NULL;
static int         g_last_op   = -1;  /* 0=start 1=stop 2=restart 3=enable 4=disable */
static int         g_status_calls = 0;

static int mock_start(WubuArchd *d, const char *root, const char *svc)
{ g_last_op=0; g_last_root=root; g_last_svc=svc; return 0; }
static int mock_stop(WubuArchd *d, const char *root, const char *svc)
{ g_last_op=1; g_last_root=root; g_last_svc=svc; return 0; }
static int mock_restart(WubuArchd *d, const char *root, const char *svc)
{ g_last_op=2; g_last_root=root; g_last_svc=svc; return 0; }
static int mock_enable(WubuArchd *d, const char *root, const char *svc)
{ g_last_op=3; g_last_root=root; g_last_svc=svc; return 0; }
static int mock_disable(WubuArchd *d, const char *root, const char *svc)
{ g_last_op=4; g_last_root=root; g_last_svc=svc; return 0; }
static int mock_status(WubuArchd *d, const char *root, const char *svc,
                       WubuArchService *out)
{ g_status_calls++; g_last_root=root; g_last_svc=svc;
  memset(out, 0, sizeof(*out));
  strncpy(out->name, svc, 64);
  strncpy(out->root_name, root, 64);
  out->state = SERVICE_STATE_RUNNING; out->pid = 4242;
  return 0; }

static const wubu_ns_svc_ops_t mock_ops = {
    mock_start, mock_stop, mock_restart, mock_enable, mock_disable, mock_status
};

/* -- Helpers ------------------------------------------------------- */

static char g_tmp[4096];
static int file_exists(const char *rel) {
    char p[8192]; snprintf(p, sizeof(p), "%s/%s", g_tmp, rel);
    struct stat st; return stat(p, &st) == 0;
}
static int file_contains(const char *rel, const char *needle) {
    char p[8192]; snprintf(p, sizeof(p), "%s/%s", g_tmp, rel);
    FILE *f = fopen(p, "r"); if (!f) return 0;
    char buf[4096]; int found = 0;
    while (fgets(buf, sizeof(buf), f))
        if (strstr(buf, needle)) { found = 1; break; }
    fclose(f); return found;
}

/* -- Tests --------------------------------------------------------- */

static void test_service_namespace(void) {
    printf("\n-- Service control plane (rip off systemd) --\n");
    wubu_ns_bridge_create(g_tmp);

    /* publish a service under root "arch0" */
    int rc = wubu_ns_publish_service(NULL, "arch0", "gamescope", &mock_ops);
    CHECK(rc == 0, "publish service gamescope under arch0");
    CHECK(file_exists("svc/arch0/gamescope/status"),
          "/n/svc/arch0/gamescope/status exists");
    CHECK(file_exists("svc/arch0/gamescope/ctl"),
          "/n/svc/arch0/gamescope/ctl exists");
    CHECK(file_contains("svc/arch0/gamescope/status", "state: active"),
          "status snapshots SERVICE_STATE_RUNNING -> 'active'");
    CHECK(file_contains("svc/arch0/gamescope/status", "pid: 4242"),
          "status reports pid from mock");

    /* ctl dispatch routes to the right op */
    g_last_op = -1;
    CHECK(wubu_ns_svc_ctl(NULL, "arch0", "gamescope", "start", &mock_ops) == 0,
          "svc_ctl start returns 0");
    CHECK(g_last_op == 0, "start routed to svc_start op");

    CHECK(wubu_ns_svc_ctl(NULL, "arch0", "gamescope", "stop", &mock_ops) == 0,
          "svc_ctl stop returns 0");
    CHECK(g_last_op == 1, "stop routed to svc_stop op");

    CHECK(wubu_ns_svc_ctl(NULL, "arch0", "gamescope", "restart", &mock_ops) == 0,
          "svc_ctl restart returns 0");
    CHECK(g_last_op == 2, "restart routed to svc_restart op");

    CHECK(wubu_ns_svc_ctl(NULL, "arch0", "gamescope", "enable", &mock_ops) == 0,
          "svc_ctl enable returns 0");
    CHECK(g_last_op == 3, "enable routed to svc_enable op");

    CHECK(wubu_ns_svc_ctl(NULL, "arch0", "gamescope", "disable", &mock_ops) == 0,
          "svc_ctl disable returns 0");
    CHECK(g_last_op == 4, "disable routed to svc_disable op");

    CHECK(wubu_ns_svc_ctl(NULL, "arch0", "gamescope", "frobnicate", &mock_ops) == -1,
          "unknown command rejected (-1)");
}

static void test_bottle_namespace(void) {
    printf("\n-- Bottle control plane (rip off Flatpak/Bottles) --\n");
    wubu_ns_bridge_create(g_tmp);

    WubuBottle *b = wubu_bottle_create("Cyberpunk", BOTTLE_TYPE_PROTON);
    CHECK(b != NULL, "create bottle Cyberpunk (proton)");
    b->installed = true; b->verified = true;

    int rc = wubu_ns_publish_bottle(b, "cyberpunk");
    CHECK(rc == 0, "publish bottle cyberpunk");
    CHECK(file_exists("bottles/cyberpunk/info"), "info exists");
    CHECK(file_exists("bottles/cyberpunk/verify"), "verify exists");
    CHECK(file_exists("bottles/cyberpunk/ctl"), "ctl exists");
    CHECK(file_contains("bottles/cyberpunk/info", "type: proton"),
          "info reports bottle type proton");
    CHECK(file_contains("bottles/cyberpunk/info", "installed: 1"),
          "info reports installed=1");

    /* bottle action routing (verify is pure-C; run uses a bogus proton
     * prefix so wubu_bottle_run fails fast instead of forking wine). */
    CHECK(wubu_ns_bottle_action(b, "verify") == 1,
          "bottle_action verify routes to wubu_bottle_verify (returns 1=ok)");

    WubuBottle *br = wubu_bottle_create("Hangless", BOTTLE_TYPE_PROTON);
    strncpy(br->prefix_path, "/nonexistent_prefix_xyz", sizeof(br->prefix_path) - 1);
    CHECK(wubu_ns_bottle_action(br, "run") == -1,
          "bottle_action run routes to wubu_bottle_run (fails fast: no launcher)");
    wubu_bottle_destroy(br);

    CHECK(wubu_ns_bottle_action(b, "explode") == -1, "unknown action rejected (-1)");

    wubu_bottle_destroy(b);
}

static void test_state_str(void) {
    printf("\n-- State string mapping --\n");
    CHECK(strcmp(wubu_ns_state_str(SERVICE_STATE_RUNNING), "active") == 0,
          "RUNNING -> active");
    CHECK(strcmp(wubu_ns_state_str(SERVICE_STATE_DISABLED), "inactive") == 0,
          "DISABLED -> inactive");
    CHECK(strcmp(wubu_ns_state_str(SERVICE_STATE_FAILED), "failed") == 0,
          "FAILED -> failed");
}

int main(void) {
    /* temp namespace root */
    char tmpl[] = "/tmp/wubu_ns_XXXXXX";
    char *d = mkdtemp(tmpl);
    if (!d) { printf("mkdtemp failed\n"); return 1; }
    strcpy(g_tmp, d);

    test_state_str();
    test_service_namespace();
    test_bottle_namespace();

    /* cleanup */
    char cmd[9000]; snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp);
    system(cmd);

    printf("\n==================================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("==================================================\n");
    return g_fail == 0 ? 0 : 1;
}
