# Triple-Devil's-Advocate audit: WuBuOS vs SteamOS standards (2026-08-09)

The directive: "go through with a triple devil's advocate, fine tooth
comb, and actually compare it to the standards of the Steam operating
system and fill the gaps."

## DA pass 1 — the desktop/session layer (vs SteamOS Desktop Mode = KDE Plasma)

| SteamOS Desktop surface | WuBuOS | Verdict |
|---|---|---|
| SDDM login + session select | no boot login; the session manager (wubu_session) exists but the desktop boot doesn't wire it | **GAP (documented)** |
| System Settings (display/network/sound/personalization/input) | the Control Panel manager: Sound + Hardware were real; **Display/Network/Theme were DECLARED in the header with ZERO implementation** | **FILLED this pass** |
| the tray (battery/wifi/heat/volume) | the world-state tray (battery/wifi/heat) + clock + daemon icons; volume lives in the Sound applet | mostly ✓ |
| the notification center | dosgui_notif_center (add/mark_read/clear/is_open) | ✓ |
| the taskbar (start/window buttons/clock) | dosgui_wm_taskbar + clock menu | ✓ |
| the start menu (search/recent/power) | dosgui_startmenu (Search, Recently Used, Power Options, All Programs) | ✓ |
| the file manager | dosgui_explorer (drives/tree/preview) | ✓ |
| Big Picture Mode | dosgui_bpm (fullscreen gamepad grid over the era apps) | ✓ (previous pass) |

## DA pass 2 — the gaming/compat layer (vs SteamOS's stack)

| SteamOS component | WuBuOS | Verdict |
|---|---|---|
| gamescope session | the session manager's GAME mode (gamescope command builder) | ✓ partial |
| Steam Runtime (sniper) | wubu_steamrt (the REAL repo.steampowered manifest + env builder) | ✓ |
| Pressure Vessel | wubu_pressure_vessel (the PV1/PV2 path) | ✓ |
| Proton | wubu_proton2 + the VSL personalities (DOS/8086, Win/NT, Linux ELF, HolyC) | ✓ |
| gamemode | wubu_gamemode (the perf governor) | ✓ |
| Steam client (store/library/overlay) | the era-apps launcher + the colonel app registry; the REAL Steam client is run via Proton/VSL, not reimplemented | honest scope: WuBuOS HOSTS Steam, it doesn't clone Steam |
| the update mechanism | the arch daemon auto-update + the 8 snapshot modules | ✓ |

## DA pass 3 — the driver/hardware layer (REAL vs SIMULATED)

**The fine-tooth finding**: the driver registry's probes take their
MMIO from injected `set_*_mmio` hooks — the tests simulate the
hardware. On real hardware the MMIO must come from the PCI BARs.
wubu_pci ALREADY reads BAR0/BAR1 in the config space, but
wubu_drv_pci_scan() DROPPED them.

**FILLED this pass**: the scan now carries bar0/bar1 into the device
table + wubu_drv_dev_bar() exposes the real MMIO base — the mapping
point the drivers use on real hardware. Test-proven
(wubu_drv_dev_bar on a BAR'd device returns the MMIO base).

Remaining honest gaps (documented, not claimed):
- the keyboard/mouse CP applets (declared, not built — SteamOS Input)
- the boot login/session-select (SDDM parity)
- the eDP/DSI backlight + the HDA DAC DMA path (real hardware brings-up)
- the real Steam client UI (WuBuOS hosts Steam via Proton/VSL — the
  store/library/overlay are Steam's, by design)

## The fills this pass (all gated green)

1. **dosgui_cp_display.c** — the Display applet (connector/mode/VRAM
   from the world snapshot). `display: MIPI-DSI 1280x800@60 8192MB vram`
2. **dosgui_cp_network.c** — the Network applet (wifi/eth links from
   the world). `network: wifi UP (present) eth down`
3. **dosgui_cp_theme.c** — the Theme applet (the real theme engine:
   current theme + a click cycles). `theme: Win98 Classic`
4. **wubu_drv.c BAR glue** — the real-hardware MMIO mapping point
   (wubu_drv_dev_bar + the scan carries bar0/bar1)
5. **wubu_world.h** — the struct is NAMED (`struct wubu_world`) so the
   GUI headers can forward-declare it (the anonymous typedef broke the
   applet typedefs)
6. the manager registers ALL FIVE applets (display/network/sound/
   theme/hardware) — the sidebar shows the full System Settings

Gates: test_dosgui_cp_dnt 4/4, test_dosgui_controlpanel 7/7,
test_drv 11/11 (incl. the BAR glue), machines + world green.
