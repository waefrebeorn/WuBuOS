# DAX-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux DAX (Direct Access) gaps

DAX maps persistent memory directly into the page cache.

### Type routing (wubu_dax.c)

| Type | Routing |
|------|---------|
| fs / filesystem | `fs-dax` |
| dev / device | `dev-dax` |
| pmd | `pmd-dax` |
| else | `fs-dax` |

### Filesystem routing

| FS | Routing |
|----|---------|
| ext4 | `ext4` |
| xfs | `xfs` |
| btrfs | `btrfs` |
| pmem | `pmemfs` |
| else | `ext4` |

### Kernel summary

```
dax[dax=0 pmem=0 fs=0 inode=0 dev=0 drv=none]
```

Published to `/kv/world/hw_dax`. (No /dev/pmem on WSL2.)
