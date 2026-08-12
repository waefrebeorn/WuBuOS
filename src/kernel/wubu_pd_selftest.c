/*
 * wubu_pd_selftest.c -- verifies kernel-owned USB-PD/flow-steering routing.
 */
#include "wubu_pd.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_pd_selftest ===\n\n");

    wubu_hw_detect();
    wubu_pd_probe();

    printf("  typec=%d pd=%d tcpm=%d rfs=%d arfs=%d\n",
           wubu_pd_typec(), wubu_pd_supported(), wubu_pd_tcpm(),
           wubu_pd_rfs(), wubu_pd_arfs());

    /* PD contract routing. */
    CHECK(strcmp(wubu_pd_contract_for("source"), "source") == 0,
          "source -> source");
    CHECK(strcmp(wubu_pd_contract_for("sink"), "sink") == 0,
          "sink -> sink");
    CHECK(strcmp(wubu_pd_contract_for("dual"), "dual-role") == 0,
          "dual -> dual-role");
    CHECK(strcmp(wubu_pd_contract_for("unknown"), "pd") == 0,
          "unknown -> pd fallback");

    /* Flow steering routing. */
    CHECK(strcmp(wubu_pd_flow_for("ixgbe"), "ixgbe-arfs") == 0,
          "ixgbe -> ixgbe-arfs");
    CHECK(strcmp(wubu_pd_flow_for("i40e"), "i40e-arfs") == 0,
          "i40e -> i40e-arfs");
    CHECK(strcmp(wubu_pd_flow_for("mlx5"), "mlx5-arfs") == 0,
          "mlx5 -> mlx5-arfs");
    CHECK(strcmp(wubu_pd_flow_for("unknown"), "rfs") == 0,
          "unknown -> rfs fallback");

    char s[256];
    wubu_pd_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "pd summary generated");

    printf("\n=== PD TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
