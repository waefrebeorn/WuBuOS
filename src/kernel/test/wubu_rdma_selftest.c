/*
 * wubu_rdma_selftest.c -- verifies kernel-owned RDMA routing.
 */
#include "wubu_rdma.h"
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
    printf("=== wubu_rdma_selftest ===\n\n");

    wubu_hw_detect();
    wubu_rdma_probe();

    printf("  rdma=%d ib=%d roce=%d iwarp=%d soft_roce=%d ports=%d\n",
           wubu_rdma_present(), wubu_rdma_ib(), wubu_rdma_roce(),
           wubu_rdma_iwarp(), wubu_rdma_soft_roce(), wubu_rdma_ports());

    /* RDMA driver routing is always consistent. */
    CHECK(strcmp(wubu_rdma_driver_for("mlx5"), "mlx5_ib") == 0,
          "mlx5 -> mlx5_ib");
    CHECK(strcmp(wubu_rdma_driver_for("connectx"), "mlx5_ib") == 0,
          "connectx -> mlx5_ib");
    CHECK(strcmp(wubu_rdma_driver_for("irdma"), "irdma") == 0,
          "irdma -> irdma");
    CHECK(strcmp(wubu_rdma_driver_for("bnxt"), "bnxt_re") == 0,
          "bnxt -> bnxt_re");
    CHECK(strcmp(wubu_rdma_driver_for("qlogic"), "qedr") == 0,
          "qlogic -> qedr");
    CHECK(strcmp(wubu_rdma_driver_for("rxe"), "rdma_rxe") == 0,
          "rxe -> rdma_rxe (soft-roce)");
    CHECK(strcmp(wubu_rdma_driver_for("siw"), "siw") == 0,
          "siw -> siw (soft-iwarp)");
    CHECK(strcmp(wubu_rdma_driver_for("unknown"), "rdma-core") == 0,
          "unknown -> rdma-core fallback");

    char s[256];
    wubu_rdma_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "rdma summary generated");

    printf("\n=== RDMA TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
