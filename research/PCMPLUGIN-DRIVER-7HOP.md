# PCMPLUGIN-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio PCM plugin gaps

PCM plugins (ALSA plugin chain) transform audio samples at runtime.

### Plugin type routing (wubu_pcmplugin.c)

| Plugin | Routing |
|--------|---------|
| rate / resample | `rate` |
| vol / softvol | `vol` |
| copy | `copy` |
| plug / route | `plug` |
| dmix | `dmix` |
| else | `plug` |

### Chain routing

| Chain element | Routing |
|---------------|---------|
| rate | `rate` |
| vol | `vol` |
| copy | `copy` |
| plug | `plug` |
| dmix | `dmix` |

### Kernel summary

```
pcmplugin[plug=0 rate=0 vol=0 copy=0 plugtype=0 drv=none]
```

Published to `/kv/world/hw_pcmplugin`. (No /proc/asound on WSL2.)
