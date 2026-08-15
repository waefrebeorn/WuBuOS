# FLUSH-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage cache flush gaps

Cache flush (write barriers) ensure data reaches stable storage.

### Flush routing (wubu_flush.c)

| Component | Role |
|-----------|------|
| write barriers | FLUSH/FUA |
| fsync/fdatasync | flush syscall |
| NVMe FLUSH | opcode 0x00 |
| dm-flush | device-mapper |

| Mode | Routing |
|------|---------|
| writeback | `write-back` |
| writethrough | `write-through` |

| Op | Routing |
|----|---------|
| fsync | `fsync` |
| fdatasync | `fdatasync` |
| nvme | `nvme-flush` |
| barrier | `write-barrier` |

### Kernel summary

```
flush[flush=1 barrier=1 wbcache=1 fsync=0 nvme=1 drv=flush]
```

Published to `/kv/world/hw_flush`. Verified live on this host.
