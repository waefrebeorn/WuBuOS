# AUDIOFW-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio firmware gaps

Audio firmware loads DSP programs onto audio codecs (Realtek, Cirrus, etc.).

### Codec routing (wubu_audiofw.c)

| Codec | Routing |
|-------|---------|
| realtek / alc | `Realtek` |
| cirrus / cs | `Cirrus` |
| wm / wmc | `WM` |
| ti / tlv320 | `TI` |
| conex | `Conexant` |
| else | `Realtek` |

### Loader routing

| Loader | Routing |
|--------|---------|
| fw | `firmware` |
| bios | `bios` |
| elf | `elf` |
| bezirk | `bezirk` |
| else | `firmware` |

### Kernel summary

```
audiofw[afw=0 codec=0 dsp=0 loader=0 bios=0 drv=none]
```

Published to `/kv/world/hw_audiofw`. (No /lib/firmware on WSL2.)
