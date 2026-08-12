/*
 * wubu_usb.h -- kernel-owned USB driver routing + power management.
 */
#ifndef WUBU_USB_H
#define WUBU_USB_H

#include <stddef.h>

/* W1: probe the USB topology (host controllers + connector + gadget). */
void wubu_usbf_probe(void);

/* W2: accessors */
int  wubu_usbf_present(void);
int  wubu_usbf_has_xhci(void);     /* xHCI (USB 3.x) controller */
int  wubu_usbf_has_ehci(void);     /* EHCI (USB 2.0) */
int  wubu_usbf_has_ohci(void);     /* OHCI/UHCI (USB 1.1) */
int  wubu_usbf_has_usb4(void);     /* USB4/Thunderbolt */
int  wubu_usbf_has_usbc(void);     /* USB-C connector present */
int  wubu_usbf_has_otg(void);      /* OTG dual-role capable */
int  wubu_usbf_has_hid(void);      /* HID (gamepad/kb/mouse) */
int  wubu_usbf_has_storage(void);  /* mass storage */
int  wubu_usbf_has_audio(void);    /* USB audio */
int  wubu_usbf_has_video(void);    /* UVC video */
int  wubu_usbf_has_network(void);  /* USB network */
int  wubu_usbf_has_serial(void);   /* USB serial */
const char *wubu_usbf_hcd(void);   /* "xhci_hcd"|"ehci_hcd"|"ohci_hcd"|"thunderbolt" */
const char *wubu_usbf_hcd_name(void);
int  wubu_usbf_host_vendor(void);  /* PCI vendor of the host controller */

/* W3: route a USB class code to its module. */
const char *wubu_usbf_class_driver(int cls);

/* W4: autosuspend decision (latency-critical classes must not sleep). */
const char *wubu_usbf_autosuspend_param(void);

/* W5: summary fragment. */
int wubu_usbf_summary(char *out, size_t cap);

/* W6: device-mode (gadget) routing. */
const char *wubu_usbf_gadget_driver(const char *function);

#endif /* WUBU_USB_H */
