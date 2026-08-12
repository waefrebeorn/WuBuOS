# COMPUTE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux graphics compute (OpenCL/Vulkan) gaps

GPU compute is the AGI workhorse. WuBuOS routes the compute stack + ICD.

### Compute routing (wubu_compute.c)

| GPU | Driver |
|-----|--------|
| AMD | `rusticl` |
| Intel | `rusticl` |
| NVIDIA | `cuda` |
| CUDA-on-Vulkan | `zlu` |
| CPU/other | `pocl` |

### Stacks
- **rusticl** (mesa): unified OpenCL-on-Vulkan driver
- **ROCm**: AMD OpenCL/HIP
- **CUDA**: NVIDIA proprietary; **ZLUDA**: CUDA→Vulkan translation
- **radv/ANV/lavapipe**: Vulkan compute ICDs
- **pocl**: portable OpenCL (CPU + accelerators)

### Kernel summary line

```
compute[opencl=1 vulkan=0 cuda=1 rusticl=0 drv=cuda vendor=NVIDIA]
```

Published to `/kv/world/hw_compute` by `wubu_compute_summary()`.

**Verified live:** this host reports `opencl=1 cuda=1` — real compute stack.
