# MULTIGIG-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Ethernet multi-gig (2.5/5/10G) gaps

Beyond gigabit: 2.5GBase-T, 5GBase-T, 10GBase-T (802.3bz NBASE-T).

### Multi-gig driver routing (wubu_multigig.c)

| PHY | Driver |
|-----|--------|
| Realtek 2.5G | `r8125` |
| Realtek RTL8126 | `r8125` |
| Aquantia/Marvell 10G | `aquantia` |
| Marvell AQC113 | `atlantic` |
| Marvell 88X3310 | `m88x3310` |
| Broadcom 84881 | `bcm84881` |
| Intel x550 2.5G | `ixgbe` |

### Kernel summary line

```
mgig[present=0 2g5=0 5g=0 10g=0 drv=none name=-]
```

Published to `/kv/world/hw_mgig` by `wubu_multigig_summary()`.
