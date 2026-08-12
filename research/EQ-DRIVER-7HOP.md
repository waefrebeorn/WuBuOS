# EQ-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio equalizer DSP coefficients gaps

EQ shapes frequency response via filter coefficients (biquad b0-b2, a1-a2).

### EQ driver routing (wubu_eq.c)

| Engine | Driver |
|--------|--------|
| ALSA hw EQ | `alsa-eq` |
| PipeWire/EasyEffects | `pw-eq` |
| PulseAudio | `pulse-eq` |
| SOF DSP | `sof-dsp` |
| Loudness/DRC | `loudness-drc` |

### Components
- biquad: 2nd-order IIR (b0-b2, a1-a2 coefficients)
- ALSA controls: hardware codec EQ (ALC, cs35l41)
- EasyEffects: software parametric EQ
- SOF: firmware coefficient programming

### Kernel summary line

```
eq[alsa=0 sw=0 dsp=0 biquad=0 loudness=0 drv=none]
```

Published to `/kv/world/hw_eq` by `wubu_eq_summary()`.
