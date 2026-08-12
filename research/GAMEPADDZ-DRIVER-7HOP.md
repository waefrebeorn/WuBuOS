# GAMEPADDZ-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux gamepad deadzone gaps

Gamepad deadzone filters stick drift by ignoring small movements.
Deadzone calibration prevents false input on worn sticks.

### Impl routing (wubu_gamepaddz.c)

| Route | Path |
|-------|------|
| Gamepad presence | /sys/class/input/js0 |
| Gamepad device    | /sys/class/input/js0/device |

Filter: values within [-dz, +dz] become 0.
Drift: true if value is within deadzone.
