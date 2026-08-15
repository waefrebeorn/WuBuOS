# NFSMOUNT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NFS mount gaps

NFS mount routes remote filesystems (vers 2/3/4.0/4.1/4.2).

### Version routing (wubu_nfsmount.c)

| Version | Routing |
|---------|---------|
| 4.2 | `4.2` |
| 4.1 / 4,1 | `4.1` |
| 4.0 / 4,0 / 4 | `4.0` |
| 3 | `3` |
| 2 | `2` |
| else | `4.0` |

### Option routing

| Option | Routing |
|--------|---------|
| rsize | `rsize` |
| wsize | `wsize` |
| timeo | `timeo` |
| intr | `intr` |
| hard | `hard` |
| soft | `soft` |
| ac | `attr-cache` |
| else | `rsize` |

### Kernel summary

```
nfsmount[nfs=0 mount=0 vers=0 rsize=0 wsize=0 drv=none]
```

Published to `/kv/world/hw_nfsmount`.
