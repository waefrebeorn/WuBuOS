# CODEC-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio codec/DSP driver gaps

The codec is the chip turning digital audio into analog (and back). WuBuOS
routes HD-Audio codecs + ASoC codecs + the SOF DSP.

### HD-Audio codec routing (wubu_codec.c)

| Vendor | Driver |
|--------|--------|
| Realtek (ALC family) | `snd_hda_codec_realtek` |
| IDT/Sigmatel | `snd_hda_codec_idt` |
| Cirrus | `snd_hda_codec_cirrus` |
| Conexant | `snd_hda_codec_conexant` |
| Analog Devices | `snd_hda_codec_analog` |
| HDMI (ATI/NVIDIA/Intel) | `snd_hda_codec_hdmi` |

### ASoC codec routing

| Chip | Driver |
|------|--------|
| WM8960 | `snd_soc_wm8960` |
| CS42L42 | `snd_soc_cs42l42` |
| RT5682 | `snd_soc_rt5682` |
| NAU8825 | `snd_soc_nau8825` |
| MAX98357A (amp) | `snd_soc_max98357a` |
| TAS2770 (amp) | `snd_soc_tas2770` |

### DSP
SOF (Sound Open Firmware) offloads audio processing via `snd_sof_pci`.
WuBuOS flags `wubu_codec_has_sof_dsp()`.

### Kernel summary line

```
codec[present=0 hda=0 asoc=0 sof=0 drv=none name=-]
```

Published to `/kv/world/hw_codec` by `wubu_codec_summary()`.
