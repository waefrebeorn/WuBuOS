# DSPGRAPH-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio DSP graph gaps

DSP graph is the ALSA snd_soc_dapm routing (source->path->sink).

### Widget routing (wubu_dspgraph.c)

| Widget | Routing |
|--------|---------|
| mixer | `mixer` |
| dac | `dac` |
| adc | `adc` |
| mux | `mux` |
| pin | `pin` |
| switch | `switch` |
| else | `widget` |

### Path routing

| Path | Routing |
|------|---------|
| up | `up` |
| down | `down` |
| direct | `direct` |
| muted | `muted` |
| else | `direct` |

### Kernel summary

```
dspgraph[graph=0 dapm=0 widget=0 path=0 route=0 drv=none]
```

Published to `/kv/world/hw_dspgraph`. (No snd_soc_core on WSL2.)
