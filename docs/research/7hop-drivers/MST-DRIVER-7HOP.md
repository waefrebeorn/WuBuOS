# MST-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux DisplayPort MST + audio SRC gaps

Two capabilities: DP MST (multi-stream) + audio sample-rate conversion.

### MST payload routing (wubu_mst.c)

| Mode | Routing |
|------|---------|
| Single stream | `single-stream` |
| Multi-stream (MST) | `multi-stream` |
| DSC compressed | `dsc-compressed` |

Components: drm dp_mst topology manager, VC payload allocation, DSC over
MST, DP 1.2+ MST hubs.

### Audio SRC routing

| Rate | Routing |
|------|---------|
| 44.1kHz | `44100-src` |
| 48kHz | `48000-src` |
| 96kHz | `96000-src` |
| 192kHz | `192000-src` |
| best quality | `src-best-quality` |

Components: PipeWire resample, ALSA dmix/SRC rate conversion.

### Kernel summary line

```
mst[dp=1 top=0 dsc=0 src=1 resample=0 drv=dp-mst]
```

Published to `/kv/world/hw_mst` by `wubu_mst_summary()`.

**Verified live:** this host reports `dp=1 src=1`.
