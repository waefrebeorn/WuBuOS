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
  ID_CLEAR_DIGITAL_MAPPINGS) — **DONE (2026-08-09)**:
  `wubu_si_set_lizard_mode()` — the controller acts as a keyboard
  until a real client opens the device (the mappings clear, nothing
  is emitted); closing restores the keyboard map. Matches
  hid-steam.c's lizard semantics exactly.
- the nintendo/hid-nintendo.c protocol for the Switch Pro controller

## Changelog

- 2026-08-09 (32fc11b): Deck report protocol + EC + both /n subtrees.
- 2026-08-09 (938444b): IMU gyro-to-mouse + battery decode + /n battery.
- 2026-08-09 (next): lizard mode (keyboard-until-client, from
  hid-steam.c's lizard behavior).

## Proton stack integration + the desktop space (2026-08-09)

### 4. The Steam Runtime (sniper) — `wubu_steamrt.c`

The canonical sniper lib manifest was mined from the REAL repo
(repo.steampowered.com/steamrt-sniper — HTTP 200, Debian 11 based,
929 packages; the full 378-lib runtime list is in
`docs/reference/sniper-runtime-libs.txt`). The 43 gaming-critical
libs (libvulkan1, mesa-vulkan-drivers, libgl1-mesa-dri, libsdl2,
libpipewire, libopenal, libwayland-client, libasound2-plugins, ...)
are the DEPENDENCY TRUTH for the Proton runtime.

wubu_steamrt builds the EXACT Proton launch environment:
```
STEAM_COMPAT_DATA_PATH=<compatdata>/<appid>
STEAM_COMPAT_LIBRARY_PATHS=<game common dir>
STEAM_COMPAT_TOOL_PATHS=<Proton dist>
STEAM_COMPAT_INSTALLED=1
WINEPREFIX=<compat>/pfx
LD_LIBRARY_PATH=<game lib>:<sniper lib>
```
+ wubu_steamrt_verify() — the manifest-first gap check (the
pressure-vessel philosophy: the runtime fills what the host lacks).
`make test_steamrt` 5/5, `make test_ns_steamrt` 3/3.
/n/steamrt/{manifest,env,verify}.

### 5. The desktop space — `wubu_session.c`

SteamOS's session-select contract (Game Mode = gamescope session,
Desktop Mode = Plasma session) mirrored for WuBuOS's two sessions:
- GAME = the gamescope compositor session (steam -steamos3 -steampal)
- DESKTOP = the dosgui desktop (wubu-desktop)

wubu_session_init/set/switch/current + /n/session/current ("game"|
"desktop") + /n/session/cmd. `make test_session` 5/5,
`make test_ns_session` 3/3.

### Scorecard (proton stack + desktop)

| SteamOS mechanism | WuBuOS equivalent | Gate |
|---|---|---|
| sniper runtime libs | wubu_steamrt manifest + verify | 5/5 |
| Proton launch env | wubu_steamrt_build_env | included |
| steamos-session-select | wubu_session + /n/session | 5/5 |
| the steamrt run.sh | the pressure-vessel LD_LIBRARY_PATH | 5/5 |

## The real-hardware driver space (2026-08-09) — Steam Deck + laptop

The goal: WuBuOS boots on a REAL Steam Deck + laptop. The kernel's
bus layer (PCI/ACPI/XHCI/AHCI) existed; the DRIVER SPACE on top is
now complete:

### 6. The driver registry — `wubu_drv.c`

The Linux-style device model: the buses enumerate devices, every
driver carries an ID table, the registry matches + probes. The built-
in table covers the Deck + laptop hardware:

| Device | ID | Driver |
|---|---|---|
| AMD Van Gogh iGPU (Deck) | 1002:163F | gpu (KMS-lite: connector + mode + VRAM) |
| Samsung 980/990 NVMe | 144D:A80A | nvme (admin queue + identify) |
| SanDisk SN770 2230 | 15B7:5009 | nvme |
| MediaTek RZ616 Wi-Fi (Deck) | 14C3:7922 | wifi (link + MAC) |
| Intel AX201/AX211 | 8086:51F0/2723 | wifi |
| Realtek r8168/r8169 | 10EC:8168 | net (link + MAC) |
| AMD Van Gogh HDA (Deck) | 1022:1457 | hda (codec verb ring) |
| ACPI battery | class 01/0C | battery (_BIF/_BST: capacity, percent) |
| SATA class | 01/06 | ahci (the pre-existing controller) |

`make test_drv` 6/6: the fake Deck bus binds every real device, the
NVMe comes ready (512GB), the Wi-Fi MAC is read, the GPU modesets the
1280x800 DSI panel, the battery reports 97%.

### The remaining hardware classes (next steals)

- the xHCI USB stack (wubu_xhci.c exists) — the Deck's USB-C + SD
- the PS/2 + I2C HID keyboard/trackpad (input.c exists for the queue)
- the EC fan/thermal (wubu_ec_control.c, done in the earlier wave)
- the audio DAC path (the HDA codec -> the DMA engine)
- the eDP/DSI backlight (the GPU driver's panel)
