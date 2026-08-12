# FLUSH2-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage flush/barrier gaps

Storage flush (barrier, fsync, write barrier) persists data to disk.

### Type routing (wubu_flush2.c)

| Type | Routing |
|------|---------|
| barrier | `barrier` |
| fsync | `fsync` |
| fdatasync / data | `fdatasync` |
| cache | `cache-flush` |
| write | `write-barrier` |
| else | `barrier` |

### Command routing

| Interface | Routing |
|-----------|---------|
| ATA | `FLUSH CACHE` |
| SCSI | `SYNCHRONIZE CACHE` |
| NVMe | `FLUSH` |
| write | `write barrier` |
| else | `FLUSH CACHE` |

### Kernel summary

```
flush2[fl=0 barrier=0 fsync=0 cache=0 flushcmd=0 drv=none]
```

Published to `/kv/world/hw_flush2`. (No /sys/block on WSL2.)
