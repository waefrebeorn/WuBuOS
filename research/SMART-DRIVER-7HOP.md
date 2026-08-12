# SMART-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage SMART gaps

SMART reports disk health + predictive failure from sensors.

### Attribute routing (wubu_smart.c)

| Attribute | Routing |
|-----------|---------|
| realloc | `reallocated-sectors` |
| wear | `wear-leveling` |
| temp | `temperature` |
| pending | `pending-sector` |
| uncorrect | `uncorrectable` |
| else | `unknown` |

### Status routing

| Status | Routing |
|--------|---------|
| pass / ok | `ok` |
| warn / thresh | `warning` |
| fail / crit | `critical` |
| else | `unknown` |

### Kernel summary

```
smart[smart=0 ata=0 nvme=0 health=0 temp=0 drv=none]
```

Published to `/kv/world/hw_smart`. (No ata/nvme block dev on WSL2.)
