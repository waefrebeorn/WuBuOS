/* src/runtime/wubu_ns_hive_test.c
 *
 * The /n/hive subtree test. Asserts the diagnostic-hive state is mirrored
 * into /n over a REAL ns bridge root (g_ns_root set directly; links
 * wubu_ns_fs.o + wubu_ns_hive.c):
 *   1. sync creates /n/hive with loss/util/grad/fitness/ledger/batch
 *   2. a new diagnosis + ledger flow through on re-sync
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_test.h"
#include "wubu_ns_hive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NSROOT "/tmp/ns_hive_test"

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
    printf("=== wubu_ns_hive_test (the /n/hive diagnostic subtree) ===\n");
    rm_rf(NSROOT);
    mkdir(NSROOT, 0755);
    g_ns_root = NSROOT;

    g_ns_root = NULL;
    CHECK(wubu_ns_hive_sync() == -1, "sync without ns root returns -1");
    g_ns_root = NSROOT;

    wubu_ns_hive_diag_t d = {
        .loss = 7.32f, .loss_ema = 7.40f, .slope = -0.012f, .plateau = 0,
        .n_experts = 256, .util_min = 0.001f, .util_max = 0.08f, .util_mean = 0.0039f,
        .grad_norm_max = 1.8f, .grad_norm_mean = 0.42f, .n_dead_cells = 3,
        .fitness = 0.71f, .prev_fitness = 0.68f,
        .batch = 2000, .epoch = 12,
    };
    wubu_ns_hive_set_diagnosis(&d);
    wubu_ns_hive_set_ledger(140, 37, 12);
    CHECK(wubu_ns_hive_sync() == 0, "sync /n/hive");

    char buf[256];
    CHECK(read_file(NSROOT "/hive/loss", buf, sizeof buf) &&
          strstr(buf, "loss 7.3200") && strstr(buf, "plateau 0"),
          "/n/hive/loss reflects the loss surface");
    CHECK(read_file(NSROOT "/hive/util", buf, sizeof buf) &&
          strstr(buf, "mean 0.0039"), "/n/hive/util reflects route utilization");
    CHECK(read_file(NSROOT "/hive/grad", buf, sizeof buf) &&
          strstr(buf, "dead_cells 3"), "/n/hive/grad reflects gradient health");
    CHECK(read_file(NSROOT "/hive/fitness", buf, sizeof buf) &&
          strstr(buf, "fitness 0.7100") && strstr(buf, "prev 0.6800"),
          "/n/hive/fitness reflects the fitness vector");
    CHECK(read_file(NSROOT "/hive/ledger", buf, sizeof buf) &&
          strstr(buf, "accepted 140") && strstr(buf, "rejected 37"),
          "/n/hive/ledger reflects mutation gate outcomes");
    CHECK(read_file(NSROOT "/hive/batch", buf, sizeof buf) &&
          strstr(buf, "batch 2000") && strstr(buf, "n_experts 256"),
          "/n/hive/batch reflects provenance");

    /* a plateau fires + ledger advances -> flows through on re-sync */
    d.plateau = 1; d.batch = 2001;
    wubu_ns_hive_set_diagnosis(&d);
    wubu_ns_hive_set_ledger(141, 37, 12);
    CHECK(wubu_ns_hive_sync() == 0, "re-sync /n/hive");
    CHECK(read_file(NSROOT "/hive/loss", buf, sizeof buf) &&
          strstr(buf, "plateau 1"), "/n/hive/loss updated -> plateau 1");
    CHECK(read_file(NSROOT "/hive/ledger", buf, sizeof buf) &&
          strstr(buf, "accepted 141"), "/n/hive/ledger updated -> accepted 141");

    rm_rf(NSROOT);
    printf("=== wubu_ns_hive_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
