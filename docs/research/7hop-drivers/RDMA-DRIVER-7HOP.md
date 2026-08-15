# RDMA-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux RDMA/InfiniBand driver gaps

RDMA bypasses the CPU for data transfer. WuBuOS routes the verbs fabric.

### RDMA driver routing (wubu_rdma.c)

| NIC | Driver |
|-----|--------|
| Mellanox/NVIDIA ConnectX | `mlx5_ib` |
| Intel iWARP (X722) | `irdma` |
| Broadcom RoCE | `bnxt_re` |
| QLogic/Cavium RoCE | `qedr` |
| Soft-RoCE | `rdma_rxe` |
| Soft-iWARP | `siw` |

### Components
- ib_core/ib_verbs: InfiniBand/RDMA core (infiniband.ko)
- InfiniBand, RoCE (RDMA over Converged Ethernet), iWARP
- /sys/class/infiniband: ibdevs, ports, active_speed

### Kernel summary line

```
rdma[rdma=0 ib=0 roce=0 iwarp=0 soft_roce=0 ports=0 drv=none]
```

Published to `/kv/world/hw_rdma` by `wubu_rdma_summary()`.
