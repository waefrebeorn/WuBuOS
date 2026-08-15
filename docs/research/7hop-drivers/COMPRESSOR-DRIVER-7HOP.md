# COMPRESSOR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio compressor gaps

Compressor/limiter (ALSA DSP dynamics) shapes audio amplitude.

### Ratio routing (wubu_compressor.c)

| Ratio | Routing |
|-------|---------|
| 2:1 | `2:1` |
| 3:1 | `3:1` |
| 4:1 | `4:1` |
| 8:1 | `8:1` |
| inf / limit | `limiter` |
| 1:1 | `1:1` |
| else | `4:1` |

### Knee routing

| Knee | Routing |
|------|---------|
| hard | `hard` |
| soft | `soft` |
| medium | `medium` |
| else | `soft` |

### Kernel summary

```
compressor[comp=0 thresh=0 ratio=0 attack=0 release=0 drv=none]
```

Published to `/kv/world/hw_compressor`. (No /proc/asound on WSL2.)
