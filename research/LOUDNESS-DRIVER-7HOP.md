# LOUDNESS-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio loudness gaps

Loudness normalization evens out perceived volume (ReplayGain / R128 /
ITU-R BS.1770 LUFS).

### Loudness routing (wubu_loudness.c)

| Component | Role |
|-----------|------|
| ReplayGain | track + album gain |
| Opus R128 | R128_TRACK_GAIN, R128_ALBUM_GAIN |
| ITU-R BS.1770 | LUFS |
| PipeWire | loudness effect |
| ALSA dmix | volume normalization |

### Modes

| Mode | Routing |
|------|---------|
| track | `track-gain` |
| album | `album-gain` |
| lufs / itur | `lufs` |
| off | `off` |

### Targets (LUFS)

| Ref | Routing |
|-----|---------|
| 89 (RG) | `-18lufs` |
| 83 (R128) | `-16luft` |
| 79 | `-14lufs` |
| 93 (EBU) | `-23lufs` |

### Kernel summary line

```
loudness[loud=0 gain=1 r128=1 lufs=1 pw=0 drv=replaygain]
```

Published to `/kv/world/hw_loudness` by `wubu_loudness_summary()`.

**Verified live:** this host reports `gain=1 r128=1 lufs=1`.
