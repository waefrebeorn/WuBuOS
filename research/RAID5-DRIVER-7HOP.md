# RAID5-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux RAID5 gaps

RAID5 stripes data + parity across disks.

### Layout routing (wubu_raid5.c)

| Layout | Routing |
|--------|---------|
| left-asym / la | `left-asymmetric` |
| left-sym / ls | `left-symmetric` |
| right-asym / ra | `right-asymmetric` |
| right-sym / rs | `right-symmetric` |
| else | `left-symmetric` |

### Parity routing

| Parity | Routing |
|---------|---------|
| P+Q / double / raid6 | `P+Q` |
| P / single | `P` |
| Q | `Q` |
| else | `P` |

### Kernel summary

```
raid5[raid5=0 stripe=0 layout=0 parity=0 disks=0 drv=none]
```

Published to `/kv/world/hw_raid5`. (No /proc/mdstat on WSL2.)
