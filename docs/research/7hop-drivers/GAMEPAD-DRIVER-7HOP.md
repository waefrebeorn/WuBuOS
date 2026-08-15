# GAMEPAD-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux game controller + display DSC gaps

Two capabilities: game controllers (wheels/arcade) + display compression.

### Controller routing (wubu_gamepad.c)

| Device | Driver |
|--------|--------|
| Xbox | `xpad` |
| PlayStation (DualSense) | `hid-playstation` |
| Nintendo Switch | `hid-nintendo` |
| Steam Controller | `hid-steam` |
| Logitech G29/G27 wheel | `g29_ff` |
| Thrustmaster wheel | `hid-tmff` |
| Generic | `uinput` |

### DSC (Display Stream Compression) routing

| GPU | Driver |
|-----|--------|
| Intel i915 | `i915-dsc` |
| AMD amdgpu | `amdgpu-dsc` |
| NVIDIA nouveau | `nouveau-dsc` |

DSC 1.2: eDP/DP 1.4, 4K@120Hz, 8K@60Hz.

### Kernel summary line

```
gamepad[pad=0 wheel=0 arcade=0(none) dsc=0(none)]
```

Published to `/kv/world/hw_gamepad` by `wubu_gamepad_summary()`.
