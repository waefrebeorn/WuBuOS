/*
 * wubu_can.h -- kernel-owned CAN bus driver routing (SocketCAN).
 */
#ifndef WUBU_CAN_H
#define WUBU_CAN_H

#include <stddef.h>

/* W1: probe the CAN topology. */
void wubu_can_probe(void);

/* W2: accessors */
int wubu_can_present(void);
int wubu_can_has_usb(void);
int wubu_can_has_spi(void);
int wubu_can_has_pci(void);
const char *wubu_can_driver(void);

/* W3: controller routing. */
const char *wubu_can_controller_driver(const char *chip);

/* W4: summary fragment. */
int wubu_can_summary(char *out, size_t cap);

#endif /* WUBU_CAN_H */
