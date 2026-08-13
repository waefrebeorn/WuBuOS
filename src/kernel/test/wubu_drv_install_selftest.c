/*
 * wubu_drv_install_selftest.c -- verifies the DRIVER SELF-INSTALLATION
 * arm: the AGI-OS installs its own drivers.
 *
 * Pipeline under test (numerate -> decide -> acquire -> build -> load
 * -> re-probe):
 *   1. modalias synthesis for a synthetic PCI device (vendor 0xCAFE)
 *   2. manifest lookup finds the cafe_demo driver
 *   3. wubu_drv_install builds drv_demo.c to an ET_REL .o and LOADS it
 *      in memory via the kernel's own ELF relocatable loader
 *   4. the loaded module registers "cafe_demo", which then binds the
 *      injected 0xCAFE device on wubu_drv_probe
 *   5. an unknown device (no manifest) is reported unbound, and install
 *      returns WUBU_DI_NO_MANIFEST
 *
 * C11. Hosted (uses the host cc for the build leg).
 */
#include "wubu_drv.h"
#include "wubu_drv_install.h"

#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while (0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_drv_install_selftest ===\n\n");

    wubu_drv_init();

    /* 1. MODALIAS synthesis. */
    char alias[64];
    wubu_drv_dev_t cafe;
    memset(&cafe, 0, sizeof(cafe));
    cafe.bus = WUBU_DRV_BUS_PCI;
    cafe.vendor = 0xCAFE;
    cafe.device = 0xBEEF;
    cafe.class_code = 0x03; /* display */
    int n = wubu_drv_modalias(&cafe, alias, sizeof(alias));
    CHECK(n > 0 && strstr(alias, "pci:vCAFE") != NULL,
          "modalias(pci:0xCAFE) synthesizes pci:vCAFE...");
    printf("        modalias = '%s'\n", alias);

    /* 2. DECIDE — manifest lookup. */
    const wubu_drv_manifest_t *m = wubu_drv_manifest_lookup(alias);
    CHECK(m != NULL && strcmp(m->driver_name, "cafe_demo") == 0,
          "manifest has a route for pci:vCAFE -> cafe_demo");

    /* unknown device has no manifest entry */
    wubu_drv_dev_t unknown;
    memset(&unknown, 0, sizeof(unknown));
    unknown.bus = WUBU_DRV_BUS_PCI;
    unknown.vendor = 0xDEAD;
    unknown.device = 0xBEEF;
    char ualias[64];
    wubu_drv_modalias(&unknown, ualias, sizeof(ualias));
    CHECK(wubu_drv_manifest_lookup(ualias) == NULL,
          "unknown pci:0xDEAD has no manifest entry (correctly)");

    /* 3. ACQUIRE + BUILD + LOAD via the full self-install pipeline.
     *    Before: unbound. After: the module loaded & registered, and a
     *    re-probe binds the 0xCAFE device. */
    wubu_drv_dev_t probe_dev = cafe; /* a copy the registry will bind */
    int rc = wubu_drv_install(&probe_dev);
    CHECK(rc == WUBU_DI_OK,
          "wubu_drv_install(cafe) returns OK (module built+loaded)");
    printf("        install rc = %d (%s)\n", rc, rc == 0 ? "OK" : "FAIL");

    /* the loaded module registered cafe_demo -> driver count grew */
    CHECK(wubu_drv_driver_count() >= 19,
          "driver count grew (loaded cafe_demo into the registry)");

    /* 4. RE-PROBE — bind the injected cafe device. */
    wubu_drv_dev_t injected;
    memset(&injected, 0, sizeof(injected));
    injected.bus = WUBU_DRV_BUS_PCI;
    injected.vendor = 0xCAFE;
    injected.device = 0xBEEF;
    wubu_drv_add_device(&injected);
    int probed = wubu_drv_probe();
    const wubu_drv_dev_t *bound = wubu_drv_find("cafe_demo");
    CHECK(bound != NULL && bound->bound,
          "re-probe binds the 0xCAFE device to the self-installed cafe_demo");
    CHECK(probed >= 1, "probe bound at least one device after install");

    /* 5. unknown device -> NO_MANIFEST, not a crash. */
    rc = wubu_drv_install(&unknown);
    CHECK(rc == WUBU_DI_NO_MANIFEST,
          "install(unknown) returns NO_MANIFEST (reports, doesn't crash)");

    /* 6. report leg produces manifest coverage. */
    char rep[1024];
    int rn = wubu_drv_install_report(rep, sizeof(rep));
    CHECK(rn > 0 && strstr(rep, "cafe_demo") != NULL,
          "install_report lists manifest coverage including cafe_demo");

    printf("\n=== wubu_drv_install_selftest: %d passed, %d failed ===\n",
           passed, failures);
    return failures ? 1 : 0;
}
