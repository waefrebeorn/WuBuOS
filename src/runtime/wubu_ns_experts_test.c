/* src/runtime/wubu_ns_experts_test.c
 *
 * The /n/experts subtree test. Asserts the slot-bank state is mirrored into
 * /n over a REAL ns bridge root (g_ns_root set directly; links wubu_ns_fs.o
 * + wubu_ns_experts.c):
 *   1. sync creates /n/experts with config + stats
 *   2. per-layer residency renders one line per slot
 *   3. stats updates flow through on re-sync
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_test.h"
#include "wubu_ns_experts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NSROOT "/tmp/ns_experts_test"

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
    printf("=== wubu_ns_experts_test (the /n/experts slot-bank subtree) ===\n");
    rm_rf(NSROOT);
    mkdir(NSROOT, 0755);
    g_ns_root = NSROOT;

    /* sync before the root is set -> -1 */
    g_ns_root = NULL;
    CHECK(wubu_ns_experts_sync() == -1, "sync without ns root returns -1");
    g_ns_root = NSROOT;

    /* config + stats */
    wubu_ns_experts_set_config(43, 256, 2048, 5632, 8);
    wubu_ns_experts_set_stats(120, 9800, 104857600LL);
    /* layer 0: 3 of 8 slots occupied (experts 5, 12, 300->invalid so use 5,12,7) */
    int slots0[3] = {5, 12, 7};
    long lru0[3]   = {100, 250, 90};
    wubu_ns_experts_set_layer(0, slots0, lru0, 3);
    CHECK(wubu_ns_experts_sync() == 0, "sync /n/experts");

    char buf[4096];
    CHECK(read_file(NSROOT "/experts/config", buf, sizeof buf) &&
          strstr(buf, "layers 43") && strstr(buf, "experts 256") &&
          strstr(buf, "slot_bank 8"), "/n/experts/config reflects the bank");
    CHECK(read_file(NSROOT "/experts/stats", buf, sizeof buf) &&
          strstr(buf, "pageins 120") && strstr(buf, "hits 9800"),
          "/n/experts/stats reflects pageins/hits");
    CHECK(read_file(NSROOT "/experts/layer/0", buf, sizeof buf) &&
          strstr(buf, "0:5:100") && strstr(buf, "1:12:250") && strstr(buf, "2:7:90"),
          "/n/experts/layer/0 lists slot:expert:lru per resident slot");

    /* stats update flows through on re-sync */
    wubu_ns_experts_set_stats(121, 9801, 104900000LL);
    CHECK(wubu_ns_experts_sync() == 0, "re-sync /n/experts");
    CHECK(read_file(NSROOT "/experts/stats", buf, sizeof buf) &&
          strstr(buf, "pageins 121"), "/n/experts/stats updated -> 121");

    rm_rf(NSROOT);
    printf("=== wubu_ns_experts_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
