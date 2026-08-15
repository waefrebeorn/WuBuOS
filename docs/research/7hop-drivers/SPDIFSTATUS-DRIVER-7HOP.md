# SPDIFSTATUS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux SPDIF status gaps

SPDIF status senses receiver lock, sample rate, AES bits on S/PDIF.

### Rate routing (wubu_spdifstatus.c)

| Rate | Routing |
|------|---------|
| 44.1 / 44100 | `44.1kHz` |
| 48 / 48000 | `48kHz` |
| 96 / 96000 | `96kHz` |
| 192 / 192000 | `192kHz` |
| 32 / 32000 | `32kHz` |
| 88.2 / 88200 | `88.2kHz` |
| else | `48kHz` |

### Lock routing

| Lock | Routing |
|------|---------|
| lock / detect | `locked` |
| unl / nol | `unlocked` |
| valid | `valid` |
| else | `unlocked` |

### Kernel summary

```
spdifstatus[sp=0 lock=0 valid=0 aes=0 rate=0 drv=none]
```

Published to `/kv/world/hw_spdifstatus`. (No /proc/asound on WSL2.)
