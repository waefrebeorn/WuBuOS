# RAIDCACHE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage RAID cache gaps

SSD caching of spinning disks (fast tier of slower) speeds up storage.

### Cache driver routing (wubu_raidcache.c)

| Cache | Driver |
|-------|--------|
| dm-cache | `dm-cache` |
| bcache | `bcache` |
| zram | `zram` |
| raid5-cache | `raid5-cache` |
| lvm-cache | `lvm-cache` |

### Components
- dm-cache: device-mapper cache (SSD + HDD, writeback/writethrough)
- bcache: /sys/fs/bcache, writeback caching
- raid5-cache: md RAID journal
- zram: compressed RAM swap

### Kernel summary line

```
raidcache[cache=0 dm-cache=0 bcache=0 zram=0 raid-jnl=0 drv=none]
```

Published to `/kv/world/hw_raidcache` by `wubu_raidcache_summary()`.
