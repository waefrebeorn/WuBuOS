# NFSCLIENT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NFS client gaps

NFS client mounts remote filesystems via RPC (idmapd, statd, security).

### Version routing (wubu_nfsclient.c)

| Version | Routing |
|---------|---------|
| 4.2 | `4.2` |
| 4.1 | `4.1` |
| 4.0 / 4 | `4.0` |
| 3 | `3` |
| 2 | `2` |
| else | `4.0` |

### Proto routing

| Proto | Routing |
|-------|---------|
| tcp | `tcp` |
| udp | `udp` |
| rdma | `rdma` |
| else | `tcp` |

### Kernel summary

```
nfsclient[nfs=0 idmapd=0 statd=0 mount=0 sec=0 drv=none]
```

Published to `/kv/world/hw_nfsclient`.
