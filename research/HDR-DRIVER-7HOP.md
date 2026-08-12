# HDR-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display HDR + audio jack detection gaps

Two capabilities: HDR metadata on the display link + audio jack detection.

### HDR metadata routing (wubu_hdr.c)

| Format | Routing |
|--------|---------|
| HDR10 (ST 2086) | `hdr10` |
| HDR10+ (SMPTE 2094-40) | `hdr10plus` |
| Dolby Vision | `dolby-vision` |
| HLG | `hlg` |
| SDR | `sdr` |

DRM hdr_output_metadata: static (HDR10) + dynamic (HDR10+) metadata blob.

### Jack detection routing

| Jack | Routing |
|------|---------|
| Headphone | `hda-headphone` |
| Mic | `hda-mic` |
| HDMI | `hda-hdmi` |
| ASoC | `asoc-jack` |
| Line-in | `hda-linein` |

ASoC snd_soc_jack + HDA jack detection; /proc/asound codec state.

### Kernel summary line

```
hdr[hdr10=1 hdr10p=1 dv=0 sink=1 jack=0 drv=hdr10]
```

Published to `/kv/world/hw_hdr` by `wubu_hdr_summary()`.

**Verified live:** this host reports `hdr10=1 hdr10p=1 sink=1`.
