# ZONED-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux SMR/Zoned storage gaps

Zoned storage (SMR HDDs, ZNS NVMe) writes sequentially per zone.

### Zoned driver routing (wubu_zoned.c)

| Device | Driver |
|--------|--------|
| ZNS NVMe | `nvme-zns` |
| SMR HDD (ZBC) | `zbc` |
| Zone filesystem | `zonefs` |
| generic | `blk-zoned` |

### Components
- blk-zoned: zoned block device core
- ZBC SMR: shingled magnetic recording HDDs
- ZNS NVMe: zoned namespace (high-density flash)
- zonefs: zone filesystem for direct zone access

### Kernel summary line

```
zoned[zoned=0 smr=0 zns=0 zonefs=0 zones=0 drv=none]
```

Published to `/kv/world/hw_zoned` by `wubu_zoned_summary()`.
