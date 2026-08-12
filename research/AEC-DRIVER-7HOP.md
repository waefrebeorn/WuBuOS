# AEC-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio AEC + noise suppression gaps

AEC removes echo; NS removes background noise from mic capture.

### AEC routing (wubu_aec.c)

| Engine | Role |
|--------|------|
| WebRTC APM | echo, NS, gain control |
| PipeWire | aec-method |
| PulseAudio | module-echo-cancel |
| ALSA | dmix + dsnoop |

| Algorithm | Routing |
|-----------|---------|
| webrtc | `webrtc` |
| speex | `speex` |
| rnnoise | `rnnoise` |
| ooura | `ooura` |
| unknown | `webrtc` (fallback) |

| Aggression | Routing |
|------------|---------|
| aggressive | `aggressive` |
| moderate | `moderate` |
| light | `light` |
| unknown | `moderate` (fallback) |

### Kernel summary

```
aec[aec=0 ns=0 webrtc=0 pw=0 pa=0 drv=none]
```

Published to `/kv/world/hw_aec`. (PipeWire/PulseAudio not on this host.)
