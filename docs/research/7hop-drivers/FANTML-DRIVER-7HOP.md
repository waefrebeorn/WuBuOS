# FANTML-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux GPU fan/thermal control gaps

GPU fan/thermal monitoring reads temperature + fan RPM via hwmon sensors,
with PWM control for fan speed regulation.

### Impl routing (wubu_fantml.c)

| Route | Path |
|-------|------|
| GPU temp input  | /sys/class/hwmon hwmon0/temp1_input |
| Fan RPM input   | /sys/class/hwmon hwmon0/fan1_input |
| Fan PWM control  | /sys/class/hwmon hwmon0/pwm1 |

Temperature thresholds: cool <50C, normal <70C, warm <85C, hot <100C,
critical >=100C. Fan pct = rpm * 100 / max_rpm.
