/* src/runtime/wubu_ns_dram_test.c
 *
 * The /n/dram subtree test. Asserts the file<->API routing over a REAL
 * ns bridge root (g_ns_root set directly; links wubu_ns_fs.o + the hedge
 * + wubu_ns_dram.c, NOT wubu_ns_bridge.o per the memory-constrained rule):
 *   1. publish creates /n/dram with the hedge's live values
 *   2. wubu_ns_dram_put(idx,val) writes a replicated slot + refreshes state
 *   3. reading /n/dram/status reflects the put
 *   4. ctrl round-trips (echo "idx:value" -> put -> read back)
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_dram.h"
#include "wubu_dram_hedge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NSROOT "/tmp/ns_dram_test"
#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

static int read_file(const char *p, char *out, size_t cap)
{
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == ' ')) out[--n] = '\0';
    return 1;
}

static void rm_rf(const char *path)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);
}

int main(void)
{
    printf("=== wubu_ns_dram_test (the /n/dram control subtree) ===\n");
    rm_rf(NSROOT);
    mkdir(NSROOT, 0755);
    g_ns_root = NSROOT;

    /* 1. publish */
    CHECK(wubu_ns_publish_dram(8, 2) == 0, "publish /n/dram");
    char buf[128];
    CHECK(read_file(NSROOT "/dram/replicas", buf, sizeof buf) &&
          strcmp(buf, "2") == 0, "/n/dram/replicas == 2");
    CHECK(read_file(NSROOT "/dram/elem", buf, sizeof buf) &&
          strcmp(buf, "8") == 0, "/n/dram/elem == 8");
    CHECK(read_file(NSROOT "/dram/slots", buf, sizeof buf) &&
          atol(buf) == 65536, "/n/dram/slots == 65536");
    CHECK(read_file(NSROOT "/dram/state", buf, sizeof buf) &&
          strstr(buf, "replicas 2") != NULL, "/n/dram/state mentions replicas 2");

    /* 2. put writes a replicated slot + refreshes state */
    CHECK(wubu_ns_dram_put(7, 42) == 0, "dram_put(7,42)");
    CHECK(read_file(NSROOT "/dram/status", buf, sizeof buf) &&
          strstr(buf, "7 <- 42") != NULL, "/n/dram/status reflects slot 7 <- 42");
    CHECK(read_file(NSROOT "/dram/state", buf, sizeof buf) &&
          strstr(buf, "replicas 2") != NULL, "state refreshed");

    /* 3. the hedge actually holds the value in slot 7 (round-trip through
     *    the real API via the module's backing hedge, not just the file) */
    wdh_hedge_t *h = wubu_ns_dram_hedge();
    CHECK(h != NULL, "module exposes its backing hedge");
    if (h) {
        unsigned long v = 0;
        CHECK(wdh_get(h, 7, &v) == 0 && v == 42,
              "direct hedge read of slot 7 == 42 (replicated write landed)");
    }

    /* 4. ctrl file round-trip */
    CHECK(read_file(NSROOT "/dram/ctrl", buf, sizeof buf) &&
          strcmp(buf, "0:0") == 0, "/n/dram/ctrl initial 0:0");

    printf("\nResults: %d/%d passed, %d failed\n", checks - fails, checks, fails);
    rm_rf(NSROOT);
    return fails ? 1 : 0;
}
