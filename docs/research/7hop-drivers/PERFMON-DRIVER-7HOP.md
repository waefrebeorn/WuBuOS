# PERFMON-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU perf counters gaps

Perf counters (AMD GPUPerfAPI, Intel DRM perf) measure GPU metrics.

### Metric routing (wubu_perfmon.c)

| Metric | Routing |
|--------|---------|
| cycle | `cycles` |
| instr | `instructions` |
| cache | `cache-hits` |
| occup | `occupancy` |
| mem | `mem-bandwidth` |
| else | `cycles` |

### API routing

| API | Routing |
|-----|---------|
| amd | `gpuprofa` |
| intel | `i915` |
| nv | `nvml` |
| else | `gpuprof` |

### Kernel summary

```
perfmon[pm=0 event=0 cycles=0 cache=0 occ=0 drv=none]
```

Published to `/kv/world/hw_perfmon`. (No drm/amdgpu/i915 on WSL2.)
