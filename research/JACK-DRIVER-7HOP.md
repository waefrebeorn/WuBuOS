# JACK-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio jack detection gaps

Audio jack detection monitors plug state for headphones, microphones,
and SPDIF inputs via ALSA jack reporting infrastructure.

### Impl routing (wubu_jack.c)

| wubu fn | source |
|---|---|
| wubu_jack_present | /proc/asound jack state |
| wubu_jack_type_for | jack plug/unplug state |
| wubu_jack_state_for | jack presence + impedance |
| wubu_jack_summary | jack detection summary |
