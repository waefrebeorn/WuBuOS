/*
 * wubu_drv_usb.c -- the USB CLASS drivers (the Deck's USB-C + the
 * laptops' USB ports).
 *
 * The xHCI stack (wubu_xhci.c) enumerates the devices; this driver
 * binds the USB classes that matter for a gaming/desktop OS:
 *
 *   - the HID class (0x03/0x01): keyboards, mice, gamepads — the
 *     events flow into the kernel HID feed (wubu_hid)
 *   - the mass storage class (0x08/0x06): USB drives + SD readers —
 *     the block path
 *   - the Bluetooth class (0xE0/0x01): BT controllers (the RZ616's
 *     BT, Intel AX's BT)
 *
 * The probe RECORDS the class binding; the device count per class is
 * the integration signal (the /n/hw subtree + the world state).
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_usb.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int n_hid;          /* the HID class devices */
    int n_msc;          /* the mass storage class */
    int n_bt;           /* the Bluetooth class */
    int present;
} wubu_usb_class_t;

static wubu_usb_class_t g_usb;

/* USB1: probe a USB-class device (the registry calls it on the
 * matching class id). */
static int usb_hid_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    g_usb.present = 1;
    g_usb.n_hid++;
    return 0;
}
static int usb_msc_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    g_usb.present = 1;
    g_usb.n_msc++;
    return 0;
}
static int usb_bt_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    g_usb.present = 1;
    g_usb.n_bt++;
    return 0;
}

const wubu_drv_id_t wubu_usb_hid_ids[] = {
    { WUBU_DRV_ANY, WUBU_DRV_ANY, 0x03, 0x01 },  /* the HID class */
    { 0, 0, 0, 0 },
};
const wubu_drv_id_t wubu_usb_msc_ids[] = {
    { WUBU_DRV_ANY, WUBU_DRV_ANY, 0x08, 0x06 },  /* the mass storage */
    { 0, 0, 0, 0 },
};
const wubu_drv_id_t wubu_usb_bt_ids[] = {
    { WUBU_DRV_ANY, WUBU_DRV_ANY, 0xE0, 0x01 },  /* the BT class */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_usb_hid = { "usb-hid", wubu_usb_hid_ids, 1, usb_hid_probe };
const wubu_drv_t wubu_drv_usb_msc = { "usb-msc", wubu_usb_msc_ids, 1, usb_msc_probe };
const wubu_drv_t wubu_drv_usb_bt  = { "usb-bt",  wubu_usb_bt_ids,  1, usb_bt_probe };

/* the counts (the integration signal) */
int wubu_usb_hid_count(void) { return g_usb.n_hid; }
int wubu_usb_msc_count(void) { return g_usb.n_msc; }
int wubu_usb_bt_count(void)  { return g_usb.n_bt; }
int wubu_usb_present(void)   { return g_usb.present; }
