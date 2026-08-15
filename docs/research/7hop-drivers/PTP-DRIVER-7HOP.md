# PTP-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Ethernet PTP/TSN + haptics gaps

Precision time + deterministic networking + input vibration.

### PTP/TSN (wubu_ptp.c)
- **PTP hardware clock** (phc): /dev/ptpN, phc2sys, hardware timestamping
- **802.1AS gPTP**: AV/automotive time sync
- **TSN**: taprio (time-aware shaper), etf (earliest tx), mqprio

### PTP driver routing

| NIC | Driver |
|-----|--------|
| Intel I210/I225 (igb) | `igb-ptp` |
| Intel X540 (ixgbe) | `ixgbe-ptp` |
| Intel E810 (ice) | `ice-ptp` |
| Mellanox | `mlx5-ptp` |
| TSN | `tsn` |

### Haptics routing
Xbox -> `xpad`, DualSense/PS -> `sony`, generic HID -> `hid-ff`, else `ff-memless`.

### Kernel summary line

```
ptp[ptp=0 phc=0 tsn=0 haptic=0(none) drv=none]
```

Published to `/kv/world/hw_ptp` by `wubu_ptp_summary()`.
