# SPDIF-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux SPDIF/HDMI audio gaps

SPDIF carries raw PCM/bitstream; HDMI passthrough sends encoded audio.

### SPDIF routing (wubu_spdif.c)

| Codec | Routing |
|-------|---------|
| ac3 / eac3 | `ac3` |
| dts | `dts` |
| pcm | `pcm` |
| aac | `aac` |
| unknown | `pcm` (fallback) |

| Format | Routing |
|--------|---------|
| raw | `raw` |
| burst | `burst` |
| hbr | `hbr` |
| fbr | `fbr` |
| unknown | `raw` (fallback) |

### Kernel summary

```
spdif[spdif=0 hdmi=0 iec61937=0 passthru=0 i2s=0 drv=none]
```

Published to `/kv/world/hw_spdif`. (No Intel HDA on WSL2.)
