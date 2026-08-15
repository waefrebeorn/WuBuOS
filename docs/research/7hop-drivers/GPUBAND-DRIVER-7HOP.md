# GPUBAND-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU scheduler priority bands gaps

GPU scheduler uses priority bands (high/normal/low/critical) for
command submission ordering + fairness.

### Band routing (wubu_gpuband.c)

| Priority | Routing |
|----------|---------|
| high / critical | `high` |
| low | `low` |
| kernel | `kernel` |
| else | `normal` |

| Context class | Routing |
|---------------|---------|
| 3d | `3d` |
| compute | `compute` |
| video | `video` |
| copy | `copy` |
| else | `default` |

### Kernel summary

```
gpuband[band=1 fair=1 prio=1 entity=1 stats=1 drv=drm-sched]
```

Published to `/kv/world/hw_gpuband`. (drm-sched detected on WSL2.)
