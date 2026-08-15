# PCMRING-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio PCM ring buffer gaps

PCM ring buffers queue audio samples between userspace and hardware,
with buffer/period sizing determining latency.

### Impl routing (wubu_pcmring.c)

| Route | Path |
|-------|------|
| PCM state      | /proc/asound/card*/pcm*/sub*/status |
| Hw params      | /proc/asound/card*/pcm*/sub*/hw_params |
| Sw params      | /proc/asound/card*/pcm*/sub*/sw_params |
| HDA parameters | /sys/module/snd_hda_core/parameters |

Formats: S32_LE, S24_3LE, S24_LE, S16_LE, FLOAT, S32, S24, S16, U8.
Latency = buffer_size / (rate/1000ms).
