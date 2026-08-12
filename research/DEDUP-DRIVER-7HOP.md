# DEDUP-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage deduplication gaps

Deduplication removes duplicate data blocks to save storage.

### Dedup routing (wubu_dedup.c)

| Component | Role |
|-----------|------|
| dm-dedup | device-mapper dedup target |
| btrfs | built-in dedup (send/receive) |
| xfs | reflink (copy-on-write) |
| ZFS | built-in dedup (L2ARC + ARC) |
| duperemove | userspace btrfs dedup |
| bees | userspace xfs dedup |

| Mode | Routing |
|------|---------|
| inode | `inode-dedup` |
| block | `block-dedup` |
| file | `file-dedup` |
| offline | `offline` |
| online | `online` |
| unknown | `dedup` (fallback) |

| Level | Routing |
|-------|---------|
| aggressive | `aggressive` |
| conservative | `conservative` |
| none | `none` |
| unknown | `conservative` (fallback) |

### Kernel summary

```
dedup[dedup=1 dm=1 btrfs=0 xfs=1 zfs=0 drv=xfs-reflink]
```

Published to `/kv/world/hw_dedup`. Verified live on this host (xfs reflink).
