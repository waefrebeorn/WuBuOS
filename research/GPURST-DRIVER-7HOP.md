# GPURST-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU reset/recovery gaps

GPU reset recovers from a hung GPU after a timeout, re-init the ring.

### Stage routing (wubu_gpurst.c)

| Stage | Routing |
|-------|---------|
| pre / stop | `pre-reset` |
| reset | `reset` |
| post / resume | `post-reset` |
| fault | `fault` |
| else | `idle` |

### Ring routing

| Ring | Routing |
|------|---------|
| gfx / 3d | `gfx` |
| compute | `compute` |
| dma | `dma` |
| video | `video` |
| else | `gfx` |

### Kernel summary

```
gpurst[reset=0 ring=0 hb=0 timeout=0 recover=0 drv=none]
```

Published to `/kv/world/hw_gpurst`. (No amdgpu/i915 module on WSL2.)
