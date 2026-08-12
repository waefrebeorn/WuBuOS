# THERMAL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux fan/thermal control gaps

Thermal management controls fans via hwmon PWM.

### Thermal routing (wubu_thermal.c)

| Component | Role |
|-----------|------|
| hwmon | /sys/class/hwmon (pwm1, temp) |
| fancontrol | fan curves |
| thermal zone | /sys/class/thermal |
| pwm-fan | PWM fan driver |

### Modes

| Mode | Routing |
|------|---------|
| auto | `auto` |
| manual | `manual` |
| disabled | `disabled` |

### Fan curves

| Curve | Routing |
|-------|---------|
| aggressive | `aggressive` |
| quiet | `quiet` |
| balanced | `balanced` |

### Kernel summary line

```
thermal[hwmon=1 fan=0 zone=0 trip=0 fanctl=0 drv=hwmon]
```

Published to `/kv/world/hw_thermal` by `wubu_thermal_summary()`.

**Verified live:** this host reports `hwmon=1`.
