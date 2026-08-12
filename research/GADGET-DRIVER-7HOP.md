# GADGET-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux USB gadget + NVMe endurance gaps

Two capabilities: USB gadget mode + NVMe SSD health telemetry.

### USB gadget routing (wubu_gadget.c)

| Controller | Driver |
|------------|--------|
| DWC3 | `dwc3` |
| CDNS3 | `cdns3` |
| Generic UDC | `udc` |
| configfs | `configfs` |

### Gadget functions

| Function | Routing |
|----------|---------|
| mass storage | `mass_storage` |
| RNDIS/Ethernet | `rndis` |
| Serial ACM | `acm` |
| HID | `hid` |
| UVC camera | `uvc` |
| ECM | `ecm` |

### NVMe endurance routing

| Metric | Routing |
|--------|---------|
| SMART/log | `smart-log` |
| Media wear | `media-wear` |
| TBW | `tbw` |
| % used | `pct-used` |

### Kernel summary line

```
gadget[udc=0 cfgfs=0 active=0 nvme=1 smart=0 drv=none]
```

Published to `/kv/world/hw_gadget` by `wubu_gadget_summary()`.

**Verified live:** this host reports `nvme=1`.
