/*
 * wubu_nvidia_fermi_selftest.c -- verifies NVIDIA Fermi legacy routing.
 */
#include "wubu_nvidia_fermi.h"
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
    printf("=== wubu_nvidia_fermi_selftest ===\n");

    wubu_nvidia_fermi_probe();

    int p = wubu_nvidia_fermi_present();
    CHECK(p == 0 || p == 1, "nvidia_fermi present is boolean");

    /* Legacy need. */
    CHECK(wubu_nvidia_fermi_needs_legacy(1) == 1, "Fermi = needs legacy");
    CHECK(wubu_nvidia_fermi_needs_legacy(0) == 0, "non-Fermi = no legacy");

    /* EOL status. */
    CHECK(wubu_nvidia_fermi_eol_status() == 470, "470.xx EOL legacy");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvidia_fermi_summary(out, sizeof(out));
    CHECK(strstr(out, "nvidia_fermi[") != NULL, "summary has nvidia_fermi fragment");

    printf("\n=== NVIDIA_FERMI TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
