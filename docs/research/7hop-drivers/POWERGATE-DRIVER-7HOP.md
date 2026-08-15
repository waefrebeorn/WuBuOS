# POWERGATE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU power gating gaps

Power gating cuts power to GPU blocks (shader/texture/L2) during idle.

### Domain routing (wubu_powergate.c)

| Domain | Routing |
|--------|---------|
| shader | `shader` |
| texture | `texture` |
| l2 | `l2` |
| mcv | `mcv` |
| skep | `skep` |
| else | `shader` |

### State routing

| State | Routing |
|-------|---------|
| on / active | `on` |
| off / gated | `gated` |
| suspend | `suspend` |
| else | `on` |

### Kernel summary

```
powergate[pg=0 runtime=0 shader=0 texture=0 l2=0 drv=none]
```

Published to `/kv/world/hw_powergate`. (No amdgpu/i915 on WSL2.)
