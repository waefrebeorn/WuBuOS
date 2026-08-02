# WuBuOS user-facing gap audit + vision status

Date: 2026-08-02. Class: the USER-facing OS (not the 106/106 kernel register —
the register is closed; the user-facing polish is a different ledger).

## Real gaps found THIS session (all verified against the tree)

1. **The Control Panel's sound applet was declared, never built.**
   `dosgui_cp_create_sound_applet()` + `dosgui_cp_sound_state()` existed in
   the header for ages with NO implementation anywhere (grep proved it). The
   same pattern holds for the DISPLAY / NETWORK / THEME applet factories —
   the Control Panel has a sidebar of names but the applet bodies are
   missing. FIXED (sound): `dosgui_cp_sound.c` implements the applet against
   the new `wubu_sound` synthesis engine (volume bar, mute toggle, 8 Test
   buttons that PLAY the synthesized events). NOT YET FIXED: the other three
   factories.
2. **The OS had NO system sounds.** Nothing synthesized, no chimes, no
   assets. FIXED: `wubu_sound.{h,c}` — a pure-C PCM synthesizer (additive
   sine/square/triangle/saw + ADSR) with the Win98/XP heritage event table:
   startup chord (C5-E5-G5-C6), three-note notify, sawtooth error, click,
   shutdown (descending), maximize/minimize/restore blips. 8 WAV files
   rendered into `sounds/` (22050 Hz mono 16-bit). The DA caught a REAL bug:
   `wubu_sound_render` returned the padded cap instead of the actual sample
   count.
3. **No CPUID/SIMD/GPU detection anywhere in the OS** (only the VSL Vulkan
   backend switch). FIXED: `wubu_hwdetect.{h,c}` — the full strategy ladder:
   CPUID tier (scalar -> SSE2 -> AVX -> AVX2 -> AVX512 with FMA/BMI/
   AVX512-VNNI sub-flags + the XCR0 OS-enable check), vendor/brand strings,
   logical/physical cores, and the GPU matrix (CUDA > Vulkan/dxg > llvmpipe).
   Verified on this host: AuthenticAMD AVX512 tier + Vulkan (dxg) GPU.

## User-facing gap register (the NEXT polish items, in priority order)

- CP-1 Display/Network/Theme applet factories (same declared-only pattern as
  the sound one — the sidebar bodies are missing).
- CP-2 The sound applet's test buttons need a real audio sink (the engine
  renders; the host playback needs the audio-layer wiring — WSL has no
  sound-card by default; a wav-writer sink is the immediate path).
- UI-1 XP-vibes chrome details (the theme engine has 5 themes + pixel glyphs;
  the next polish: XP Luna title-bar glow, Win98 3D bevels on controls,
  taskbar hover states, start-menu two-column XP layout).
- UI-2 Boot-time hardware report (the detected matrix printed at boot: the
  tier + GPU backend + cores — the "Hardware:" line).
- UI-3 The startup chord on desktop-init (once the sink exists).
- SND-1 Per-event volume + the theme-able sound scheme (the Win98 vs XP
  sound sets).
- METAL-1 The ring-0 HolyC REPL (`wubu_holyd_metal`) — the "develop LIVE
  in-OS" vision's pending item.
- METAL-2 The hardware-detection on metal (CPUID works freestanding; the
  ACPI/SMBIOS machine-identity + the detected matrix into the boot log).

## Vision status (the user's whole vision vs the tree)

- **Win98/XP UI**: themes (5), pixel-glyph icons, selection chrome, the
  desktop features — REAL and capture-verified; the polish ledger above is
  the remaining delta.
- **Sounds**: NEW — the synthesis engine + 8 WAVs + the sound applet; the
  playback sink is the one gap.
- **Bare-metal AGI**: kernel register 106/106; the boot volume mounts; the
  AGI kernel (measure/verify/promote/operate) runs on metal; the ring-0 REPL
  + live-in-OS development remain (METAL-1).
- **Hardware acceleration**: NEW — the detection strategies module; the VSL
  Vulkan path + the gfxstream shim exist; the SIMD-tier dispatch is the
  next wiring (the wubuwizard engine already consumes a tier model).
- **The whole-vision doc**: `docs/AGI_OS_DESIGN.md` (ring-0 Colonel + Wayland
  user space + VSL overlays + attestation anti-cheat) is the design
  contract; this ledger tracks the user-facing execution against it.
