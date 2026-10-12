/*
 * wubu_ns_brain.c -- WuBuOS Namespace Bridge: the Brain's model catalog +
 * live training run (Track A5).
 *
 * Extends the 9P meta-layer so `ls /n` shows the WHOLE machine -- kernel,
 * hw, and now the Brain's live state -- same doctrine as /n/kernel, /n/dram,
 * /n/bonzi, /n/experts, /n/hive.
 *
 *   /n/models/catalog   -- "n_tensors" + the first entries (name elems
 *                          offset), the uniform tensor catalog
 *   /n/models/<name>    -- one file per tensor: "elems offset dtype shape"
 *   /n/train/loss       -- the live loss (this batch + ema)
 *   /n/train/lr         -- the live learning rate(s)
 *   /n/train/step       -- "step epoch" (provenance)
 *
 * The tensor store + trainer live in wubuwizard (the Brain); this module
 * keeps its OWN mirror and the owner pushes state in, then sync flushes to
 * /n. Dependency-light: links only wubu_ns_fs.o (same discipline as the
 * other namespace modules).
 *
 * Reuses g_ns_root/ns_mkdir/ns_write from wubu_ns_fs.c. C11, opaque, minimal.
 */

#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_brain.h"
#include <stdio.h>
#include <string.h>

#define NSB_MAX_TENSORS 256
#define NSB_MODELS_DIR  "models"

/* The mirror of the Brain's catalog + training state. */
static struct {
    /* model catalog */
    int  n_tensors;
    struct { char name[128]; int64_t elems; int64_t offset; char dtype[16]; } tensors[NSB_MAX_TENSORS];
    /* training run */
    float loss, loss_ema;
    float lr, muon_lr, adam_lr;
    uint32_t step, epoch;
} g_b;

/* -- model catalog --------------------------------------------------- */
void wubu_ns_brain_set_catalog_count(int n_tensors) { g_b.n_tensors = n_tensors; }

int wubu_ns_brain_add_tensor(const char *name, int64_t elems, int64_t offset,
                             const char *dtype) {
    if (g_b.n_tensors >= NSB_MAX_TENSORS || !name) return -1;
    /* store into a slot; the caller advances the count via set_catalog_count
     * OR we auto-append. We append here for convenience. */
    int i = g_b.n_tensors;
    snprintf(g_b.tensors[i].name, sizeof(g_b.tensors[i].name), "%s", name);
    g_b.tensors[i].elems = elems;
    g_b.tensors[i].offset = offset;
    snprintf(g_b.tensors[i].dtype, sizeof(g_b.tensors[i].dtype), "%s",
             dtype ? dtype : "f32");
    g_b.n_tensors = i + 1;
    return 0;
}

/* -- training run ---------------------------------------------------- */
void wubu_ns_brain_set_train(float loss, float loss_ema, float lr,
                             float muon_lr, float adam_lr,
                             uint32_t step, uint32_t epoch) {
    g_b.loss = loss; g_b.loss_ema = loss_ema;
    g_b.lr = lr; g_b.muon_lr = muon_lr; g_b.adam_lr = adam_lr;
    g_b.step = step; g_b.epoch = epoch;
}

/* Flush the mirror into the /n/models and /n/train trees. Returns 0 on success,
 * -1 if the namespace root isn't set. */
int wubu_ns_brain_sync(void)
{
    if (!g_ns_root) return -1;
    char sub[160], buf[4096];

    if (ns_mkdir(NSB_MODELS_DIR) != 0) return -1;
    if (ns_mkdir("train") != 0) return -1;

    /* /n/models/catalog -- summary + entries */
    snprintf(sub, sizeof(sub), NSB_MODELS_DIR "/catalog");
    int off = snprintf(buf, sizeof(buf), "n_tensors %d\n", g_b.n_tensors);
    for (int i = 0; i < g_b.n_tensors && off < (int)sizeof(buf) - 160; i++) {
        off += snprintf(buf + off, sizeof(buf) - off, "%s elems=%lld offset=%lld %s\n",
                        g_b.tensors[i].name,
                        (long long)g_b.tensors[i].elems,
                        (long long)g_b.tensors[i].offset,
                        g_b.tensors[i].dtype);
    }
    if (ns_write(sub, buf) != 0) return -1;

    /* /n/models/<name> -- one file per tensor */
    for (int i = 0; i < g_b.n_tensors; i++) {
        snprintf(sub, sizeof(sub), NSB_MODELS_DIR "/%s", g_b.tensors[i].name);
        snprintf(buf, sizeof(buf), "elems %lld, offset %lld, dtype %s\n",
                 (long long)g_b.tensors[i].elems,
                 (long long)g_b.tensors[i].offset,
                 g_b.tensors[i].dtype);
        if (ns_write(sub, buf) != 0) return -1;
    }

    /* /n/train entries */
    snprintf(sub, sizeof(sub), "train/loss");
    snprintf(buf, sizeof(buf), "loss %.4f, ema %.4f\n", g_b.loss, g_b.loss_ema);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "train/lr");
    snprintf(buf, sizeof(buf), "lr %.6g, muon %.6g, adam %.6g\n",
             g_b.lr, g_b.muon_lr, g_b.adam_lr);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "train/step");
    snprintf(buf, sizeof(buf), "step %u, epoch %u\n", g_b.step, g_b.epoch);
    if (ns_write(sub, buf) != 0) return -1;

    return 0;
}
