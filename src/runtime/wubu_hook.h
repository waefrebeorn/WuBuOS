/*
 * wubu_hook.h -- mkinitcpio: the hook chain.
 *
 * mkinitcpio builds an initramfs by running a chain of HOOKS in a fixed stage
 * order (pre-trigger, pre-mount, pre-swap, pre-lvm, pre-enc, mount, late,
 * final).  The rules that matter:
 *
 *   * STAGE ORDER IS FIXED.  A hook registered later never runs earlier than a
 *     hook in an earlier stage -- 'mount' must not run before 'pre-mount'.
 *   * Within a stage, REGISTRATION ORDER is kept, because hooks of the same
 *     stage are frequently order-dependent.
 *   * A NON-EXECUTABLE hook is SKIPPED, not run and not an error.  A hook that
 *     is present but not +x is how a distribution ships an optional step.
 *   * A failing OPTIONAL hook warns and the chain CONTINUES.
 *   * A failing REQUIRED hook ABORTS the chain.  Continuing past a failed
 *     'mount' hook would produce an initramfs that cannot boot.
 *   * After an abort, NO LATER HOOK RUNS, and the failure is reported BY NAME.
 *   * A hook may not be registered twice.
 *   * A chain may not be run twice without a reset: a second run would re-run
 *     side effects on an already-built initramfs.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_HOOK_H
#define WUBU_HOOK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WUBU_HOOK_EARLY = 0,
    WUBU_HOOK_PRE_MOUNT,
    WUBU_HOOK_PRE_SWAP,
    WUBU_HOOK_PRE_LVM,
    WUBU_HOOK_PRE_ENC,
    WUBU_HOOK_MOUNT,
    WUBU_HOOK_LATE,
    WUBU_HOOK_FINAL,
    WUBU_HOOK_STAGE_COUNT
} wubu_hook_stage_t;

const char *wubu_hook_stage_name(wubu_hook_stage_t s);

typedef struct {
    char              name[64];
    wubu_hook_stage_t stage;
    bool              executable;
    bool              required;
    bool              ran;
    bool              skipped;
    bool              failed;
} wubu_hook_t;

#define WUBU_HOOK_MAX 64

typedef struct {
    uint32_t     count;
    wubu_hook_t  hooks[WUBU_HOOK_MAX];
    bool         ran;
    char         failed_name[64];
} wubu_hook_chain_t;

void wubu_hook_chain_init(wubu_hook_chain_t *c);

/* Register a hook.  Returns -1 on a duplicate name or a full chain. */
int  wubu_hook_add(wubu_hook_chain_t *c, const char *name,
                   wubu_hook_stage_t stage, bool executable, bool required);

/* The outcome of one hook.  The real runner execs the hook binary; the model
 * calls this.  Return true when the hook succeeded. */
typedef bool (*wubu_hook_fn)(const wubu_hook_t *h, void *user);

/* Run the chain in stage order.  Returns 0 when it completed (or only optional
 * hooks failed), -1 when a REQUIRED hook failed.  *failed_name receives the
 * name of the hook that aborted the chain, if any. */
int wubu_hook_run(wubu_hook_chain_t *c, wubu_hook_fn fn, void *user,
                  const char **failed_name);

/* Reset a run chain so it can be run again. */
void wubu_hook_chain_reset(wubu_hook_chain_t *c);

uint32_t wubu_hook_count(const wubu_hook_chain_t *c);

#endif /* WUBU_HOOK_H */
