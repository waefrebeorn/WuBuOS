/*
 * wubu_securekey_selftest.c -- verifies kernel-owned security routing.
 */
#include "wubu_securekey.h"
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
    printf("=== wubu_securekey_selftest ===\n\n");

    wubu_hw_detect();
    wubu_securekey_probe();

    printf("  present=%d fido=%d ccid=%d tpm=%d drv=%s\n",
           wubu_securekey_present(), wubu_securekey_fido(),
           wubu_securekey_ccid(), wubu_securekey_tpm(),
           wubu_securekey_driver() ? wubu_securekey_driver() : "none");

    /* Driver routing is always consistent. */
    CHECK(strcmp(wubu_securekey_driver_for("u2f"), "hid-fido2") == 0,
          "u2f -> hid-fido2");
    CHECK(strcmp(wubu_securekey_driver_for("fido"), "hid-fido2") == 0,
          "fido -> hid-fido2");
    CHECK(strcmp(wubu_securekey_driver_for("ccid"), "ccid") == 0,
          "ccid -> ccid");
    CHECK(strcmp(wubu_securekey_driver_for("tpm_tis"), "tpm_tis") == 0,
          "tpm_tis -> tpm_tis");
    CHECK(strcmp(wubu_securekey_driver_for("tpm_crb"), "tpm_crb") == 0,
          "tpm_crb -> tpm_crb");
    CHECK(strcmp(wubu_securekey_driver_for("unknown"), "security-core") == 0,
          "unknown -> security-core fallback");

    /* Present iff any security device. */
    CHECK(wubu_securekey_present() == (wubu_securekey_fido() || wubu_securekey_ccid() || wubu_securekey_tpm()),
          "present == (any security device)");

    char s[256];
    wubu_securekey_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "security summary generated");

    printf("\n=== SECUREKEY TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
