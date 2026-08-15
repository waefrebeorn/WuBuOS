# VOLTAGECTL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU voltage control gaps

GPU voltage control monitors core/memory voltage rails via hwmon sensors
for power state management across GPU generations.

### Impl routing (wubu_voltagectl.c)

| Route | Path |
|-------|------|
| Voltage rail input  | /sys/class/hwmon hwmon0/in0_input |
| Voltage rail input2 | /sys/class/hwmon hwmon0/in1_input |

Voltage states: low <700mV, nominal <900mV, high <1100mV, critical >=1100mV.
Conversion: mV * 1000 = microvolts (uV).
