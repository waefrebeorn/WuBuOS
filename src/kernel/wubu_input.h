/*
 * wubu_input.h -- kernel-owned input device driver routing.
 */
#ifndef WUBU_INPUT_H
#define WUBU_INPUT_H

#include <stddef.h>

/* W1: probe controllers (USB HID / BLE). No-op on WSL2 (host owns input). */
void wubu_input_probe(void);

/* W2: test hooks */
void wubu_input_set_controller(int vendor, int device);
void wubu_input_set_poll_hz(int hz);

/* W2b: accessors */
int          wubu_input_poll_hz(void);
int          wubu_input_has_controller(void);
const char *wubu_input_controller_driver(void);  /* "xpad"|"xpadneo"|"hid-playstation" */
const char *wubu_input_controller_name(void);
int          wubu_input_uses_ble(void);           /* controller is Bluetooth */
int          wubu_input_usb_bus_present(void);

/* W4: routing hint (out-of-tree driver needed? BLE profile?). */
const char *wubu_input_routing_hint(void);

/* W5: safe mouse polling rate (cap 1000Hz). */
int wubu_input_safe_poll_hz(void);

/* W6: summary fragment. */
int wubu_input_summary(char *out, size_t cap);

#endif /* WUBU_INPUT_H */
