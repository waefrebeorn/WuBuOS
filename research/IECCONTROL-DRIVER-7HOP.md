# IECCONTROL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux IEC 60958 control gaps

IEC control (AES bits, encoding, clock) manages S/PDIF digital audio passthrough.

### Encoding routing (wubu_ieccontrol.c)

| Encoding | Routing |
|----------|---------|
| consumer / pcm | `consumer` |
| pro | `professional` |
| broadcast | `broadcast` |
| ac3 | `ac3` |
| dts | `dts` |
| else | `consumer` |

### Clock routing

| Clock | Routing |
|-------|---------|
| ext | `external` |
| int | `internal` |
| master | `master` |
| slave | `slave` |
| else | `internal` |

### Kernel summary

```
ieccontrol[iec=0 aes=0 enc=0 clock=0 rate=0 drv=none]
```

Published to `/kv/world/hw_ieccontrol`. (No /proc/asound on WSL2.)
