# MEM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux memory/ECC (EDAC) driver gaps

Memory health = reliability. WuBuOS routes to the EDAC subsystem, detects
ECC, and reads DIMM SPD.

### EDAC driver routing per memory controller (wubu_mem.c)

| Controller | Driver |
|-----------|--------|
| Intel Nehalem/Westmere | `i7core_edac` |
| Intel Sandy Bridge+ | `sb_edac` |
| Intel Skylake-X | `skx_edac` |
| Intel 10nm (Ice Lake+) | `i10nm_edac` |
| AMD Zen | `amd64_edac` |
| AMD AL | `al_mc_edac` |

### ECC telemetry
- CE count (corrected), UE count (uncorrected) via `/sys/devices/system/edac/mc/mc0/`
- SPD via SMBus eeprom (ee1004 = DDR4, at24 = generic)

### Kernel summary line

```
mem[edac=1 ecc=0 spd=0 ce=0 ue=0 drv=edac_mc]
```

Published to `/kv/world/hw_mem` by `wubu_mem_summary()`.

**Verified live:** this host reports `edac=1 drv=edac_mc` — EDAC detected.
