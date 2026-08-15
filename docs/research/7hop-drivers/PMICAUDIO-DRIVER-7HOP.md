# PMICAUDIO-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux PMIC + audio amp/DAC driver gaps

Three connected analog-front-end families. WuBuOS routes them all.

### PMIC (power management IC)
- Regulator subsystem (/sys/class/regulator) exposes power rails
- Drivers: qcom-spmi-pmic, bq25890 (charger), pmic-8xxx, pm8921

### Audio DAC routing (wubu_pmicaudio.c)

| DAC | Driver |
|-----|--------|
| ESS Sabre (es9038/es9018) | `es9038q2m` |
| AKM (ak4490) | `ak4490` |
| TI Burr-Brown (pcm1792) | `pcm1792` |
| Cirrus (cs4398) | `cs4398` |

### Audio amplifier routing

| Amp | Driver |
|-----|--------|
| TI tas5805m | `tas5805m` |
| TI tpa3116 | `tpa3116` |
| Maxim max98357a | `max98357a` |
| NXP tda7498 | `tda7498` |

### Kernel summary line

```
pmicaudio[pmic=0(none) reg=0 dac=0(none) amp=0(none)]
```

Published to `/kv/world/hw_pmicaudio` by `wubu_pmicaudio_summary()`.
