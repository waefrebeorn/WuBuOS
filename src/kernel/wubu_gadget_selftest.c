/*
 * wubu_gadget_selftest.c -- verifies kernel-owned gadget/endurance routing.
 */
#include "wubu_gadget.h"
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
    printf("=== wubu_gadget_selftest ===\n\n");

    wubu_hw_detect();
    wubu_gadget_probe();

    printf("  udc=%d cfgfs=%d active=%d nvme=%d smart=%d\n",
           wubu_gadget_udc(), wubu_gadget_configfs(), wubu_gadget_active(),
           wubu_gadget_nvme(), wubu_gadget_smart());

    /* Gadget function routing. */
    CHECK(strcmp(wubu_gadget_function_for("mass"), "mass_storage") == 0,
          "mass -> mass_storage");
    CHECK(strcmp(wubu_gadget_function_for("rndis"), "rndis") == 0,
          "rndis -> rndis");
    CHECK(strcmp(wubu_gadget_function_for("acm"), "acm") == 0,
          "acm -> acm");
    CHECK(strcmp(wubu_gadget_function_for("hid"), "hid") == 0,
          "hid -> hid");
    CHECK(strcmp(wubu_gadget_function_for("uvc"), "uvc") == 0,
          "uvc -> uvc");
    CHECK(strcmp(wubu_gadget_function_for("unknown"), "gadget-core") == 0,
          "unknown -> gadget-core fallback");

    /* NVMe endurance routing. */
    CHECK(strcmp(wubu_gadget_nvme_for("smart"), "smart-log") == 0,
          "smart -> smart-log");
    CHECK(strcmp(wubu_gadget_nvme_for("wear"), "media-wear") == 0,
          "wear -> media-wear");
    CHECK(strcmp(wubu_gadget_nvme_for("tbw"), "tbw") == 0,
          "tbw -> tbw");
    CHECK(strcmp(wubu_gadget_nvme_for("percent"), "pct-used") == 0,
          "percent -> pct-used");
    CHECK(strcmp(wubu_gadget_nvme_for("unknown"), "nvme-health") == 0,
          "unknown -> nvme-health fallback");

    char s[256];
    wubu_gadget_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "gadget summary generated");

    printf("\n=== GADGET TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
