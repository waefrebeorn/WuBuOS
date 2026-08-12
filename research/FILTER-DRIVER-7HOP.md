# FILTER-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio DSP filter gaps

Audio DSP filters implement biquad EQ coefficients (LPF, HPF, BPF,
peaking, etc.) per the Audio EQ Cookbook.

### Filter routing (wubu_filter.c)

| Type | Routing |
|------|---------|
| lpf | `lowpass` |
| hpf | `highpass` |
| bpf | `bandpass` |
| notch | `notch` |
| peak | `peaking` |
| lowshelf | `lowshelf` |
| highshelf | `highshelf` |
| else | `biquad` |

### Biquad computation

Uses folded sin/cos pair (per `folded-polynomial-sincos` skill) +
Newton-Raphson rsqrt — no libm dependency, kernel-freestanding safe.

### Kernel summary

```
filter[filter=0 biquad=0 eq=0 pw=0 alsa=0 drv=dsp]
```

Published to `/kv/world/hw_filter`. (DSP detected, no ALSA on WSL2.)
