# VRAM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU VRAM gaps

VRAM is dedicated graphics memory; framebuffer holds scanout buffer.

### VRAM routing (wubu_vram.c)

| Pool | Routing |
|------|---------|
| stolen | `stolen` (Intel) |
| ttm | `ttm` (TTM BO pool) |
| vram | `vram` |
| fb | `framebuffer` |
| unknown | `vram` (fallback) |

| Alloc hint | Routing |
|-----------|---------|
| gpu | `gpu-domain` |
| cpu | `cpu-domain` |
| wc | `write-combining` |
| uc | `uncached` |
| wb | `write-back` |

### Kernel summary

```
vram[vram=0 fb=0 stolen=0 ttm=0 drm_mm=0 drv=none]
```

Published to `/kv/world/hw_vram`. (No discrete GPU + no /dev/dgx on WSL2.)
