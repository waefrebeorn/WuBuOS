/*
 * wubu_usb.h -- WuBuOS USB subsystem (xHCI host controller + HID devices).
 *
 * The kernel's USB stack is POLLING-based: wubu_usb_poll() walks the xHCI
 * event ring + root-hub ports on every call, so it needs no interrupt
 * delivery (the metal interrupt path is still being brought up).  HID
 * devices (keyboard, mouse, gamepad, any report-descriptor device) feed
 * the kernel's input layer via input_key_push/input_mouse_push.
 */
#ifndef WUBU_USB_H
#define WUBU_USB_H

#include <stdint.h>

/* HID device classes we understand (interface class/subclass/protocol). */
#define USB_HID_CLASS_BOOT   3
#define USB_HID_PROTO_KBD    1
#define USB_HID_PROTO_MOUSE  2

typedef enum {
    WUBU_USB_DEV_NONE = 0,
    WUBU_USB_DEV_KEYBOARD,
    WUBU_USB_DEV_MOUSE,
    WUBU_USB_DEV_GAMEPAD,
    WUBU_USB_DEV_OTHER_HID,
} wubu_usb_dev_kind;

/* One enumerated HID device. */
typedef struct {
    uint8_t  slot;            /* xHCI slot id (1-based, 0 = none) */
    uint8_t  kind;            /* wubu_usb_dev_kind */
    uint8_t  ep_in;           /* interrupt-in endpoint address (0x81..) */
    uint16_t mps;             /* endpoint max packet size */
    uint16_t report_len;      /* expected HID report length */
    uint8_t  report[64];
    uint8_t  has_report;
} wubu_usb_dev;

/* Init: PCI-scan for the xHCI controller, bring it up, enumerate attached
 * HID devices.  Returns 0 on success (controller running). */
int  wubu_usb_init(void);

/* Poll the event ring + ports (call from the input/bonzi loop). */
void wubu_usb_poll(void);

/* Counters (for boot assertions + status). */
int  wubu_usb_controller_ok(void);
int  wubu_usb_device_count(void);
int  wubu_usb_keyboard_count(void);
int  wubu_usb_mouse_count(void);

/* The enumerated devices array. */
const wubu_usb_dev *wubu_usb_devices(int *count);

/* Called by the HID parser with a fresh report. */
void wubu_usb_hid_process(uint8_t slot, const uint8_t *report, uint16_t len);

#endif /* WUBU_USB_H */
