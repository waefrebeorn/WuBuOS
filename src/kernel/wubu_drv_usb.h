/*
 * wubu_drv_usb.h -- the USB class drivers.
 */
#ifndef WUBU_DRV_USB_H
#define WUBU_DRV_USB_H

/* the drivers (registered by the registry) */
extern const struct wubu_drv wubu_drv_usb_hid;
extern const struct wubu_drv wubu_drv_usb_msc;
extern const struct wubu_drv wubu_drv_usb_bt;

/* the counts (the integration signal) */
int wubu_usb_hid_count(void);
int wubu_usb_msc_count(void);
int wubu_usb_bt_count(void);
int wubu_usb_present(void);

#endif
