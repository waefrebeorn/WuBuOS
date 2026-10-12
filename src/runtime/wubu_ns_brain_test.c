/* src/runtime/wubu_ns_brain_test.c
 *
 * The /n/models + /n/train subtree test (Track A5). Asserts the Brain's
 * catalog + training state mirror into /n over a REAL ns bridge root:
 *   1. catalog renders n_tensors + per-tensor files
 *   2. training loss/lr/step render
 *   3. updates flow through on re-sync
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_test.h"
#include "wubu_ns_brain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NSROOT "/tmp/ns_brain_test"

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
    printf("=== wubu_ns_brain_test (the /n/models + /n/train subtrees) ===\n");
    rm_rf(NSROOT);
    mkdir(NSROOT, 0755);
    g_ns_root = NSROOT;

    g_ns_root = NULL;
    CHECK(wubu_ns_brain_sync() == -1, "sync without ns root returns -1");
    g_ns_root = NSROOT;

    /* catalog: 2 tensors */
    wubu_ns_brain_add_tensor("model.embed_tokens.weight", 50000*512, 0, "f32");
    wubu_ns_brain_add_tensor("model.layers.0.attn.q.weight", 512*512, 102400000, "f32");
    /* training run */
    wubu_ns_brain_set_train(7.32f, 7.40f, 1e-4f, 2e-2f, 2e-3f, 2000, 12);
    CHECK(wubu_ns_brain_sync() == 0, "sync /n/models + /n/train");

    char buf[4096];
    CHECK(read_file(NSROOT "/models/catalog", buf, sizeof buf) &&
          strstr(buf, "n_tensors 2") && strstr(buf, "model.embed_tokens.weight"),
          "/n/models/catalog lists n_tensors + entries");
    CHECK(read_file(NSROOT "/models/model.embed_tokens.weight", buf, sizeof buf) &&
          strstr(buf, "elems 25600000"),
          "/n/models/<name> carries the tensor's elems/offset/dtype");
    CHECK(read_file(NSROOT "/train/loss", buf, sizeof buf) &&
          strstr(buf, "loss 7.3200") && strstr(buf, "ema 7.4000"),
          "/n/train/loss reflects the live loss");
    CHECK(read_file(NSROOT "/train/lr", buf, sizeof buf) &&
          strstr(buf, "muon"), "/n/train/lr reflects the learning rates");
    CHECK(read_file(NSROOT "/train/step", buf, sizeof buf) &&
          strstr(buf, "step 2000") && strstr(buf, "epoch 12"),
          "/n/train/step reflects provenance");

    /* a training step advances -> flows through */
    wubu_ns_brain_set_train(7.28f, 7.39f, 9e-5f, 2e-2f, 2e-3f, 2001, 12);
    CHECK(wubu_ns_brain_sync() == 0, "re-sync /n/models + /n/train");
    CHECK(read_file(NSROOT "/train/loss", buf, sizeof buf) &&
          strstr(buf, "loss 7.2800"), "/n/train/loss updated -> 7.28");
    CHECK(read_file(NSROOT "/train/step", buf, sizeof buf) &&
          strstr(buf, "step 2001"), "/n/train/step updated -> 2001");

    rm_rf(NSROOT);
    printf("=== wubu_ns_brain_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
