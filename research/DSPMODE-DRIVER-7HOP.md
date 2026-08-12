# DSPMODE-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio codec DSP modes gaps

Codec DSPs (SOF + HD-Audio) have power/wake modes. WuBuOS routes them.

### DSP modes (wubu_dspmode.c)

| Mode | Routing |
|------|---------|
| Active | full DSP (EQ, effects, beamforming) |
| Low power | reduced processing during idle |
| Voice trigger | wake-word during S0ix |
| Suspend | codec D3/S3 hooks |

### DSP driver
- SOF (Sound Open Firmware): `snd_sof_pci`, DSP power states
- HD-Audio DSP: `snd_hda_intel` + codec D3
- Voice wake: SOF hotword (wake-word in S0ix)

### Kernel summary line

```
dspmode[sof=0(none) pm=0 voice_wake=0 suspend=1 mode=active]
```

Published to `/kv/world/hw_dspmode` by `wubu_dspmode_summary()`.
