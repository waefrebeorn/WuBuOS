# VRR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display VRR + spatial audio gaps

Two capabilities: variable refresh rate + spatial audio.

### VRR routing (wubu_vrr.c)

| GPU | Driver |
|-----|--------|
| AMD | `amdgpu-vrr` |
| Intel i915 | `i915-vrr` |
| NVIDIA nouveau | `nouveau-vrr` |

VRR = FreeSync (AMD), Adaptive-Sync (Intel/DP 1.2a), G-Sync (NVIDIA).

### Spatial audio routing

| Audio | Driver |
|-------|--------|
| Dolby Atmos | `dolby-atmos` |
| PipeWire spatial | `pipewire-spatial` |
| EasyEffects | `easyeffects` |
| HRTF/binaural | `hrtf` |
| ALSA spatial | `alsa-spatial` |

### Kernel summary line

```
vrr[vrr=0(none) freesync=0 adaptive=0 spatial=0(none) atmos=0]
```

Published to `/kv/world/hw_vrr` by `wubu_vrr_summary()`.
