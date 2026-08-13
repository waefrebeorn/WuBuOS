/*
 * wubu_sata_selftest.c -- verifies kernel-owned SATA/NCQ routing.
 */
#include "wubu_sata.h"
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
    printf("=== wubu_sata_selftest ===\n\n");

    wubu_hw_detect();
    wubu_sata_probe();

    printf("  present=%d ahci=%d ncq=%d hotplug=%d pmp=%d smart=%d\n",
           wubu_sata_present(), wubu_sata_has_ahci(), wubu_sata_has_ncq(),
           wubu_sata_has_hotplug(), wubu_sata_has_pmp(), wubu_sata_has_smart());

    /* Controller routing is always consistent. */
    CHECK(strcmp(wubu_sata_controller_driver("ahci"), "ahci") == 0,
          "ahci -> ahci");
    CHECK(strcmp(wubu_sata_controller_driver("sata_pmp"), "sata_pmp") == 0,
          "sata_pmp -> sata_pmp");
    CHECK(strcmp(wubu_sata_controller_driver("nvme"), "nvme") == 0,
          "nvme -> nvme");
    CHECK(strcmp(wubu_sata_controller_driver("usb-storage"), "usb-storage") == 0,
          "usb-storage -> usb-storage");
    CHECK(strcmp(wubu_sata_controller_driver("ide"), "ata_piix") == 0,
          "ide -> ata_piix");
    CHECK(strcmp(wubu_sata_controller_driver("unknown"), "libata") == 0,
          "unknown -> libata fallback");

    /* NCQ implies AHCI. */
    CHECK(!wubu_sata_has_ncq() || wubu_sata_has_ahci(),
          "NCQ implies AHCI");

    char s[256];
    wubu_sata_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "sata summary generated");

    printf("\n=== SATA TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
