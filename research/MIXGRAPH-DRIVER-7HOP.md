# MIXGRAPH-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio mixing graph gaps

The audio graph routes apps/streams/effects to output. PipeWire fixed 20
years of ALSA/Pulse/JACK chaos.

### Graph driver routing (wubu_mixgraph.c)

| Graph | Driver |
|-------|--------|
| PipeWire | `pipewire` |
| WirePlumber | `wireplumber` |
| PulseAudio | `pulseaudio` |
| JACK | `jack` |
| ALSA mixer | `alsa-dmix` |

### Components
- PipeWire + WirePlumber: modern graph + session manager
- PulseAudio: legacy sink/source graph
- JACK: pro-audio real-time ports
- ALSA dmix: software mixing

### Kernel summary line

```
mixgraph[pw=0 pulse=0 jack=1 alsa=1 wplumber=0 drv=jack]
```

Published to `/kv/world/hw_mixgraph` by `wubu_mixgraph_summary()`.

**Verified live:** this host reports `jack=1 alsa=1`.
