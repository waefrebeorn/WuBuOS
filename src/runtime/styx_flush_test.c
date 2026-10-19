/* styx_flush_test.c -- Tflush cancels a generator blocked mid-Tread. */
#include "styx_flush.h"
#include <stdio.h>

static int pass, fail;
#define T(n) do { printf("  TEST %-52s", n); } while (0)
#define OK() do { printf("✅\n"); pass++; } while (0)
#define NO(m) do { printf("❌ %s\n", m); fail++; } while (0)

/* A synthetic generator: produces bytes until cancelled, with a hard cap so a
 * broken cancel shows up as a finish rather than a hang. */
static int generator(styx_flushtab_t *tab, uint16_t tag, int cap)
{
    int produced = 0;
    while (produced < cap) {
        if (styx_flush_is_cancelled(tab, tag)) return -1;   /* aborted */
        produced++;
    }
    return produced;
}

int main(void)
{
    styx_flushtab_t t;
    styx_flush_init(&t);

    T("a request in flight can be cancelled by Tflush");
    int mid = 0;
    if (styx_flush_begin(&t, 5) != 0) NO("begin failed");
    else {
        /* cancel "mid-read", as a second connection would */
        if (styx_flush_cancel(&t, 5) != 0) NO("cancel failed");
        else if (generator(&t, 5, 1000) == -1) { mid = 1; OK(); }
        else NO("generator ran to completion despite the flush");
    }

    T("the cancelled request frees its tag");
    if (styx_flush_end(&t, 5) == 0 && styx_flush_in_flight(&t) == 0) OK();
    else NO("slot not freed");

    T("the tag can be reused after the flush");
    if (styx_flush_begin(&t, 5) == 0 && !styx_flush_is_cancelled(&t, 5)) OK();
    else NO("reuse carried the old cancellation");

    T("a duplicate in-flight tag is refused");
    if (styx_flush_begin(&t, 5) == -1) OK(); else NO("duplicate tag accepted");

    T("an uncancelled generator completes");
    styx_flush_end(&t, 5);
    styx_flush_begin(&t, 6);
    if (generator(&t, 6, 50) == 50) OK(); else NO("clean generator aborted");

    T("flushing an unknown tag reports failure");
    if (styx_flush_cancel(&t, 999) == -1) OK(); else NO("cancelled a phantom request");

    printf("\n=== styx_flush: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
