# PHY-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux Ethernet PHY/MDIO driver gaps

The PHY converts the MAC's signal to the wire. WuBuOS routes it via phylib.

### PHY driver routing (wubu_phy.c)

| PHY | Driver |
|-----|--------|
| Marvell 88E | `marvell-phy` |
| Broadcom bcm | `broadcom-phy` |
| Micrel KSZ | `micrel-phy` |
| Realtek RTL | `realtek-phy` |
| TI DP83867/22 | `ti-phy` |
| Motorcomm YT85 | `motorcomm` |
| Qualcomm/Atheros AT803 | `at803x` |
| Intel (igc) | `igc-phy` |
| generic | `genphy` |

### Kernel summary line

```
phy[present=0 mdio=1 link=0 drv=none name=-]
```

Published to `/kv/world/hw_phy` by `wubu_phy_summary()`.
