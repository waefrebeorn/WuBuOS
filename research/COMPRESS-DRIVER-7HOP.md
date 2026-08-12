# COMPRESS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux storage compression gaps

Transparent compression compresses at write, decompresses at read.

### Algo routing (wubu_compress.c)

| Algorithm | Routing |
|-----------|---------|
| zstd | `zstd` |
| lz4 | `lz4` |
| lzo | `lzo` |
| zlib | `zlib` |
| gzip | `gzip` |
| else | `none` |

### Mode routing

| Mode | Routing |
|------|---------|
| transparent | `transparent` |
| force | `force` |
| zlib | `zlib` |
| else | `auto` |

### Kernel summary

```
compress[compress=0 btrfs=0 zfs=0 zstd=0 lz4=0 drv=none]
```

Published to `/kv/world/hw_compress`. (No btrfs/ZFS on WSL2.)
