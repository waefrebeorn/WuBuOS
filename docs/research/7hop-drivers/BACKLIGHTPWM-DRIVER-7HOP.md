# BACKLIGHTPWM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux display backlight PWM gaps

Backlight PWM modulates LED duty cycle for brightness.

### Backlight routing (wubu_backlightpwm.c)

| Type | Routing |
|------|---------|
| sysfs | `sysfs` |
| ACPI | `acpi-video` |
| Intel | `intel-backlight` |
| AMD | `amdgpu-bl` |
| PWM | `pwm-raw` |

| Brightness | Routing |
|------------|---------|
| max | `max` |
| min / off | `min` |
| 50 / half | `50` |
| unknown | `auto` (fallback) |

### Kernel summary

```
backlightpwm[bl=1 pwm=1 sysfs=1 acpi=0 intel=0 drv=sysfs-backlight]
```

Published to `/kv/world/hw_backlightpwm`. Verified live on this host.
