/*
 * wubu_virt_selftest.c -- verifies kernel-owned virtualization routing.
 */
#include "wubu_virt.h"
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
    printf("=== wubu_virt_selftest ===\n\n");

    wubu_hw_detect();
    wubu_virt_probe();

    printf("  hyper=%s pv=%s virtio=%d\n",
           wubu_virt_hypervisor_name() ? wubu_virt_hypervisor_name() : "bare-metal",
           wubu_virt_pv_driver() ? wubu_virt_pv_driver() : "-",
           wubu_virt_has_virtio());

    /* Hypervisor is detected or bare-metal. */
    CHECK(wubu_virt_hypervisor() >= 0, "hypervisor status resolved");

    /* PV driver-set routing is always consistent. */
    CHECK(wubu_virt_driver_set(1) != NULL, "KVM driver set present");
    CHECK(strstr(wubu_virt_driver_set(2), "hv_vmbus") != NULL,
          "Hyper-V driver set has hv_vmbus");
    CHECK(strstr(wubu_virt_driver_set(3), "vmxnet3") != NULL,
          "VMware driver set has vmxnet3");
    CHECK(strstr(wubu_virt_driver_set(4), "xen-blkfront") != NULL,
          "Xen driver set has xen-blkfront");
    CHECK(wubu_virt_driver_set(0) == NULL, "no driver set for bare-metal");

    /* If we detected a hypervisor, the PV driver is set. */
    CHECK(wubu_virt_hypervisor() == 0 || wubu_virt_pv_driver() != NULL,
          "detected hypervisor -> PV driver present");

    char s[256];
    wubu_virt_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "virt summary generated");

    printf("\n=== VIRT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
