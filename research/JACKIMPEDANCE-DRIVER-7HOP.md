# JACKIMPEDANCE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio jack impedance gaps

Jack impedance sensing detects headphone/microphone type by impedance.

### Type routing (wubu_jackimpedance.c)

| Impedance | Routing |
|-----------|---------|
| 16 | `16-ohm` |
| 32 | `32-ohm` |
| 150 | `150-ohm` |
| 300 | `300-ohm` |
| 600 | `600-ohm` |
| high | `high-impedance` |
| low | `low-impedance` |
| else | `32-ohm` |

### Device routing

| Device | Routing |
|--------|---------|
| headphone / hp | `headphone` |
| headset / hs | `headset` |
| mic | `mic` |
| line | `line` |
| else | `headphone` |

### Kernel summary

```
jackimpedance[ji=0 headphone=0 mic=0 line=0 threshold=0 drv=none]
```

Published to `/kv/world/hw_jackimpedance`. (No /proc/asound on WSL2.)
