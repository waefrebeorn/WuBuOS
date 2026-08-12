# MIXERGRAPH-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio mixer graph gaps

Mixer graph connects audio paths (source→sink edges, control groups).

### Path routing (wubu_mixergraph.c)

| Path | Routing |
|------|---------|
| pb / play | `playback` |
| cap / capt / record | `capture` |
| mon / monitor | `monitor` |
| loopback / be | `loopback` |
| else | `playback` |

### Group routing

| Group | Routing |
|-------|---------|
| master | `Master` |
| pcm | `PCM` |
| cd | `CD` |
| mic | `Mic` |
| line | `Line` |
| ig / be | `Beep` |
| else | `Master` |

### Kernel summary

```
mixergraph[mix=0 pb=0 cap=0 mon=0 groups=0 drv=none]
```

Published to `/kv/world/hw_mixergraph`. (No /proc/asound on WSL2.)
