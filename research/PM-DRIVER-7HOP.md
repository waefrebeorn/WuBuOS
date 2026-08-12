# PM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux power-mode (S0ix/sleep/runtime PM) gaps

Power modes are how the machine sleeps/wakes. WuBuOS owns the sleep-state
+ idle routing.

### Sleep states (wubu_pm.c)
- S0ix / s2idle: modern x86 low-power idle (Intel/AMD)
- S3 (suspend to RAM), S4 (hibernate), S5 (shutdown)
- Runtime PM: per-device autosuspend
- cpuidle: C-states (intel_idle, acpi_idle)

### CPU idle routing

| CPU | Driver |
|-----|--------|
| Intel | `intel_idle` |
| AMD | `acpi_idle` |
| ARM | `cpuidle-arm` |

### Kernel summary line

```
pm[s0ix=0 s3=1 s4=1 runtime=1 cpuidle=1(cpuidle) drv=runtime-pm]
```

Published to `/kv/world/hw_pm` by `wubu_pm_summary()`.

**Verified live:** this host reports `s3=1 s4=1 runtime=1` — real sleep
states detected via /sys/power/state.
