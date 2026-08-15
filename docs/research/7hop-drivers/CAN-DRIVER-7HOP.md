# CAN-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux SocketCAN driver gaps

CAN (Controller Area Network) is the automotive + industrial control bus.
"Runs on everything" includes cars, drones, and industrial rigs.

### Controller -> driver routing (wubu_can.c)

| Controller | Driver | Notes |
|-----------|--------|-------|
| mcp2515 (Microchip SPI) | `mcp251x` | the ubiquitous DIY/auto chip |
| mcp2518 (Microchip) | `mcp251xfd` | CAN FD |
| PEAK PCAN-USB | `peak_usb` | the lab standard |
| ESD CAN-USB | `esd_usb2` | industrial |
| Generic SocketCAN USB | `gs_usb` | cheap adapters |
| ELM327 OBD-II serial | `can327` | car diagnostics |
| NXP/Philips sja1000 | `sja1000` | legacy parallel/PCI |
| Kvaser | `kvaser_usb` | professional |

### Kernel summary line

```
can[present=0 usb=0 spi=0 pci=0 drv=none]
```

Published to `/kv/world/hw_can` by `wubu_can_summary()`.
