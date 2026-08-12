# SMR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage SMR gaps

SMR/ZNS drives write in sequential zones with managed write pointers.

### Zone routing (wubu_smr.c)

| Zone type | Routing |
|-----------|---------|
| swr | `sequential-write-required` |
| soc | `sequential-write-preferred` |
| conv | `conventional` |
| off | `offline` |
| ro | `read-only` |
| else | `sequential-write-required` |

### Op routing

| Op | Routing |
|----|---------|
| append | `append` |
| reset | `reset` |
| report | `report` |
| open | `open` |
| close | `close` |
| else | `report` |

### Kernel summary

```
smr[smr=0 zone=0 zns=0 wp=0 zonefs=0 drv=none]
```

Published to `/kv/world/hw_smr`. (No host-managed zone on WSL2.)
