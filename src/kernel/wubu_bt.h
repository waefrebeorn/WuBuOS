/*
 * wubu_bt.h -- kernel-owned Bluetooth driver routing + LE Audio.
 */
#ifndef WUBU_BT_H
#define WUBU_BT_H

#include <stddef.h>

/* W1: probe the Bluetooth topology. */
void wubu_bt_probe(void);

/* W2: accessors */
int  wubu_bt_present(void);
int  wubu_bt_usb(void);
int  wubu_bt_pci(void);
int  wubu_bt_uart(void);
int  wubu_bt_le_audio(void);   /* LE Audio / isochronous support */
const char *wubu_bt_driver(void);

/* W3: controller driver routing per vendor. */
const char *wubu_bt_controller_driver(const char *vendor);

/* W4: summary fragment. */
int wubu_bt_summary(char *out, size_t cap);

#endif /* WUBU_BT_H */
