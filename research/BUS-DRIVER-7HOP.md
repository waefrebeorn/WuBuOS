# BUS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux I2C/SPI bus controller gaps

I2C and SPI are the on-board buses for sensors/codecs/RTCs/touch. WuBuOS
routes the controllers.

### I2C controller routing (wubu_bus.c)

| Controller | Driver |
|-----------|--------|
| Intel SMBus | `i2c-piix4` |
| Intel/AMD DesignWare | `i2c-designware` |
| NXP i.MX | `i2c-imx` |
| Broadcom | `i2c-bcm2835` |
| Qualcomm | `i2c-qcom-geni` |
| Nvidia | `i2c-tegra` |

### SPI controller routing

| Controller | Driver |
|-----------|--------|
| Marvell | `spi-orion` |
| NXP i.MX | `spi-imx` |
| Broadcom | `spi-bcm2835` |
| Nvidia | `spi-tegra` |
| Qualcomm | `spi-qcom-qspi` |
| Intel | `spi-pxa2xx` |

### Kernel summary line

```
bus[i2c=1(0/i2c-core) spi=0(0/none)]
```

Published to `/kv/world/hw_bus` by `wubu_bus_summary()`.
