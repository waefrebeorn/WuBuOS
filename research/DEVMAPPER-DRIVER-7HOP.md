# DEVMAPPER-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux device mapper gaps

DM (device mapper) provides virtual block devices and targets.

### Target routing (wubu_devmapper.c)

| Target | Routing |
|--------|---------|
| linear | `linear` |
| stripe | `stripe` |
| mirror | `mirror` |
| snapshot | `snapshot` |
| thin | `thin` |
| crypt | `crypt` |
| multipath | `multipath` |
| else | `linear` |

### Mode routing

| Mode | Routing |
|------|---------|
| read-w / rw | `read-write` |
| read-only / ro | `read-only` |
| read | `read` |
| write | `write` |
| else | `read-write` |

### Kernel summary

```
devmapper[dm=0 linear=0 stripe=0 mirror=0 snapshot=0 drv=none]
```

Published to `/kv/world/hw_devmapper`. (No /dev/mapper on WSL2.)
