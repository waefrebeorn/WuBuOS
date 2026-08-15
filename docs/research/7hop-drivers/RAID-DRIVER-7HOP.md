# RAID-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux RAID/SAS storage driver gaps

Servers + high-end workstations use hardware RAID + SAS HBAs. WuBuOS
routes them + software RAID (md).

### RAID/HBA routing (wubu_raid.c)

| Controller | Driver |
|-----------|--------|
| Broadcom/LSI MegaRAID | `megaraid_sas` |
| Broadcom/LSI SAS HBA | `mpt3sas` |
| HPE Smart Array (Gen11+) | `smartpqi` |
| Adaptec/PMC/Microsemi | `aacraid` |
| Marvell SAS | `mv_sas` |
| Areca | `arcmsr` |
| 3ware/LSI | `3w-sas` |
| Software RAID | `md` (mdadm, raid0-10) |

### Kernel summary line

```
raid[present=0 sas=0 md=0 drv=none name=-]
```

Published to `/kv/world/hw_raid` by `wubu_raid_summary()`.
