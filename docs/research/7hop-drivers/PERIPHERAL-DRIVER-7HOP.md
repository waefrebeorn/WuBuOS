# PERIPHERAL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux peripheral driver gaps

The "long tail" of hardware: serial, parallel, GPIO, hwmon sensors. These
are the chips on old boards, test rigs, and embedded systems that an OS
must support to "run on everything".

### Serial (UART 16550/8250)

- `8250` (legacy ISA/MMIO), `8250_pci` (PCI 16550/16650), `8250_dw`
  (DesignWare), ttyS0-3. WuBuOS routes `wubu_peripheral_serial_driver()`.

### Parallel

- `parport_pc` (parport core) + `lp` (printer) + `ppdev` (raw device).
  Legacy LPT printers/devices. `wubu_peripheral_parallel_driver()`.

### GPIO / pinctrl

- `gpiolib` + `pinctrl`. Access via `/dev/gpiochipN` (chardev) or
  `/sys/class/gpio` (legacy sysfs). `wubu_peripheral_gpio_driver()`.

### hwmon sensors

- `coretemp` (Intel), `k10temp` (AMD), `lm75`, `tmp102`, `it87`,
  `nct6775`, `w83627ehf`, `bme280`, `ams` — voltage/temp/fan/PWM.
  Accessed via `/sys/class/hwmon`. SMBus (I2C) carries the sensor bus.

### Kernel summary line

```
peri[serial=1(8250) par=0 gpio=1 hwmon=1 smbus=1]
```

Published to `/kv/world/hw_peri` by `wubu_peripheral_summary()`.
