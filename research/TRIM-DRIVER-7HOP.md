# TRIM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage TRIM + USB-C alt mode gaps

Two capabilities: SSD TRIM (discard) + USB-C alt mode.

### TRIM routing (wubu_trim.c)

| Filesystem | Routing |
|------------|---------|
| ext4 | `ext4-discard` |
| btrfs | `btrfs-discard` |
| xfs | `xfs-discard` |
| f2fs | `f2fs-discard` |
| NVMe | `nvme-deallocate` |
| ATA | `ata-trim` |

Components: fstrim (periodic), discard mount option, ATA DSM / NVMe
DEALLOCATE.

### USB-C alt mode routing

| Mode | Routing |
|------|---------|
| DisplayPort | `displayport-alt` |
| Thunderbolt | `thunderbolt-alt` |
| USB4 | `usb4` |
| generic | `typec-altmode` |

### Kernel summary line

```
trim[trim=1 fstrim=1 discard=1 altmode=0 tb=0 drv=fstrim]
```

Published to `/kv/world/hw_trim` by `wubu_trim_summary()`.

**Verified live:** this host reports `trim=1 fstrim=1 discard=1`.
