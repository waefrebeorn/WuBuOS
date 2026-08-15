# CODECGRAPH-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio codec graph gaps

Codec graph = ALSA widget tree (pin, ADC, DAC, mixer, amp).

### Widget routing (wubu_codecgraph.c)

| Widget | Routing |
|--------|---------|
| pin | `pin-complex` |
| adc | `adc` |
| dac | `dac` |
| mixer | `mixer` |
| selector | `selector` |
| unknown | `widget` (fallback) |

### Verb routing

| Verb | Routing |
|------|---------|
| pin | `SET_PIN_WIDGET` |
| amp | `SET_AMP_GAIN` |
| gpio | `SET_GPIO` |

### Kernel summary

```
codecgraph[codec=0 graph=0 amp=0 widgets=0 dapm=0 drv=snd-hda]
```

Published to `/kv/world/hw_codecgraph`. (No Intel HDA on WSL2.)
