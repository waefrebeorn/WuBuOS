/*
 * test_tandem.c -- the Tandem headless test: the user+AGI loop.
 * Exercises the tandem timing loop (tick -> propose -> yield) with
 * the psychology engines. No GUI needed: the launch path is stubbed
 * to build the state directly.
 */
#include <stdio.h>
#include <string.h>
#include "../kernel/wubu_psych.h"
#include "../kernel/wubu_bonzi_study.h"
#include "../apps/tandem/tandem.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_tandem (user+AGI shared desktop loop) ===\n");

    /* the loop: the human drives -> the AGI watches -> the AGI proposes
     * after the patience window -> the human accepts. */
    wubu_psych_user_t u;
    wubu_psych_attention_t attn;
    wubu_psych_init(&u);
    memset(&attn, 0, sizeof(attn));

    /* the human takes the controls */
    wubu_psych_input_seen(&attn, 0);
    CHECK(attn.human_driving == 1, "human drives at t=0");

    /* the AGI must NOT act during the human's active input */
    CHECK(wubu_psych_may_act(&attn, 100, 500) == 0, "no AGI act during human");

    /* the patience window: after the human idles past patience, the
     * AGI may propose */
    uint32_t patience_left = wubu_psych_patience_left(&attn, u.patience_ms + 1,
                                                      u.patience_ms);
    CHECK(patience_left == 0, "patience expired");

    /* the companion proposes */
    wubu_psych_proposal_t prop;
    strcpy(prop.title, "compact the cache?");
    strcpy(prop.body, "the cache is 80% full");
    prop.proposal_id = 1;
    CHECK(wubu_psych_propose(&attn, &prop, 2) == 0, "propose");
    CHECK(attn.pending_proposal == 1, "proposal pending");

    /* the human accepts -> the loop continues */
    CHECK(wubu_psych_accept(&attn, 1) == 0, "accept");
    CHECK(attn.pending_proposal == 0, "cleared");

    /* the companion tick drives the actions */
    uint32_t action = 9;
    CHECK(wubu_bs_tick(&u, 100, &action) == 0 && action == 0, "tick quiet");
    CHECK(wubu_bs_tick(&u, 6000, &action) == 0 && action == 1, "tick yield");

    /* the study ledger */
    uint32_t log[8];
    int n = 0;
    for (int i = 0; i < 4; i++)
        wubu_bs_log_interaction(log, 8, &n, 1, 1);
    float rate = wubu_bs_success_rate(log, n, 1);
    CHECK(rate == 1.0f, "study success rate");

    if (failures == 0) printf("ALL TANDEM TESTS PASSED\n");
    else printf("%d TANDEM FAILURES\n", failures);
    return failures ? 1 : 0;
}
