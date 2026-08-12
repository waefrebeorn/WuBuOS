# GAMEPADBM-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Linux gamepad button map gaps

Gamepad button maps define physical-to-logical button mapping
for consistent input across controllers from different vendors.

### Impl routing (wubu_gamepadbm.c)

| Route | Path |
|-------|------|
| Gamepad presence | /sys/class/input/js0 |
| Gamepad device    | /sys/class/input/js0/device |

Standard map: 0=A, 1=B, 2=X, 3=Y, 4=LB, 5=RB, 6=BACK, 7=START.
Buttons 0-15 valid; 16+ invalid. Pressed = non-zero value.
