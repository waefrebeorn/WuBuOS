# VLANAUDIO-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux NIC VLAN + audio DSP gaps

Two capabilities: network VLAN segmentation + the audio processing graph.

### VLAN (wubu_vlanaudio.c)
- 802.1Q: `vlan.ko` / 8021q module, /proc/net/vlan
- NIC offload: rx/tx vlan offload (ethtool -k)
- bridge VLAN filtering: br_vlan
- VLAN routing: all capable NICs -> `8021q`

### Audio DSP
- **PipeWire**: the modern audio graph (pw-cli), filters + EQ
- **ALSA**: asound, dmix software mixer
- **SOF** (Sound Open Firmware): DSP offload, equalizer/beamforming

| DSP | Driver |
|-----|--------|
| PipeWire | `pipewire` |
| SOF | `snd_sof` |
| ALSA mixer | `alsa-dmix` |
| PulseAudio | `pulseaudio` |

### Kernel summary line

```
vlanaudio[vlan=0 offload=1 pipewire=0 alsa=0 sof=0 dsp=alsa]
```

Published to `/kv/world/hw_vlanaudio` by `wubu_vlanaudio_summary()`.
