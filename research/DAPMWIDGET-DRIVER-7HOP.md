# DAPMWIDGET-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio DAPM widget gaps

DAPM (Dynamic Audio Power Management) widgets are audio path nodes.

### Widget type routing (wubu_dapmwidget.c)

| Type | Routing |
|------|---------|
| adc | `adc` |
| dac | `dac` |
| mix | `mixer` |
| mux | `mux` |
| pga | `pga` |
| sw/switch | `switch` |
| else | `adc` |

### Power routing

| Power | Routing |
|-------|---------|
| on / power | `on` |
| off | `off` |
| suspend | `suspend` |
| else | `off` |

### Kernel summary

```
dapmwidget[dapm=0 widget=0 power=0 path=0 stream=0 drv=none]
```

Published to `/kv/world/hw_dapmwidget`. (No snd_soc_core on WSL2.)
