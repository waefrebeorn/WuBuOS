# MEMMGR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU memory manager gaps

GPU MM (GEM/TTM) allocates VRAM/GTT. Memory manager for GPU buffer allocation.

### Heap routing (wubu_memmgr.c)

| Heap | Routing |
|------|---------|
| vram | `vram` |
| gtt | `gtt` |
| shared/shm | `shared` |
| stolen | `stolen` |
| else | `vram` |

### Type routing

| Type | Routing |
|------|---------|
| gem | `gem` |
| ttm | `ttm` |
| bo | `bo` |
| else | `gem` |

### Kernel summary

```
memmgr[mm=0 gem=0 ttm=0 vram=0 gtt=0 drv=none]
```

Published to `/kv/world/hw_memmgr`. (No drm/amdgpu/i915 on WSL2.)
