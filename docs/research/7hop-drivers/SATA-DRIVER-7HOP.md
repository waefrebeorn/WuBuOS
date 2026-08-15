# SATA-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux advanced SATA/NCQ driver gaps

SATA is the ubiquitous disk bus. NCQ, hotplug, port multipliers extend it.

### SATA capabilities (wubu_sata.c)
- **NCQ** (native command queuing): up to 32 outstanding commands
- **Hotplug**: async device insert/remove (libata)
- **Port multiplier** (sata_pmp): one port -> 15 devices
- **SMART**: health/self-test via smartctl
- **libahci**: AHCI driver (ICH8-10, SB700+, generic)

### Controller routing

| Controller | Driver |
|-----------|--------|
| AHCI | `ahci` |
| Port multiplier | `sata_pmp` |
| NVMe | `nvme` |
| USB storage | `usb-storage` |
| IDE | `ata_piix` |

### Kernel summary line

```
sata[present=1 ahci=1 ncq=1 hotplug=1 pmp=0 smart=0 drv=ahci]
```

Published to `/kv/world/hw_sata` by `wubu_sata_summary()`.

**Verified live:** this host reports `ahci=1 ncq=1 hotplug=1` — real AHCI.
