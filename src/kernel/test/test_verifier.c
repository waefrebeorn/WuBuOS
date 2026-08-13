/* test_verifier.c -- host tests for the DA-3 independent verifier.
 * Builds wubu_verifier.c with a minimal shim (no AGI kernel needed:
 * only wubu_verifier_score is exercised directly; install() requires
 * the AGI kernel so it is skipped here). */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "wubu_verifier.h"
#include "wubu_verifier.c"

/* shims for install()'s AGI-kernel references (not exercised here) */
typedef struct wubu_agi_kernel wubu_agi_kernel_t;
typedef float (*wubu_agi_verifier_fn)(const char *, uint64_t, void *, bool *);
static wubu_agi_kernel_t *g_k;

/* G1: the runtime-PCR gate -- a live non-zero chain on the host */
int wubu_attest_runtime_pcr(uint8_t out[32])
{
    if (!out) return -1;
    for (int i = 0; i < 32; i++) out[i] = (uint8_t)(0x5A + i);
    return 0;
}

/* G2: the self-test gate -- all checks pass on the host */
uint32_t wubu_self_test_run(uint32_t *total)
{
    if (total) *total = 5;
    return 5;
}
wubu_agi_kernel_t *wubu_agi_kernel_global(void) { return g_k; }
void wubu_agi_kernel_set_verifier(wubu_agi_kernel_t *k,
                                  wubu_agi_verifier_fn fn, void *ud)
{ (void)k; (void)fn; (void)ud; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

int main(void)
{
    bool passed = true;

    /* well-formed + trusted emitter + structure -> PASS */
    float s = wubu_verifier_score(
        "bonzi.heartbeat regions=34 attest=VALID promoted=0 frozen=no", 0, NULL, &passed);
    CHECK(passed && s >= WUBU_VERIFIER_THRESHOLD);

    /* trusted + kv structure -> PASS */
    s = wubu_verifier_score("agent.step=1 regions=6 viewport=1920x1080", 0, NULL, &passed);
    CHECK(passed);

    /* garbage / non-printable -> REJECT */
    s = wubu_verifier_score("bonzi.\x01\x02\x03", 0, NULL, &passed);
    CHECK(!passed && s == 0.0f);

    /* unknown emitter -> REJECT */
    s = wubu_verifier_score("evil.inject regions=1", 0, NULL, &passed);
    CHECK(!passed && s == 0.0f);

    /* noise (too short) -> REJECT */
    s = wubu_verifier_score("a", 0, NULL, &passed);
    CHECK(!passed && s == 0.0f);

    /* NULL -> REJECT */
    s = wubu_verifier_score(NULL, 0, NULL, &passed);
    CHECK(!passed && s == 0.0f);

    /* overlong -> REJECT */
    char big[300];
    for (int i = 0; i < 299; i++) big[i] = 'a';
    big[299] = 0;
    s = wubu_verifier_score(big, 0, NULL, &passed);
    CHECK(!passed && s == 0.0f);

    if (failures == 0) printf("test_verifier: ALL PASS\n");
    else printf("test_verifier: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
