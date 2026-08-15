/*
 * wubu_peripheral.c -- kernel-owned peripheral driver routing.
 *
 * The "long tail" of hardware that makes an OS "run on everything":
 * legacy serial (16550/8250 UART), parallel (lp/ppdev), GPIO/pinctrl,
 * and the hwmon sensor family (voltage/temp/fan/PWM). These are the
 * chips that never get attention but show up on old boards, test rigs,
 * and embedded systems.
 *
 * Common gaps:
 *   - serial: 8250/16550 base + 8250_dw (DesignWare), 8250_pci, ttyS0-3
 *   - parallel: parport, lp (printer), ppdev (raw); removed on modern but
 *     needed for legacy LPT printers/devices
 *   - GPIO: gpiolib + pinctrl; sysfs /sys/class/gpio (legacy) or
 *     /sys/bus/gpio (chardev gpiochipN)
 *   - sensors: hwmon family — lm75, tmp102, w83627ehf, it87, nct6775,
 *     coretemp (Intel), k10temp (AMD), bme280, ams (ambient)
 *
 * WuBuOS owns this: probe the peripheral bus, route to the right driver,
 * and expose the hwmon + GPIO topology to the Brain.
 */
#include "wubu_peripheral.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

/* ---- PCI classes for peripheral controllers ---- */
#define PCI_CLASS_COMM      0x07   /* simple comm (serial) */
#define PCI_SUBCLASS_8250   0x00   /* 16550/8250 serial */
#define PCI_SUBCLASS_PARALLEL 0x01 /* parallel port */
#define PCI_SUBCLASS_MULTIPORT 0x02
#define PCI_SUBCLASS_MODEM  0x03
#define PCI_CLASS_SERIAL    0x0C   /* serial bus */
#define PCI_SUBCLASS_GPIO   0x01   /* GPIO controller */
#define PCI_SUBCLASS_SMBUS  0x05   /* SMBus (hwmon access) */
#define PCI_CLASS_MEM       0x05

/* ---- Global state ---- */
static int g_serial = 0;
static int g_parallel = 0;
static int g_gpio = 0;
static int g_hwmon = 0;
static int g_smbus = 0;
static char g_serial_drv[32] = "";
static char g_hwmon_chip[32] = "";

/* ---- W1: probe peripheral topology ---- */
void wubu_peripheral_probe(void)
{
    g_serial = 0; g_parallel = 0; g_gpio = 0; g_hwmon = 0; g_smbus = 0;
    g_serial_drv[0] = '\0'; g_hwmon_chip[0] = '\0';

#ifdef WUBU_HOSTED
    /* Serial via /sys/class/tty. */
    g_serial = (access("/sys/class/tty/ttyS0", R_OK) == 0);

    /* GPIO chardev or sysfs. */
    g_gpio = (access("/sys/bus/gpio", R_OK) == 0) ||
             (access("/dev/gpiochip0", R_OK) == 0);

    /* hwmon sensors. */
    g_hwmon = (access("/sys/class/hwmon", R_OK) == 0);

    /* Parallel port. */
    g_parallel = (access("/dev/lp0", R_OK) == 0) ||
                 (access("/sys/class/parport", R_OK) == 0);

    /* SMBus (I2C for hwmon). */
    g_smbus = (access("/sys/bus/i2c", R_OK) == 0);

    /* PCI scan for serial/GPIO controllers (bare-metal only). */
    if (wubu_hw_is_wsl()) return;
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    for (int i = 0; i < n; i++) {
        if ((devs[i].class_code >> 8) == PCI_CLASS_COMM &&
            devs[i].subclass == PCI_SUBCLASS_8250) {
            g_serial = 1;
            strcpy(g_serial_drv, "8250_pci");
        } else if ((devs[i].class_code >> 8) == PCI_CLASS_SERIAL &&
                   devs[i].subclass == PCI_SUBCLASS_GPIO) {
            g_gpio = 1;
        }
    }
#endif
}

/* ---- W2: driver routing ---- */
const char *wubu_peripheral_serial_driver(void)
{
    if (!g_serial_drv[0])
        strcpy(g_serial_drv, "8250");
    return g_serial_drv;
}

const char *wubu_peripheral_parallel_driver(void)
{
    return "parport_pc";  /* lp + ppdev */
}

const char *wubu_peripheral_gpio_driver(void)
{
    return "gpiolib";     /* pinctrl + gpio chardev */
}

/* ---- W3: accessors ---- */
int  wubu_peripheral_has_serial(void)    { return g_serial; }
int  wubu_peripheral_has_parallel(void)  { return g_parallel; }
int  wubu_peripheral_has_gpio(void)      { return g_gpio; }
int  wubu_peripheral_has_hwmon(void)     { return g_hwmon; }
int  wubu_peripheral_has_smbus(void)     { return g_smbus; }

/* ---- W4: summary ---- */
int wubu_peripheral_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "peri[serial=%d(%s) par=%d gpio=%d hwmon=%d smbus=%d]",
        g_serial, wubu_peripheral_serial_driver(),
        g_parallel, g_gpio, g_hwmon, g_smbus);
}
