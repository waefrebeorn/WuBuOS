/*
 * wubu_fingerprint_selftest.c -- verifies kernel-owned biometric routing.
 */
#include "wubu_fingerprint.h"
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
    printf("=== wubu_fingerprint_selftest ===\n\n");

    wubu_hw_detect();
    wubu_fingerprint_probe();

    printf("  present=%d goodix=%d vfs=%d egis=%d authenc=%d fpc=%d\n",
           wubu_fingerprint_present(), wubu_fingerprint_goodix(),
           wubu_fingerprint_vfs(), wubu_fingerprint_egis(),
           wubu_fingerprint_authenc(), wubu_fingerprint_fpc());

    /* Vendor routing is always consistent. */
    CHECK(strcmp(wubu_fingerprint_vendor_driver("goodix"), "goodixmoc") == 0,
          "goodix -> goodixmoc");
    CHECK(strcmp(wubu_fingerprint_vendor_driver("synaptics"), "vfs5011") == 0,
          "synaptics -> vfs5011");
    CHECK(strcmp(wubu_fingerprint_vendor_driver("egis"), "egis") == 0,
          "egis -> egis");
    CHECK(strcmp(wubu_fingerprint_vendor_driver("authenc"), "authenc") == 0,
          "authenc -> authenc");
    CHECK(strcmp(wubu_fingerprint_vendor_driver("fpc"), "fpc1020") == 0,
          "fpc -> fpc1020");
    CHECK(strcmp(wubu_fingerprint_vendor_driver("unknown"), "libfprint") == 0,
          "unknown -> libfprint fallback");

    char s[256];
    wubu_fingerprint_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "fingerprint summary generated");

    printf("\n=== FINGERPRINT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
