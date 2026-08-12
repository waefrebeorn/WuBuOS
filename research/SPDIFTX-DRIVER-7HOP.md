# SPDIFTX-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux SPDIF TX control gaps

SPDIF TX control manages S/PDIF optical/coax output encoding (PCM/AC3/DTS).

### Encoding routing (wubu_spdiftx.c)

| Encoding | Routing |
|----------|---------|
| ac3 / eac3 | `ac3` |
| dts | `dts` |
| pcm / lpcm | `pcm` |
| else | `pcm` |

### Media routing

| Media | Routing |
|-------|---------|
| opt | `optical` |
| coax | `coax` |
| arc | `arc` |
| hdmi | `hdmi-arc` |
| else | `optical` |

### Kernel summary

```
spdiftx[tx=0 iec=0 ac3=0 dts=0 optical=0 drv=none]
```

Published to `/kv/world/hw_spdiftx`. (No /proc/asound on WSL2.)
