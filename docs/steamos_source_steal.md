# SteamOS Source Steal — The Driver Space (2026-08-09)

Fresh SteamOS source pulled and mined; the drivers are implemented into
WuBuOS with green tests. "We take from SteamOS. We are all TempleOS
kernel, but we AGI and take all the work. Best of wins."

## The source (cloned this session)

| Source | What it is | Stolen from |
|---|---|---|
| `vendor/SteamOS/` | Valve's SteamOS repo (OEM spec + docs) | the OEM contract |
| `vendor/mainline-linux/drivers/hid/hid-steam.c` | **THE Steam Controller/Deck gamepad driver** (1880 lines, the full 64-byte report protocol) | the input report layout |
| `vendor/mainline-linux/drivers/platform/x86/ayaneo-ec.c` | the handheld EC driver (fan/PWM/mode) | the EC register semantics |

(`vendor/` is gitignored — re-cloneable reference, kept on disk for
stealing.)

## What was stolen + implemented

### 1. The Steam Deck controller report protocol → `wubu_steaminput.c`

hid-steam.c's `steam_do_deck_input_event` decodes the 64-byte Deck
state report (ID 9) sent every ~4ms. WuBuOS now parses the SAME layout
in `wubu_si_parse_deck_report()`:

- bytes 8/9/10/11/13/14 → buttons (TR2 TL2 TR TL Y B X A, dpad, grips,
  sticks-click, base/Steam button)
- bytes 48/50 (L stick), 52/54 (R stick) — Y negated
- bytes 44/46 → triggers
- bytes 16/18 (L pad), 20/22 (R pad) — gated by the touch bits

Every decoded control flows through the mapping table (A=Space,
L-stick=WASD, R-stick=mouse) into the kernel input queue.
`make test_steaminput` 7/7 green (incl. a real report with A + stick).

### 2. The handheld EC driver → `wubu_ec_control.c`

ayaneo-ec.c's register semantics (the same class as the Deck's EC):
- fan speed: 16-bit across two registers
- PWM duty: 0-255 stored, percent*2.55 encoding
- PWM mode: 1 = manual / 2 = auto
- temp register

`wubu_ec_init/fan_rpm/set_pwm/set_mode/temp` with a pluggable ops
table (the test injects a fake register file).
`make test_ec_control` 5/5 green.

### 3. Both on the /n control plane (the "do it better" half)

SteamOS reaches the controller through sdgyrodsd + SDL gamecontrollerdb
+ Steam's config DB, and the EC through fancontrol + hwmon sysfs.
WuBuOS expresses ALL of it through ONE filesystem:

```
/n/ec/fan            -> the fan RPM (read)
/n/ec/pwm            -> write 0-100 = set the manual duty
/n/ec/mode           -> write 1/2 = manual/auto
/n/ec/temp           -> the EC thermal reading
/n/ec/status         -> one-line summary
/n/steaminput/map    -> the controller-as-keyboard bindings
/n/steaminput/report -> write 64 hex bytes = parse a Deck report
/n/steaminput/status -> summary
```

Each file wraps the REAL WuBuOS API (wubu_ec_control / wubu_steaminput)
via ns_mkdir/ns_write — zero new daemons, one namespace.
`make test_ns_ec` 4/4, `make test_ns_steaminput` 3/3 green.

## Scorecard

| SteamOS mechanism | WuBuOS equivalent | Gate |
|---|---|---|
| hid-steam (Deck gamepad) | `wubu_steaminput.c` report parser | 7/7 |
| EC fancontrol / hwmon | `wubu_ec_control.c` | 5/5 |
| sdgyrodsd / SDL db / Steam config | `/n/steaminput` | 3/3 |
| fancontrol / i8k / sysfs | `/n/ec` | 4/4 |

## Next steals (from the same source)

- the `steam_do_deck_sensors_event` IMU path (gyro/accel) — **DONE
  (2026-08-09)**: `wubu_si_parse_deck_sensors()` — accel 24-28, gyro
  30-34, the gyro drives mouse deltas (gyro-to-mouse aim).
- the battery voltage (bytes 62-63) + the power_supply path — **DONE
  (2026-08-09)**: `wubu_si_parse_battery()` (voltage mV at 12-13,
  percent at 14) + `/n/steaminput/battery`.
- the lizard-mode toggle (ID_SET_DEFAULT_DIGITAL_MAPPINGS /
  ID_CLEAR_DIGITAL_MAPPINGS) — the "controller acts as keyboard until
  a real client opens" behavior
- the nintendo/hid-nintendo.c protocol for the Switch Pro controller

## Changelog

- 2026-08-09 (32fc11b): Deck report protocol + EC + both /n subtrees.
- 2026-08-09 (next): IMU gyro-to-mouse + battery decode + /n battery.
