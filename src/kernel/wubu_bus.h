/*
 * wubu_bus.h -- kernel-owned I2C/SPI bus controller driver routing.
 */
#ifndef WUBU_BUS_H
#define WUBU_BUS_H

#include <stddef.h>

/* W1: probe the I2C/SPI bus topology. */
void wubu_bus_probe(void);

/* W2: accessors */
int  wubu_bus_has_i2c(void);
int  wubu_bus_has_spi(void);
int  wubu_bus_i2c_controllers(void);
int  wubu_bus_spi_controllers(void);
const char *wubu_bus_i2c_driver(void);
const char *wubu_bus_spi_driver(void);

/* W3: bus controller driver routing. */
const char *wubu_bus_i2c_driver_for(const char *ctrl);
const char *wubu_bus_spi_driver_for(const char *ctrl);

/* W4: summary fragment. */
int wubu_bus_summary(char *out, size_t cap);

#endif /* WUBU_BUS_H */
