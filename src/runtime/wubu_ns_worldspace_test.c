/*
 * wubu_ns_worldspace_test.c -- prove the /n/worldspace subtree publishes the
 * body's space-facing state, and that it says so honestly when no portal is
 * configured and no agent is enroled.
 */
#include "../egojar.h"
#include "wubu_ns_bridge_internal.h"
#include "../kernel/wubu_world.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NSROOT  "/tmp/wubu_ws_ns"
#define TESTHOME "/tmp/wubu_ws_home"

static int failures;
static int checks;
#define CHECK(c, m) do { \
    checks++; \
    if (!(c)) { printf("  FAIL: %s\n", m); failures++; } \
} while (0)

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int main(void)
{
    printf("\n=== /n/worldspace ===\n");

    /* a deterministic home: no portal file, no agent card */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s %s", NSROOT, TESTHOME);
    if (system(cmd) != 0) { /* best effort */ }
    mkdir(NSROOT, 0755);
    mkdir(TESTHOME, 0755);
    setenv("HOME", TESTHOME, 1);
    *(const char **)&g_ns_root = NSROOT;

    CHECK(wubu_ns_publish_worldspace() == 0, "publish /n/worldspace");

    char path[300], *buf;

    /* the world snapshot is real state, not a placeholder */
    snprintf(path, sizeof(path), "%s/worldspace/state", NSROOT);
    buf = read_file(path);
    CHECK(buf != NULL, "worldspace/state exists");
    if (buf) {
        CHECK(strchr(buf, '[') != NULL, "worldspace/state is structured");
        CHECK(strstr(buf, "power[") != NULL, "worldspace/state carries power");
        free(buf);
    }

    /* no agent card -> it says so, never a fabricated identity */
    snprintf(path, sizeof(path), "%s/worldspace/agent", NSROOT);
    buf = read_file(path);
    CHECK(buf != NULL, "worldspace/agent exists");
    if (buf) {
        CHECK(strstr(buf, "not enroled") != NULL, "worldspace/agent says not enroled");
        free(buf);
    }

    /* no portal configured -> it says so and names how to set one */
    snprintf(path, sizeof(path), "%s/worldspace/portal", NSROOT);
    buf = read_file(path);
    CHECK(buf != NULL, "worldspace/portal exists");
    if (buf) {
        CHECK(strstr(buf, "unconfigured") != NULL, "worldspace/portal says unconfigured");
        CHECK(strstr(buf, "portal=") != NULL, "worldspace/portal names how to set one");
        free(buf);
    }

    /* a configured portal is read back verbatim */
    char conf[300];
    snprintf(conf, sizeof(conf), "%s/.config", TESTHOME);
    mkdir(conf, 0755);
    snprintf(conf, sizeof(conf), "%s/.config/wubu", TESTHOME);
    mkdir(conf, 0755);
    snprintf(conf, sizeof(conf), "%s/.config/wubu/worldspace.conf", TESTHOME);
    FILE *f = fopen(conf, "w");
    CHECK(f != NULL, "the portal config writes");
    if (f) { fprintf(f, "portal=a2a://space.example:9000\n"); fclose(f); }
    CHECK(wubu_ns_publish_worldspace() == 0, "republish with a portal");
    buf = read_file(path);
    CHECK(buf != NULL && strstr(buf, "a2a://space.example:9000") != NULL,
          "the configured portal is published");
    free(buf);

    /* the transport and capability surface */
    snprintf(path, sizeof(path), "%s/worldspace/transport", NSROOT);
    buf = read_file(path);
    CHECK(buf != NULL, "worldspace/transport exists");
    if (buf) {
        CHECK(strstr(buf, "sdr") != NULL, "the SDR transport is named");
        CHECK(strstr(buf, "swarm") != NULL, "the swarm ledger is named");
        free(buf);
    }
    snprintf(path, sizeof(path), "%s/worldspace/capabilities", NSROOT);
    buf = read_file(path);
    CHECK(buf != NULL, "worldspace/capabilities exists");
    if (buf) {
        CHECK(strstr(buf, "world-sample") != NULL, "world-sample is a capability");
        CHECK(strstr(buf, "agent-identity") != NULL, "agent-identity is a capability");
        free(buf);
    }

    printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
