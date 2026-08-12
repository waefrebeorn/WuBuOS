# NS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NVMe namespace/multipath gaps

NVMe namespaces are logical volumes; multipath provides failover.

### Namespace routing (wubu_ns.c)

| Component | Role |
|-----------|------|
| nvme | /sys/class/nvme |
| namespace | /sys/class/nvme/*/namespaces |
| nvme-multipath | dm failover |
| nvme-cli | nvme list-ns, id-ns |

### Path routing (ANA)

| Path | Routing |
|------|---------|
| primary/optimized | `primary` |
| secondary/non-opt | `secondary` |
| multipath | `multipath` |

### State routing

| State | Routing |
|-------|---------|
| live | `live` |
| offline | `offline` |
| read-only | `read-only` |

### Kernel summary line

```
ns[nvme=1 ns=0 mpath=0 ana=0 cli=0 drv=nvme]
```

Published to `/kv/world/hw_ns` by `wubu_ns_summary()`.

**Verified live:** this host reports `nvme=1`.
