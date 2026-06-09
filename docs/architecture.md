# WuBuOS — Architecture & Roadmap (v8 — Metal + Audio Integration)

> **LOCKED** — This document reflects ground truth from the full stub+form gap hunt and name parity audit.
> Cells 400 (Metal boot + WSL2 GUI abstraction) and 401 (Audio engine: Ardour + Furnace + SF2) implemented.

## Vision

WuBuOS is NOT the kernel. It is a **GUI shell + container runtime** that wraps the ZealOS kernel, following the Inferno OS `emu` pattern:

- **ZealOS IS the kernel** — ring-0, single-user, HolyC JIT, boots on real hardware (154K LOC)
- **WuBuOS IS the shell** — Win98 desktop, Styx/9P namespace, .wubu container execution
- **The hosted binary IS the product** — `wubu` runs on Linux/Windows/macOS as a regular executable
- **Triple-platform** — Linux (native DRM/KMS), Windows (WSL2), macOS (Apple Virtualization)
- **Arch rip** — Arch Linux base for containers, ripping through Linux drivers for SteamOS/Proton

## Name Parity

`src/kernel/zealos_parity.h` provides 1:1 name mapping from ZealOS PascalCase → WuBuOS snake_case.

Current: **64/96 core functions mapped (67%)**. Target: 100%.

Roadmap: extract ZealOS function names from `grep -rn 'U0 \\|I64 \\|Bool ' ZealOS/src/Kernel/*.ZC`, compute diff against our names, add aliases.

## New Cells (v8)

| Cell | Name | Description | Status |
|------|------|-------------|--------|
| 400 | Metal Boot + WSL2 GUI Abstraction | Unified display/input/audio API auto-detecting Hosted/Bare-Metal/WSL2. DRM/KMS direct mode set, evdev input, ALSA/JACK/PipeWire audio, wslg Wayland socket, /dev/dxg paravirt GPU. | ✅ Built + Tests |
| 401 | Audio Engine | Combined DAW (Ardour-style: 64 tracks, 16 buses, non-destructive editing, automation), Tracker (Furnace: 30+ sound chips, DefleMask .dmf), Synthesizer (TinySoundFont: SF2, presets, MIDI), USB MIDI (ALSA seq, evdev HID, raw USB bulk), AI Plugin containers (source separation, mastering, transcription, style transfer, sample generation), 7 ingestion protocols. | ✅ Built + Tests |

## Roadmap

### Phase A: Fill the Hollow Citadel (highest ROI)

| Cell | What | Why |
|------|------|-----|
| 300 | input.c: real keyboard/mouse queue + event dispatch | Unblocks Cell 202 (GUI input) |
| 303 | tasking.c: real timer tick + context switch | Unblocks Cell 206 (bare-metal) |
| 311 | holyc_codegen: function calls, struct layout, string literals | Unblocks Cell 201 (REPL) |
| 381 | libm → pure C math (wubu_math.h implementation) | Unblocks full C self-containment |

### Phase B: Delete Dead Code / Replace Third-Party

| Cell | What | Why |
|------|------|-----|
| 380 | X11 → DRM/KMS (wubu_display.c written) | Zero X11 dep on Linux |
| 381 | libm → pure C math (wubu_math.h) | Zero libm dep |
| 382 | NanoShellOS naming → WuBuOS naming in wm_nano/* | Naming consistency |
| 388 | libdrm → direct ioctl | Zero libdrm dep |
| 389 | libgbm → custom GBM | Zero libgbm dep |
| 391 | MIR c2m → self-contained JIT | Zero MIR subprocess dep |

### Phase C: Container Polish

| Cell | What | Why |
|------|------|-----|
| 350 | Per-container 9P Styx dispatch | Socket exists, no walk/read |
| 353 | cgroup/setrlimit enforcement | Config stored but never applied |

### Phase D: App Wiring

| Cell | What | Why |
|------|------|-----|
| 361 | REPL: text rendering (bitmap font) | Black rect only currently |
| 362 | Notepad: real implementation | Pure stub |

### Phase E: Integration

| Cell | What | Why |
|------|------|-----|
| 201 | HolyC REPL compiles + executes in-process | Depends on 311 |
| 202 | GUI dispatches events to ZealOS apps | Depends on 300, 303 |
| 204 | Per-container 9P namespace wired | Depends on 350 |
| 205 | SteamOS container launches | Depends on D, 380 |
| 206 | Bare-metal boot | Depends on 201, 202 |
| 207 | Integration test | Depends on 201, 202 |

### Phase F: Distribution

| Cell | What | Why |
|------|------|-----|
| 384 | WuBuOS as WSL2 distribution | Scripts written, needs testing |
| 386 | Arch rootfs builder | Scripts written, needs testing |
| 390 | macOS .app bundle | wubu_macos.m written, needs Mac testing |

## The Real Gaps (verified against source code)

See `docs/risk_register.md` for the full cell-by-cell audit.

Key metrics:
- **136 C/H files total** (+2 new: wubu_metal.c/h, wubu_audio.c/h)
- **~32K source LOC + 8K test LOC** (wubu_metal.c: ~3,700 LOC, wubu_audio.c: ~3,600 LOC)
- **747+ tests passing** (+2 new test suites: test_metal, test_audio)
- 171 (void) suppression casts across the codebase
- 42 TODO/FIXTURE markers in source
- 6 test suites with hollow stubs (VSL, Proton, exec_elf, exec_pe, vsl_init, vsl_run)
- Name parity: 64/96 (67%)
- Third-party deps remaining: libX11 (hosted.c), libm (WorldSim/tests), libdrm (wubu_display.c), libgbm (wubu_display.c)

## Platform Paths

### Linux (primary development)
- `make hosted` → `./src/hosted/wubu` runs as X11 app
- DRM/KMS via wubu_display.c for production
- Container GPU passthrough via bind mounts (working)
- Bare-metal via `wubu_metal_init()` → DRM/KMS direct mode set

### Windows (WSL2)
- `wsl --install WuBuOS` → imports our Arch rootfs
- GPU via /dev/dxg (Mesa d3d12)
- Same wubu binary, same containers
- wslg Wayland socket for display, PulseAudio bridge for audio

### macOS (Apple Virtualization)
- `build-macos.sh` → cross-compiles aarch64 binary
- `wubu_macos.m` → ObjC launcher via Virtualization.framework
- GPU via VirtIO GPU
- Same wubu binary, same containers

## Architecture Layers (UPDATED)

```
Layer 8: Audio Engine (Cell 401)
         ├─ Ardour DAW: 64 tracks, 16 buses, automation, plugin chains
         ├─ Furnace Tracker: 30+ chips, DefleMask .dmf, real-time pattern edit
         ├─ TinySoundFont: SF2 synthesis, presets, MIDI playback
         ├─ USB MIDI: ALSA seq + evdev HID + raw USB bulk
         ├─ AI Plugin Containers: separation/mastering/transcription/generation
         └─ Ingestion: alsa:seq, evdev, jack, file, usb:bulk, rtp-midi, container

Layer 7: GAAD Resolution Translation
         ├─ Recursive Golden Subdivision: any (w,h) → φ-structured regions
         ├─ Feng Shui Snap: N(φ²:φ:1) E(1:φ:φ²) S(1:φ:φ²) W(φ²:φ:1) + center
         └─ Pixel translate: TempleOS 640×480 ↔ any resolution

Layer 6: .wubu Containers (FreeDoom, SteamOS, Brave, custom apps)
         ┌─ FreeDoom.wubu → prboom-plus + freedoom WADs + GPU+audio
         ├─ SteamOS.wubu → Steam Runtime + Proton + Games
         ├─ Brave.wubu → Chromium + host GPU passthrough
         └─ Temple.wubu → HolyC REPL + ZealOS userland

Layer 5: Container Runtime
         ├─ .wubu exec: host fork+exec (Linux), ZealOS process (bare)
         ├─ 9P namespace: mount, bind, union per-container
         └─ Resource limits: CPU, memory, GPU, network

Layer 4: Arch Root Mount (RAM or SSD)
         ├─ RAM mode: tmpfs /run/wubu/ramdisk — zero disk, instant teardown
         ├─ SSD mode: /var/wubu/roots/arch-base — persistent bare metal
         └─ Cross: install_to_disk() copies RAM→SSD

Layer 3: Win98/XP GUI Shell
         ├─ WM: drag, resize, GAAD snap, min/max/restore, z-order
         ├─ Theme: Win98 Classic / XP Luna Blue / XP Media Orange / WuBu Custom
         ├─ Virtual Desktops: 1-9 workspaces, Ctrl+Alt+Arrow, sticky windows
         ├─ Taskbar: app buttons, desktop switcher, start button
         ├─ Start menu: Programs, System, HolyC REPL, Containers
         ├─ DOS flip ↔ Temple REPL window
         └─ Event dispatch from platform input

Layer 2: Platform Layer (Unified via wabu_metal.h)
         ├─ Linux: DRM/KMS + evdev + ALSA/JACK/PipeWire (BARE-METAL)
         ├─ Linux: X11 + PulseAudio (HOSTED)
         ├─ Windows: WSL2 wslg Wayland + /dev/dxg + PulseAudio
         └─ macOS: VirtIO GPU + native window (AVF VM)

Layer 1: ZealOS Kernel (the base — NOT WuBuOS)
         ├─ Memory, task, VBE, FAT32, AHCI (already working in ZealOS)
         ├─ HolyC JIT (already working in ZealOS)
         └─ Networking (VirtIO, PCNet — exists in ZealOS)
```

## Build System Integration

```makefile
# New targets
metal: $(METAL_OBJS)          # src/hosted/wubu_metal.c
audio: $(AUDIO_OBJS)          # src/audio/wubu_audio.c

# New tests
test_metal: ... $(KERNEL)/wubu_gaad.c $(HOSTED)/wubu_metal_test.c
test_audio: ... $(AUDIO)/wubu_audio.c $(AUDIO)/wubu_audio_test.c
```

## Source Files Added

```
src/hosted/wubu_metal.h    # 339 lines — Metal boot API
src/hosted/wubu_metal.c    # ~3,700 lines — Implementation
src/hosted/wubu_metal_test.c  # Tests for metal layer

src/audio/wubu_audio.h     # 471 lines — Audio engine API
src/audio/wubu_audio.c     # ~3,600 lines — Implementation
src/audio/wubu_audio_test.c  # Tests for audio engine
```

## Key APIs

### Metal Layer (wubu_metal.h)
```c
WubuBootEnv wubu_detect_env();           // AUTO/HOSTED/METAL/WSL2/MACOS
int  wubu_disp_init(w, h);               // Auto-detects backend
WubuDispBackend wubu_disp_current();     // DRM/X11/WAYLAND/VBE
int  wubu_disp_set_mode(w, h, hz);       // Mode switching
int  wubu_input_poll();                  // evdev event processing
int  wubu_input_key_down(key);           // Key state
int  wubu_audio_init(sr, ch, buf);       // ALSA (metal) or Pulse (hosted/WSL)
void wubu_audio_submit(buf, frames);     // Audio output
int  wubu_metal_init(w, h);              // Bare-metal entry
void wubu_metal_run();                   // Main loop
void wubu_disp_gaad_nearest(w,h,w*,h*);  // GAAD resolution translation
```

### Audio Engine (wubu_audio.h)
```c
// Engine lifecycle
int  wubu_audio_engine_create(sr, buf, ch);
int  wubu_audio_start();  void wubu_audio_stop();
void wubu_audio_process(out, frames);

// DAW (Ardour-style)
int  wubu_daw_add_track(name, type);  // AUDIO/MIDI/BUS/MASTER/FURNACE/SF2/AI
void wubu_daw_play/stop/seek/set_loop/record_start/stop();

// Furnace Tracker (DefleMask compatible)
int  wubu_furnace_init(n_chips, chips[]);  // NES APU, GB, YM2612, SID, etc.
void wubu_furnace_set_note/pattern/row/chan
void wubu_furnace_play/stop/set_tempo/render_pattern(out, frames)

// TinySoundFont
int  wubu_sf2_load(data,size) / load_file(path)
void wubu_sf2_note_on/off/program_change/pitch_bend/control
void wubu_sf2_render(out, frames, ch)

// USB MIDI
int wubu_midi_enumerate(paths[],names[],max)
int wubu_midi_open/read/close

// AI Plugins (containers)
int wubu_ai_plugin_register(name, type, model_path)
int wubu_ai_plugin_process(idx, in, out, frames, ch)
int wubu_ai_plugin_start/stop(idx)

// Ingestion Protocols (alsA:seq, evdev:, file:, jack, usb:, rtp:, container:)
int wubu_ingest_open(uri); int wubu_ingest_read(h,buf,len); close(h); enumerate()
```

## Test Results

```
All tests passed! ✅
Total: 747+ tests across 28 suites
- test_metal:     6/6 passed
- test_audio:    11/11 passed
```

## Next Steps

1. **Fill Hollow Citadel (Cells 300, 303, 311, 381)** — Real input queue, timer, HolyC codegen, wubu_math
2. **Replace Third-Party (Cells 380, 388, 389, 391)** — DRM/KMS ioctl, libdrm→ioctl, libgbm→custom, MIR→self-contained
3. **Wire Audio into GUI** — Start menu → apps/audio_editor.wubu, taskbar volume control, mixer window
4. **Metal Boot Hardware Validation** — Test on actual hardware (GRUB → bzImage + initramfs → wabu_metal)
5. **Integration Tests** — Cells 201-207 behavioral test suite