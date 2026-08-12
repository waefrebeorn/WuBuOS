/*
 * wubu_peripheral.h -- kernel-owned peripheral driver routing.
 */
#ifndef WUBU_PERIPHERAL_H
#define WUBU_PERIPHERAL_H

#include <stddef.h>

/* W1: probe the peripheral topology. */
void wubu_peripheral_probe(void);

/* W2: driver routing. */
const char *wubu_peripheral_serial_driver(void);    /* 8250|8250_pci */
const char *wubu_peripheral_parallel_driver(void);  /* parport_pc */
const char *wubu_peripheral_gpio_driver(void);      /* gpiolib */

/* W3: accessors */
int wubu_peripheral_has_serial(void);
int wubu_peripheral_has_parallel(void);
int wubu_peripheral_has_gpio(void);
int wubu_peripheral_has_hwmon(void);
int wubu_peripheral_has_smbus(void);

/* W4: summary fragment. */
int wubu_peripheral_summary(char *out, size_t cap);

#endif /* WUBU_PERIPHERAL_H */
