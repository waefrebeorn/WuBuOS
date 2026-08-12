# PCMLINK-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio PCM link routing gaps

PCM link routes audio sample streams between ALSA PCM devices and
ASoC codec links on every sound card topology.

### Impl routing (wubu_pcmlink.c)

| Route | Path |
|-------|------|
| PCM capture link  | /proc/asound card pcm0c/sub0/status |
| PCM playback link | /proc/asound card pcm0p/sub0/status |
| PCM hw params      | hw_params |

Channel detection: 8ch, 6ch, 2ch, 1ch. State: active/idle.
Link direction: capture/playback.
