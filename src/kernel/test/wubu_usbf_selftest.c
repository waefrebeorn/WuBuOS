/*
 * wubu_usbf_selftest.c -- verifies kernel-owned USB driver routing.
 */
#include "wubu_usbf.h"
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
    printf("=== wubu_usbf_selftest ===\n\n");

    wubu_hw_detect();
    wubu_usbf_probe();

    printf("  present=%d hcd=%s name=%s xhci=%d ehci=%d usb4=%d usbc=%d otg=%d\n",
           wubu_usbf_present(),
           wubu_usbf_hcd() ? wubu_usbf_hcd() : "(none)",
           wubu_usbf_hcd_name() ? wubu_usbf_hcd_name() : "(none)",
           wubu_usbf_has_xhci(), wubu_usbf_has_ehci(),
           wubu_usbf_has_usb4(), wubu_usbf_has_usbc(), wubu_usbf_has_otg());

    /* On WSL2 no host controller is probed; on bare metal it must be. */
    CHECK(wubu_hw_is_wsl() || wubu_usbf_present(),
          "USB host controller present (or WSL2-host-owned)");
    CHECK(wubu_hw_is_wsl() || wubu_usbf_hcd() != NULL,
          "host controller driver resolved (or WSL2)");

    /* Controller routing is always consistent. */
    CHECK(wubu_usbf_has_xhci() + wubu_usbf_has_ehci() + wubu_usbf_has_ohci()
          + wubu_usbf_has_usb4() <= 1 || wubu_usbf_present(),
          "controller matrix is sane");

    /* Class -> driver routing (always available). */
    CHECK(strcmp(wubu_usbf_class_driver(0x03), "usbhid") == 0,
          "HID class -> usbhid");
    CHECK(strcmp(wubu_usbf_class_driver(0x08), "usb-storage") == 0,
          "mass storage -> usb-storage");
    CHECK(strcmp(wubu_usbf_class_driver(0x01), "snd-usb-audio") == 0,
          "audio class -> snd-usb-audio");
    CHECK(strcmp(wubu_usbf_class_driver(0x0E), "uvcvideo") == 0,
          "video class -> uvcvideo");
    CHECK(strcmp(wubu_usbf_class_driver(0x02), "cdc_ether") == 0,
          "CDC comm -> cdc_ether (network)");

    /* Gadget routing. */
    CHECK(strcmp(wubu_usbf_gadget_driver("uvc"), "g_uvc") == 0,
          "gadget uvc -> g_uvc");
    CHECK(strcmp(wubu_usbf_gadget_driver("mass storage"), "g_mass_storage") == 0,
          "gadget mass storage -> g_mass_storage");
    CHECK(wubu_usbf_gadget_driver("bogus") == NULL,
          "unknown gadget -> NULL");

    char s[256];
    wubu_usbf_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "USB summary generated");

    printf("\n=== USB TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
