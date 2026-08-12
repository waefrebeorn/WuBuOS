# ZNSZONE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NVMe ZNS zone gaps

ZNS manages zones (sequential write + reset) per the Zone Descriptor.

### Zone state routing (wubu_znszone.c)

| State | Routing |
|-------|---------|
| empty | `empty` |
| implicit | `implicit-open` |
| explicit | `explicit-open` |
| closed | `closed` |
| full | `full` |
| inactive | `inactive` |
| else | `empty` |

### Zone action routing

| Action | Routing |
|--------|---------|
| reset | `reset` |
| finish | `finish` |
| open | `open` |
| close | `close` |
| write | `write` |
| report | `report` |
| else | `report` |

### Kernel summary

```
znszone[zns=0 zone=0 desc=0 action=0 state=0 drv=none]
```

Published to `/kv/world/hw_znszone`. (No /sys/block/nvme on WSL2.)
