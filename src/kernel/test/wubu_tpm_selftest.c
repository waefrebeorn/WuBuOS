/*
 * wubu_tpm_selftest.c -- verifies kernel-owned TPM routing.
 */
#include "wubu_tpm.h"
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
    printf("=== wubu_tpm_selftest ===\n\n");

    wubu_hw_detect();
    wubu_tpm_probe();

    printf("  tpm=%d tpm2=%d tss=%d crb=%d measured=%d\n",
           wubu_tpm_present(), wubu_tpm_is_tpm2(), wubu_tpm_has_tss(),
           wubu_tpm_has_crb(), wubu_tpm_has_measured_boot());

    /* Driver routing is always consistent. */
    CHECK(strcmp(wubu_tpm_driver_for("crb"), "tpm_crb") == 0,
          "crb -> tpm_crb");
    CHECK(strcmp(wubu_tpm_driver_for("tis"), "tpm_tis") == 0,
          "tis -> tpm_tis");
    CHECK(strcmp(wubu_tpm_driver_for("fifo"), "tpm_tis") == 0,
          "fifo -> tpm_tis");
    CHECK(strcmp(wubu_tpm_driver_for("spi"), "tpm_tis_spi") == 0,
          "spi -> tpm_tis_spi");
    CHECK(strcmp(wubu_tpm_driver_for("i2c"), "tpm_i2c_atmel") == 0,
          "i2c -> tpm_i2c_atmel");
    CHECK(strcmp(wubu_tpm_driver_for("unknown"), "tpm_tis") == 0,
          "unknown -> tpm_tis fallback");

    /* TPM 2.0 implies present. */
    CHECK(!wubu_tpm_is_tpm2() || wubu_tpm_present(),
          "TPM2 implies present");

    char s[256];
    wubu_tpm_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "tpm summary generated");

    printf("\n=== TPM TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
