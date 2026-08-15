# GPUSENSOR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU sensor/fan curve gaps

GPU sensors monitor temperature / fan speed / power via hwmon; fan curves
map temperature to PWM duty.

### GPU-sensor routing (wubu_gpusensor.c)

| Component | Role |
|-----------|------|
| amdgpu | hwmon temp1, fan1, power1 |
| i915 | GT thermal |
| nouveau | temp + fan |
| nvml | NVIDIA GPU monitoring |
| PWM1 | fan curve control |

### Curves: aggressive / quiet / balanced / zero-rpm

### Kernel summary line

```
gpusensor[hwmon=0 temp=0 fan=0 power=0 curve=0 drv=none]
```

Published to `/kv/world/hw_gpusensor` by `wubu_gpusensor_summary()`.
