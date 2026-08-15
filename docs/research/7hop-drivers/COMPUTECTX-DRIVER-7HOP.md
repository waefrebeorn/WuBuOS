# COMPUTECTX-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU compute context gaps

GPU compute contexts (KFD queues, OpenCL/CUDA) manage kernel execution.

### Queue routing (wubu_computectx.c)

| Queue | Routing |
|-------|---------|
| compute / gfx | `compute` |
| dma | `dma` |
| copy | `copy` |
| sdma | `sdma` |
| else | `compute` |

### Priority routing

| Priority | Routing |
|----------|---------|
| high / rt | `high` |
| low | `low` |
| normal | `normal` |
| else | `normal` |

### Kernel summary

```
computectx[ctx=0 kfd=0 queue=0 opencl=0 cuda=0 drv=none]
```

Published to `/kv/world/hw_computectx`. (No KFD/CUDA on WSL2.)
