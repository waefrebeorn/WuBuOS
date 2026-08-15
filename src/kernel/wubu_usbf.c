/*
 * wubu_usb.c -- kernel-owned USB driver routing + power management.
 *
 * USB is how EVERYTHING plugs in: controllers (gamepads, wheels, arcade),
 * storage (flash, card readers, SATA bridges), audio (headsets, DACs,
 * sound dongles), video (webcams, capture), network (Wi-Fi/BT/LAN dongles),
 * printers, hubs, docks. On Linux the host-controller driver is chosen by
 * the PCI class (EHCI=0x0C03, OHCI=0x0C10, xHCI=0x0C03/0x0C30, USB4=0x0C0340),
 * and the per-device driver is chosen by USB class code. The two most
 * common user headaches:
 *   - USB autosuspend kills gamepads/audio (adds wake latency)
 *   - USB-C alt-mode (DisplayPort/Thunderbolt) docking is flaky
 *
 * WuBuOS owns all of this: detect the host controller, route devices to the
 * right class driver, disable autosuspend on latency-critical classes, and
 * expose the full USB topology for the Brain to observe.
 *
 * Research (Kevin-Bacon 7-hop on the USB frontier):
 *   - HCI matrix: OHCI (USB1) / EHCI (USB2) / xHCI (USB3, supersets all) /
 *     USB4/Thunderbolt (USB4 over PCIe, thunderbolt.ko)
 *   - class drivers: usbhid, uvcvideo, snd-usb-audio, usb-storage, usbnet,
 *     cdc-acm (serial), usblp (printer), usbhid for boot HID
 *   - USB-C/Type-C: typec bus, typec_switch, DP alt (DP over USB-C),
 *     Thunderbolt alt (TBT), USB PD (power delivery)
 *   - power: usbcore.autosuspend, module params for latency classes
 *   - gadget (device mode): configfs function drivers
 */
#include "wubu_usbf.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

/* ---- PCI classes: USB host controllers ---- */
#define PCI_CLASS_SERIAL       0x0C
#define PCI_SUBCLASS_USB       0x03   /* USB host controller */
#define PCI_PROG_XHCI          0x30   /* xHCI (USB3) */
#define PCI_PROG_UHCI          0x00
#define PCI_PROG_OHCI          0x10
#define PCI_PROG_EHCI          0x20
#define PCI_PROG_USB4          0x40   /* USB4/Thunderbolt */
#define PCI_SUBCLASS_SERIALBUS 0x06   /* serial bus controller (SMbus/USB4) */
#define PCI_VENDOR_INTEL       0x8086
#define PCI_VENDOR_AMD         0x1022
#define PCI_VENDOR_RENESAS     0x1912
#define PCI_VENDOR_ASMEDIA     0x1B21
#define PCI_VENDOR_VIA         0x1106
#define PCI_VENDOR_ETRON       0x1A0A
#define PCI_VENDOR_REDHAT      0x1AF4  /* virtio */

/* ---- USB class codes (bInterfaceClass) ---- */
#define USB_CLASS_HID          0x03
#define USB_CLASS_MASS_STORAGE 0x08
#define USB_CLASS_AUDIO        0x01
#define USB_CLASS_VIDEO        0x0E
#define USB_CLASS_COMM         0x02   /* CDC comm (network/serial) */
#define USB_CLASS_HUB          0x09
#define USB_CLASS_PRINTER      0x07
#define USB_CLASS_MISC         0xEF   /* IAD / composite */
#define USB_CLASS_MIDI         0x01   /* audio streaming, subclass 3 */

/* ---- Global state ---- */
static int  g_usb_present = 0;
static int  g_xhci = 0;          /* xHCI (USB3) controller present */
static int  g_ehci = 0;          /* EHCI (USB2) */
static int  g_ohci = 0;          /* OHCI (USB1) */
static int  g_usb4 = 0;          /* USB4/Thunderbolt */
static int  g_usbc = 0;          /* USB-C connector */
static int  g_otg = 0;           /* OTG dual-role */
static int  g_hid = 0;           /* HID (gamepad/keyboard/mouse) present */
static int  g_msc = 0;           /* mass storage present */
static int  g_uaudio = 0;        /* USB audio present */
static int  g_uvc = 0;           /* USB video present */
static int  g_unet = 0;          /* USB network present */
static int  g_serial = 0;        /* USB serial present */
static int  g_autosuspend = 1;   /* autosuspend on (1) or disabled (0) */
static int  g_host_ctl_vendor = 0;
static char g_hcd[32] = "";      /* host controller driver name */
static char g_hcd_name[64] = ""; /* human-readable controller */

/* ---- W1: probe the USB topology ---- */
void wubu_usbf_probe(void)
{
    g_usb_present = 0; g_xhci = 0; g_ehci = 0; g_ohci = 0;
    g_usb4 = 0; g_usbc = 0; g_otg = 0;
    g_hid = 0; g_msc = 0; g_uaudio = 0; g_uvc = 0;
    g_unet = 0; g_serial = 0;
    g_host_ctl_vendor = 0;
    g_hcd[0] = '\0'; g_hcd_name[0] = '\0';

    /* WSL2: host owns USB (usbipd may attach, but default no PCI). */
    if (wubu_hw_is_wsl()) return;

#ifdef WUBU_HOSTED
    /* Bare metal: scan PCI for USB host controllers. */
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    for (int i = 0; i < n; i++) {
        if ((devs[i].class_code >> 8) == PCI_CLASS_SERIAL &&
            devs[i].subclass == PCI_SUBCLASS_USB) {
            g_usb_present = 1;
            g_host_ctl_vendor = devs[i].vendor;
            switch (devs[i].prog_if) {
            case PCI_PROG_XHCI:
                g_xhci = 1;
                strcpy(g_hcd, "xhci_hcd");
                strcpy(g_hcd_name, "xHCI (USB 3.x)");
                break;
            case PCI_PROG_EHCI:
                g_ehci = 1;
                strcpy(g_hcd, "ehci_hcd");
                strcpy(g_hcd_name, "EHCI (USB 2.0)");
                break;
            case PCI_PROG_OHCI:
            case PCI_PROG_UHCI:
                g_ohci = 1;
                strcpy(g_hcd, "ohci_hcd");
                strcpy(g_hcd_name, "OHCI (USB 1.1)");
                break;
            case PCI_PROG_USB4:
                g_usb4 = 1;
                strcpy(g_hcd, "thunderbolt");
                strcpy(g_hcd_name, "USB4/Thunderbolt");
                break;
            default:
                strcpy(g_hcd, "xhci_hcd");
                strcpy(g_hcd_name, "USB host controller");
                break;
            }
            break;  /* primary controller */
        }
        /* USB-C / Thunderbolt on the serial-bus class (0x0C/0x06). */
        if ((devs[i].class_code >> 8) == PCI_CLASS_SERIAL &&
            devs[i].subclass == PCI_SUBCLASS_SERIALBUS) {
            g_usbc = 1;  /* USB-C / Type-C connector controller */
        }
    }

    /* USB-C dual-role is exposed via the Type-C subsystem even without
     * a separate PCI ID; a device with USB-C port (virtually all modern
     * laptops) has OTG capability. */
    g_otg = g_usbc;
#endif
}

/* ---- W2: accessors ---- */
int  wubu_usbf_present(void)      { return g_usb_present; }
int  wubu_usbf_has_xhci(void)     { return g_xhci; }
int  wubu_usbf_has_ehci(void)     { return g_ehci; }
int  wubu_usbf_has_ohci(void)     { return g_ohci; }
int  wubu_usbf_has_usb4(void)     { return g_usb4; }
int  wubu_usbf_has_usbc(void)     { return g_usbc; }
int  wubu_usbf_has_otg(void)      { return g_otg; }
int  wubu_usbf_has_hid(void)      { return g_hid; }
int  wubu_usbf_has_storage(void)  { return g_msc; }
int  wubu_usbf_has_audio(void)    { return g_uaudio; }
int  wubu_usbf_has_video(void)    { return g_uvc; }
int  wubu_usbf_has_network(void)  { return g_unet; }
int  wubu_usbf_has_serial(void)   { return g_serial; }
const char *wubu_usbf_hcd(void)   { return g_hcd[0] ? g_hcd : NULL; }
const char *wubu_usbf_hcd_name(void) { return g_hcd_name[0] ? g_hcd_name : NULL; }
int  wubu_usbf_host_vendor(void)  { return g_host_ctl_vendor; }

/* ---- W3: route a USB device class to its driver ----
 * The class-code -> module mapping, so the kernel can autoload the right
 * driver for a connected device. Callable per enumerated device. */
const char *wubu_usbf_class_driver(int cls)
{
    switch (cls) {
    case USB_CLASS_HID:          return "usbhid";        /* gamepad/kb/mouse */
    case USB_CLASS_MASS_STORAGE: return "usb-storage";   /* flash/card/SSD */
    case USB_CLASS_AUDIO:        return "snd-usb-audio"; /* headset/DAC/mic */
    case USB_CLASS_VIDEO:        return "uvcvideo";      /* webcam/capture */
    case USB_CLASS_COMM:         return "cdc_ether";     /* RNDIS/NCM/CDC net */
    case USB_CLASS_HUB:          return "hub";           /* hub/root hub */
    case USB_CLASS_PRINTER:      return "usblp";         /* printer */
    case USB_CLASS_MISC:         return "usb-storage";   /* composite/IAD */
    default:                     return "usbcore";       /* generic */
    }
}

/* ---- W4: autosuspend decision ----
 * Gamepads, audio, and storage must NOT autosuspend (wake latency + drops).
 * The kernel emits the module param to disable it for the latency-critical
 * classes. Returns the usbcore.autosuspend param string (or NULL to leave). */
const char *wubu_usbf_autosuspend_param(void)
{
    if (g_autosuspend && (g_hid || g_uaudio || g_msc))
        return "usbcore.autosuspend=-1";
    return NULL;
}

/* ---- W5: summary ---- */
int wubu_usbf_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "usb[ctl=%s%s%s%s hcd=%s hid=%d msc=%d aud=%d vid=%d net=%d ser=%d]",
        g_xhci ? "xhci" : "", g_ehci ? "+ehci" : "",
        g_ohci ? "+ohci" : "", g_usb4 ? "+usb4" : "",
        g_hcd[0] ? g_hcd : "none",
        g_hid, g_msc, g_uaudio, g_uvc, g_unet, g_serial);
}

/* ---- W6: device-mode (gadget) routing ---- */
const char *wubu_usbf_gadget_driver(const char *function)
{
    if (!function) return NULL;
    /* configfs function drivers for USB device mode. */
    if (strstr(function, "uvc"))    return "g_uvc";     /* gadget webcam */
    if (strstr(function, "mass"))   return "g_mass_storage"; /* gadget disk */
    if (strstr(function, "serial")) return "g_serial";  /* gadget serial */
    if (strstr(function, "ether"))  return "g_ether";   /* gadget RNDIS */
    if (strstr(function, "hid"))    return "g_hid";     /* gadget HID */
    return NULL;
}
