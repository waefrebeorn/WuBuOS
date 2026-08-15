# SAMPLERATE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio sample rate/format gaps

PCM width (S16/S24/S32/float) + sample rate define the audio stream.

### Format routing (wubu_samplerate.c)

| Format | Routing |
|--------|---------|
| float | `float` |
| s24 | `s24` |
| s32 | `s32` |
| s16 | `s16` |
| u8 | `u8` |
| else | `s16` |

### Rate routing

| Rate | Routing |
|------|---------|
| 192 / 384 / 176 | `high-res` |
| 96 / 88 | `high-rate` |
| 48 | `48k` |
| 44 | `44.1k` |
| else | `48k` |

### Kernel summary

```
samplerate[sr=0 pcm=0 float=0 24bit=0 hi=0 drv=none]
```

Published to `/kv/world/hw_samplerate`. (No snd_pcm on WSL2.)
