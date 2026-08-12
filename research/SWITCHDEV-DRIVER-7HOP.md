# SWITCHDEV-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux network switch fabric gaps

Managed switches (DSA + switchdev ASICs) connect many ports. WuBuOS routes
them.

### Switch driver routing (wubu_switchdev.c)

| Chip | Driver |
|------|--------|
| Marvell mv88e6xxx | `mv88e6xxx` |
| Microchip KSZ | `ksz_common` |
| Realtek RTL8366 | `rtl8366rb` |
| MediaTek MT7530 | `mt7530` |
| Qualcomm QCA8K | `qca8k` |
| Mellanox Spectrum | `mlxsw_spectrum` |
| Broadcom b53 | `b53` |
| Microsemi Ocelot/Felix | `ocelot_switch` |

### Subsystems
- **switchdev**: switch ASIC model (mlxsw Spectrum, ocelot)
- **DSA** (Distributed Switch Architecture): mv88e6xxx, ksz, qca8k, b53
- Router SoC switches: mt7530, qca8k

### Kernel summary line

```
sw[present=0 dsa=0 asic=0 drv=none name=-]
```

Published to `/kv/world/hw_sw` by `wubu_switchdev_summary()`.
