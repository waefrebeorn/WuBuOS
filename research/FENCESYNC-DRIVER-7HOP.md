# FENCESYNC-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU sync fence gaps

Sync fences (DMA fence, timeline) coordinate GPU-CPU command ordering.

### Type routing (wubu_fencesync.c)

| Type | Routing |
|------|---------|
| sdma | `sdma-fence` |
| seqno | `seqno` |
| sync / fd | `sync-fd` |
| dma | `dma-fence` |
| else | `dma-fence` |

### Op routing

| Op | Routing |
|----|---------|
| wait | `wait` |
| signal | `signal` |
| timeline | `timeline` |
| timeout | `timeout` |
| else | `wait` |

### Kernel summary

```
fencesync[fence=0 timeline=0 wait=0 timeout=0 signal=0 drv=none]
```

Published to `/kv/world/hw_fencesync`. (No drm/amdgpu/i915 on WSL2.)
