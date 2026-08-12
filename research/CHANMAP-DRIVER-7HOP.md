# CHANMAP-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio channel map gaps

Channel maps assign ALSA channel positions (FL, FR, C, LFE) to outputs.

### Position routing (wubu_chanmap.c)

| Pos | Routing |
|-----|---------|
| fl | `front-left` |
| fr | `front-right` |
| fc | `front-center` |
| lfe | `lfe` |
| sl/sur | `surround-left` |
| sr | `surround-right` |
| else | `front-left` |

### Layout routing

| Layout | Routing |
|--------|---------|
| mono | `mono` |
| stereo | `stereo` |
| 5.1 | `5.1` |
| 7.1 | `7.1` |
| else | `stereo` |

### Kernel summary

```
chanmap[map=0 stereo=0 51=0 71=0 chmap=0 drv=none]
```

Published to `/kv/world/hw_chanmap`. (No /proc/asound on WSL2.)
