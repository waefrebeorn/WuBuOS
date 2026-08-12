# USB4-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux USB4/Thunderbolt driver gaps

USB4/Thunderbolt is the high-speed daisy-chain (40/80 Gbps): eGPU, docks,
NVMe, displays. WuBuOS routes the TB/USB4 controller + security.

### TB/USB4 routing (wubu_usb4.c)

| Host | Driver |
|------|--------|
| Intel (Titan/Maple/Goshen Ridge) | `thunderbolt` |
| AMD | `usb4` |
| Apple | `thunderbolt` |

### Components
- `thunderbolt.ko`: Intel/AMD/Apple TB3 + USB4 host controller
- `bolt` (boltctl): userspace TB manager, security levels
- /sys/bus/thunderbolt: domains, routers, ports, NVM
- USB4: retimer + CLx (low-power link)

### Kernel summary line

```
usb4[tb=0 usb4=0 bolt=0 secure=0 domains=0 drv=none]
```

Published to `/kv/world/hw_usb4` by `wubu_usb4_summary()`.
