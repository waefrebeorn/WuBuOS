# DAPM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux audio DAPM routing gaps

DAPM (Dynamic Audio Power Management) optimizes audio power by routing
signals between widgets (input/output/mux/mixer/pga/speaker).

### Impl routing (wubu_dapm.c)

| Route | Path |
|-------|------|
| DAPM params | /sys/module/snd_hda_core/parameters |
| Sound control | /sys/class/sound/controlC0 |

Widget types: input(0), output(1), mux(2), mixer(3), pga(4), speaker(5).
Path active if name contains "on" or "enable".
