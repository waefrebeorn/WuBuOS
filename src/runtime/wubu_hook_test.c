/*
 * wubu_hook_test.c -- the mkinitcpio hook chain.
 *
 * The load-bearing rules: stage order is FIXED (a hook registered later never
 * runs earlier than one in an earlier stage), a NON-EXECUTABLE hook is skipped
 * rather than run, an OPTIONAL failure continues, and a REQUIRED failure aborts
 * the chain with no later hook running.
 */
#include "wubu_hook.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

/* Record the run order, and fail the hooks whose names are in `fail`. */
typedef struct {
    char order[WUBU_HOOK_MAX][64];
    int  n;
    const char *fail;      /* comma-separated names that should fail */
} recorder_t;

static bool rec_fn(const wubu_hook_t *h, void *user)
{
    recorder_t *r = (recorder_t *)user;
    if (r && r->n < WUBU_HOOK_MAX) {
        strncpy(r->order[r->n], h->name, 63);
        r->order[r->n][63] = '\0';
        r->n++;
    }
    if (!r || !r->fail) return true;
    /* does the name appear in the comma-separated fail list? */
    size_t nl = strlen(h->name);
    for (const char *p = r->fail; *p; ) {
        const char *comma = strchr(p, ',');
        size_t l = comma ? (size_t)(comma - p) : strlen(p);
        if (l == nl && strncmp(p, h->name, l) == 0) return false;
        if (!comma) break;
        p = comma + 1;
    }
    return true;
}

int main(void)
{
    printf("=== test_wubu_hook ===\n");
    wubu_hook_chain_t c;
    recorder_t rec;
    const char *failed = NULL;

    wubu_hook_chain_init(&c);
    CHECK(c.count == 0, "the chain starts empty");

    /* -- stage order beats registration order -- */
    CHECK(wubu_hook_add(&c, "z-late", WUBU_HOOK_LATE, true, true) == 0,
          "a late hook is registered first");
    CHECK(wubu_hook_add(&c, "a-early", WUBU_HOOK_EARLY, true, true) == 0,
          "an early hook is registered second");
    CHECK(wubu_hook_add(&c, "m-mount", WUBU_HOOK_MOUNT, true, true) == 0,
          "a mount hook third");
    CHECK(c.count == 3, "three hooks");

    memset(&rec, 0, sizeof(rec));
    CHECK(wubu_hook_run(&c, rec_fn, &rec, &failed) == 0, "the chain runs");
    CHECK(failed == NULL, "nothing failed");
    CHECK(rec.n == 3, "all three ran");
    CHECK(strcmp(rec.order[0], "a-early") == 0,
          "the EARLY hook runs first despite being registered second");
    CHECK(strcmp(rec.order[1], "m-mount") == 0, "then mount");
    CHECK(strcmp(rec.order[2], "z-late") == 0, "then late");

    /* -- registration order within a stage -- */
    wubu_hook_chain_reset(&c);
    CHECK(wubu_hook_count(&c) == 3, "a reset keeps the hooks");
    CHECK(wubu_hook_add(&c, "pre1", WUBU_HOOK_PRE_MOUNT, true, true) == 0, "pre1");
    CHECK(wubu_hook_add(&c, "pre2", WUBU_HOOK_PRE_MOUNT, true, true) == 0, "pre2");
    CHECK(wubu_hook_add(&c, "pre3", WUBU_HOOK_PRE_MOUNT, true, true) == 0, "pre3");
    memset(&rec, 0, sizeof(rec));
    CHECK(wubu_hook_run(&c, rec_fn, &rec, &failed) == 0, "the chain runs again");
    /* find pre1/pre2/pre3 relative order */
    int p1 = -1, p2 = -1, p3 = -1;
    for (int i = 0; i < rec.n; i++) {
        if (strcmp(rec.order[i], "pre1") == 0) p1 = i;
        if (strcmp(rec.order[i], "pre2") == 0) p2 = i;
        if (strcmp(rec.order[i], "pre3") == 0) p3 = i;
    }
    CHECK(p1 >= 0 && p2 >= 0 && p3 >= 0 && p1 < p2 && p2 < p3,
          "hooks in the SAME stage keep registration order");

    /* -- a non-executable hook is SKIPPED, not run -- */
    {
        wubu_hook_chain_t k;
        wubu_hook_chain_init(&k);
        wubu_hook_add(&k, "real", WUBU_HOOK_EARLY, true, true);
        wubu_hook_add(&k, "notexec", WUBU_HOOK_MOUNT, false, true);
        wubu_hook_add(&k, "after", WUBU_HOOK_LATE, true, true);
        memset(&rec, 0, sizeof(rec));
        CHECK(wubu_hook_run(&k, rec_fn, &rec, &failed) == 0,
              "a non-executable hook does not fail the chain");
        CHECK(rec.n == 2, "and is not run");
        CHECK(k.hooks[1].skipped && !k.hooks[1].failed,
              "it is recorded as SKIPPED, not as a failure");
    }

    /* -- an OPTIONAL failure continues -- */
    {
        wubu_hook_chain_t k;
        wubu_hook_chain_init(&k);
        wubu_hook_add(&k, "bad-optional", WUBU_HOOK_EARLY, true, false);
        wubu_hook_add(&k, "good-after", WUBU_HOOK_LATE, true, true);
        memset(&rec, 0, sizeof(rec));
        rec.fail = "bad-optional";
        CHECK(wubu_hook_run(&k, rec_fn, &rec, &failed) == 0,
              "an OPTIONAL failure does not abort");
        CHECK(failed == NULL, "and nothing is reported as the abort");
        CHECK(rec.n == 2, "the later hook still runs");
        CHECK(k.hooks[0].failed, "the failure is recorded");
    }

    /* -- a REQUIRED failure aborts, and NO later hook runs -- */
    {
        wubu_hook_chain_t k;
        wubu_hook_chain_init(&k);
        wubu_hook_add(&k, "good-first", WUBU_HOOK_EARLY, true, true);
        wubu_hook_add(&k, "bad-required", WUBU_HOOK_MOUNT, true, true);
        wubu_hook_add(&k, "never-runs", WUBU_HOOK_LATE, true, true);
        memset(&rec, 0, sizeof(rec));
        rec.fail = "bad-required";
        CHECK(wubu_hook_run(&k, rec_fn, &rec, &failed) == -1,
              "a REQUIRED failure aborts the chain");
        CHECK(failed && strcmp(failed, "bad-required") == 0,
              "and the abort is reported BY NAME");
        CHECK(rec.n == 2, "the hook before it ran, and the failing one");
        CHECK(!k.hooks[2].ran,
              "and NOTHING after the failure ran (a half-built initramfs would "
              "be worse than none)");
    }

    /* -- duplicates and bounds -- */
    {
        wubu_hook_chain_t k;
        wubu_hook_chain_init(&k);
        CHECK(wubu_hook_add(&k, "dup", WUBU_HOOK_EARLY, true, true) == 0, "added");
        CHECK(wubu_hook_add(&k, "dup", WUBU_HOOK_LATE, true, true) == -1,
              "a DUPLICATE name is refused (it would run the side effects twice)");
        CHECK(k.count == 1, "and did not replace the first");
        CHECK(wubu_hook_add(&k, NULL, WUBU_HOOK_EARLY, true, true) == -1,
              "a null name is refused");
        CHECK(wubu_hook_add(&k, "x", (wubu_hook_stage_t)99, true, true) == -1,
              "an unknown stage is refused");
    }

    /* -- a chain cannot be run twice without a reset -- */
    {
        wubu_hook_chain_t k;
        wubu_hook_chain_init(&k);
        wubu_hook_add(&k, "once", WUBU_HOOK_EARLY, true, true);
        CHECK(wubu_hook_run(&k, rec_fn, NULL, &failed) == 0, "the first run");
        CHECK(wubu_hook_run(&k, rec_fn, NULL, &failed) == -1,
              "a SECOND run without a reset is refused");
        wubu_hook_chain_reset(&k);
        CHECK(wubu_hook_run(&k, rec_fn, NULL, &failed) == 0,
              "and a reset allows it again");
    }

    /* -- stage names -- */
    CHECK(strcmp(wubu_hook_stage_name(WUBU_HOOK_PRE_MOUNT), "pre-mount") == 0,
          "the stages have readable names");
    CHECK(strcmp(wubu_hook_stage_name((wubu_hook_stage_t)99), "unknown") == 0,
          "and an unknown stage has a name");

    /* -- null handling -- */
    wubu_hook_chain_init(NULL);
    CHECK(wubu_hook_count(NULL) == 0, "a null chain has no hooks");
    CHECK(wubu_hook_add(NULL, "x", WUBU_HOOK_EARLY, true, true) == -1,
          "a null chain cannot add");
    CHECK(wubu_hook_run(NULL, rec_fn, NULL, &failed) == -1, "a null chain cannot run");
    wubu_hook_chain_reset(NULL);

    if (failures == 0) printf("ALL WUBU_HOOK TESTS PASSED\n");
    else printf("%d WUBU_HOOK FAILURES\n", failures);
    return failures ? 1 : 0;
}
