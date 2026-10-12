/*
 * wubu_ns_brain.h -- the Brain's /n control plane (Track A5).
 *
 * Extends the 9P meta-layer so `ls /n` shows the whole machine including
 * the Brain's live state: the model tensor catalog at /n/models and the
 * live training run at /n/train. The owner pushes state via setters, then
 * wubu_ns_brain_sync() flushes to /n. See wubu_ns_brain.c.
 */
#ifndef WUBU_NS_BRAIN_H
#define WUBU_NS_BRAIN_H

#include <stdint.h>

/* Model catalog: add a tensor entry (name/elems/offset/dtype). Appends;
 * returns 0 on success, -1 if full. */
int wubu_ns_brain_add_tensor(const char *name, int64_t elems, int64_t offset,
                             const char *dtype);
/* Set the catalog count directly (alternative to repeated add). */
void wubu_ns_brain_set_catalog_count(int n_tensors);

/* Training run: push the live loss/lr/step/epoch. */
void wubu_ns_brain_set_train(float loss, float loss_ema, float lr,
                             float muon_lr, float adam_lr,
                             uint32_t step, uint32_t epoch);

/* Flush the mirror into the /n/models and /n/train trees. Returns 0 on success,
 * -1 if the namespace root isn't set. */
int wubu_ns_brain_sync(void);

#endif /* WUBU_NS_BRAIN_H */
