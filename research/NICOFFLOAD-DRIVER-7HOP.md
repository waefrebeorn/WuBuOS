# NICOFFLOAD-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NIC offload + multi-queue gaps

Modern NICs offload packet processing. WuBuOS routes + exposes the
offload/queue topology.

### Offloads (ethtool / netdev features)
- TSO/GSO (segmentation offload), GRO/LRO (receive offload)
- RSS (receive side scaling, hash flows across queues)
- RPS/XPS (software packet steering), RFS (flow steering)

### NIC driver routing (wubu_nicoffload.c)

| NIC | Driver |
|-----|--------|
| Intel X540 | `ixgbe` |
| Intel XL710 | `i40e` |
| Intel I225 | `igc` |
| Intel E810 | `ice` |
| Mellanox | `mlx5` |
| Broadcom | `bnxt` |
| Intel 82579 | `e1000e` |
| Solarflare | `sfc` |

### Kernel summary line

```
nicoff[nic=1 queues=8 tso=1 gro=1 rss=1 mq=1 drv=none]
```

Published to `/kv/world/hw_nicoff` by `wubu_nicoffload_summary()`.

**Verified live:** this host reports `queues=8 rss=1 mq=1` — 8-queue NIC.
