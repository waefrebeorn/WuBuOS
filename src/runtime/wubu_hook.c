/*
 * wubu_hook.c -- mkinitcpio: the hook chain.
 *
 * See wubu_hook.h.
 */
#include "wubu_hook.h"

#include <string.h>

const char *wubu_hook_stage_name(wubu_hook_stage_t s)
{
    static const char *names[] = {
        "early", "pre-mount", "pre-swap", "pre-lvm",
        "pre-enc", "mount", "late", "final"
    };
    if ((int)s < 0 || (int)s >= WUBU_HOOK_STAGE_COUNT) return "unknown";
    return names[s];
}

void wubu_hook_chain_init(wubu_hook_chain_t *c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
}

uint32_t wubu_hook_count(const wubu_hook_chain_t *c)
{
    return c ? c->count : 0;
}

int wubu_hook_add(wubu_hook_chain_t *c, const char *name,
                  wubu_hook_stage_t stage, bool executable, bool required)
{
    if (!c || !name || !*name) return -1;
    if ((int)stage < 0 || (int)stage >= WUBU_HOOK_STAGE_COUNT) return -1;
    if (c->count >= WUBU_HOOK_MAX) return -1;

    /* a duplicate name is an ERROR, not a replace: registering 'mount' twice
     * would run one hook's side effects twice. */
    for (uint32_t i = 0; i < c->count; i++)
        if (strcmp(c->hooks[i].name, name) == 0) return -1;

    wubu_hook_t *h = &c->hooks[c->count++];
    memset(h, 0, sizeof(*h));
    strncpy(h->name, name, sizeof(h->name) - 1);
    h->stage = stage;
    h->executable = executable;
    h->required = required;
    return 0;
}

void wubu_hook_chain_reset(wubu_hook_chain_t *c)
{
    if (!c) return;
    for (uint32_t i = 0; i < c->count; i++) {
        c->hooks[i].ran = false;
        c->hooks[i].skipped = false;
        c->hooks[i].failed = false;
    }
    c->ran = false;
    c->failed_name[0] = '\0';
}

int wubu_hook_run(wubu_hook_chain_t *c, wubu_hook_fn fn, void *user,
                  const char **failed_name)
{
    if (failed_name) *failed_name = NULL;
    if (!c) return -1;
    /* A second run without a reset would re-run side effects on an
     * already-built initramfs. */
    if (c->ran) return -1;
    c->ran = true;

    /* STAGE ORDER is fixed, and within a stage REGISTRATION ORDER is kept.  A
     * selection over the array by stage gives both. */
    for (int stage = 0; stage < (int)WUBU_HOOK_STAGE_COUNT; stage++) {
        for (uint32_t i = 0; i < c->count; i++) {
            wubu_hook_t *h = &c->hooks[i];
            if ((int)h->stage != stage) continue;

            /* A non-executable hook is SKIPPED -- present but not +x is how a
             * distribution ships an optional step.  It is not an error. */
            if (!h->executable) {
                h->skipped = true;
                h->ran = true;
                continue;
            }
            if (!fn) return -1;

            bool ok = fn(h, user);
            h->ran = true;
            if (ok) continue;

            h->failed = true;
            /* An OPTIONAL failure warns and the chain CONTINUES. */
            if (!h->required) continue;

            /* A REQUIRED failure ABORTS.  Continuing past a failed 'mount'
             * hook would produce an initramfs that cannot boot. */
            strncpy(c->failed_name, h->name, sizeof(c->failed_name) - 1);
            if (failed_name) *failed_name = c->failed_name;
            return -1;
        }
    }
    return 0;
}
