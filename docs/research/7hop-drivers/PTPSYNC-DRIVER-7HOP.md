# PTPSYNC-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NIC PTP time sync gaps

PTP (IEEE 1588) synchronizes clocks precisely over the network via the
NIC PHC (PTP hardware clock).

### PTP sync routing (wubu_ptp_sync.c)

| Component | Role |
|-----------|------|
| ptp4l | PTP daemon |
| phc2sys | PHC->system clock sync |
| PHC | hardware clock (/dev/ptp*) |

### PTP roles

| Role | Routing |
|------|---------|
| master/grandmaster | `master` |
| slave | `slave` |
| transparent | `transparent` |
| boundary | `boundary` |

### NIC PHC routing

| NIC | Driver |
|-----|--------|
| igc (i225) | `igc-phc` |
| ixgbe (x550) | `ixgbe-phc` |
| e1000e | `e1000e-phc` |
| mlx5 | `mlx5-phc` |

### Kernel summary line

```
ptpsync[phc=0 ptp4l=0 phc2sys=0 hwts=0 synced=0 drv=none]
```

Published to `/kv/world/hw_ptpsync` by `wubu_ptp_sync_summary()`.
