# JACKDETECT-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio jack detection gaps

Jack detection senses headset/mic insertion (TRRS pinout).

### Pinout routing (wubu_jackdetect.c)

| Pinout | Routing |
|--------|---------|
| omtp | `omtp` |
| ctia | `ctia` |
| ymck | `ymck` |
| else | `ctia` |

### State routing

| State | Routing |
|-------|---------|
| insert/plug | `inserted` |
| remov/unplug | `removed` |
| mic | `mic-present` |
| no-mic | `no-mic` |
| else | `removed` |

### Kernel summary

```
jackdetect[jack=0 headset=0 mic=0 omtp=0 ctia=0 drv=none]
```

Published to `/kv/world/hw_jackdetect`. (No /proc/asound on WSL2.)
