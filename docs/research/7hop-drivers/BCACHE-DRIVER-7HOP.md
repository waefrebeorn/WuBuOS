# BCACHE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage bcache gaps

bcache provides SSD/HDD hybrid caching via the bcache subsystem,
accelerating rotational storage with flash cache.

### Impl routing (wubu_bcache.c)

| Route | Path |
|-------|------|
| Registration    | /sys/fs/bcache |
| Cache mode        | /sys/block/bcache*/bcache/cache_mode |
| Cached devices    | /sys/block/bcache*/bcache/cached_devs |
| Cache hit ratio   | /sys/block/bcache*/bcache/cache_hit_ratio |

Modes: writeback, writethrough, writearound, none.
Hit pct = hits * 100 / (hits + misses).
