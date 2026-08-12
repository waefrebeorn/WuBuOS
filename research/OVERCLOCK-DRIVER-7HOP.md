# OVERCLOCK-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU overclock gaps

GPU overclocking raises core/memory clocks past stock via overdrive/sysfs.

### Clock routing (wubu_overclock.c)

| Clock | Routing |
|-------|---------|
| core / sclk / gt | `core` |
| mem / mclk | `memory` |
| vddc | `vddc` |
| soc | `soc` |
| else | `core` |

### State routing

| State | Routing |
|-------|---------|
| stock | `stock` |
| boot | `boot` |
| stable | `stable` |
| oc | `overclock` |
| else | `stock` |

### Kernel summary

```
overclock[oc=0 od=0 sysfs=0 core=0 mem=0 drv=none]
```

Published to `/kv/world/hw_overclock`. (No amdgpu/i915 on WSL2.)
