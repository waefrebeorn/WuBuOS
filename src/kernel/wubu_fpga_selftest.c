/*
 * wubu_fpga_selftest.c -- verifies kernel-owned FPGA routing.
 */
#include "wubu_fpga.h"
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
    printf("=== wubu_fpga_selftest ===\n\n");

    wubu_hw_detect();
    wubu_fpga_probe();

    printf("  present=%d mgr=%d region=%d bridge=%d drv=%s\n",
           wubu_fpga_present(), wubu_fpga_has_mgr(), wubu_fpga_has_region(),
           wubu_fpga_has_bridge(), wubu_fpga_driver() ? wubu_fpga_driver() : "none");

    /* Manager routing is always consistent. */
    CHECK(strcmp(wubu_fpga_mgr_driver("xilinx"), "xilinx-pr-decoupler") == 0,
          "xilinx -> xilinx-pr-decoupler");
    CHECK(strcmp(wubu_fpga_mgr_driver("altera"), "altera-fpga2sdram") == 0,
          "altera -> altera-fpga2sdram");
    CHECK(strcmp(wubu_fpga_mgr_driver("lattice"), "lattice-ecp3") == 0,
          "lattice -> lattice-ecp3");
    CHECK(strcmp(wubu_fpga_mgr_driver("microsemi"), "microsemi-spi") == 0,
          "microsemi -> microsemi-spi");
    CHECK(strcmp(wubu_fpga_mgr_driver("unknown"), "fpga-mgr") == 0,
          "unknown -> fpga-mgr fallback");

    char s[256];
    wubu_fpga_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "fpga summary generated");

    printf("\n=== FPGA TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
