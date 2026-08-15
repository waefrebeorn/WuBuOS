# GPUSCHED-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU compute scheduler gaps

The GPU scheduler manages command submission priority + fairness between
contexts (3D, compute, video).

### GPU scheduler routing (wubu_gpusched.c)

| Component | Role |
|-----------|------|
| drm-sched | job submission + priority |
| i915 GuC | firmware command submission + preemption |
| amdgpu scheduler | GPU scheduler + CP |
| nvkm | NVIDIA scheduler (prio bands) |
| priority | high > normal > low |

### Priority routing

| Priority | Routing |
|----------|---------|
| high | `high` |
| normal | `normal` |
| low | `low` |

### Class routing

| Class | Routing |
|-------|---------|
| 3D | `3d` |
| compute | `compute` |
| video | `video` |
| copy | `copy` |

### Kernel summary line

```
gpusched[sched=0 guc=0 prio=0 preempt=0 fair=0 drv=none]
```

Published to `/kv/world/hw_gpusched` by `wubu_gpusched_summary()`.
