# WuBuOS Master Architecture: Bare-Metal + WSL2 + Audio Engine

## THE VISION

WuBuOS is a synthesis of three computing eras on Arch Linux:

1. **TempleOS/HolyC** — the raw DOS soul (ring-0 escape hatch, JIT compilation)
2. **Win98/XP** — the humane shell (taskbar, start menu, window management)
3. **Arch Linux** — the stable kernel (drivers, Proton, containers, GPU passthrough)

Plus a professional audio production environment:
- **Ardour** DAW backend — multitrack recording, mixing, mastering
- **Furnace** tracker — chiptune composition on 30+ sound chips
- **TinySoundFont** — SF2 sample-based synthesis with USB MIDI
- **AI plugins** containerized DSP with GPU inference

## BOOT PATHS

### Path 1: Hosted (Linux Binary)
```
wubu (ELF binary)
  → X11/Wayland (via wubu_disp_init auto-detect)
  → VBE framebuffer (render target)
  → GUI shell (Win98/XP theming, GAAD snap, virtual desktops)
  → evdev input → WM → apps
  → PulseAudio/PipeWire output
```

### Path 2: Bare-Metal (Kernel + Initramfs)
```
GRUB/systemd-boot
  → bzImage + initramfs.img
  → Kernel boots, mounts initramfs
  → /init → /wubu
  → DRM/KMS direct mode set (no X11)
  → evdev input (keyboard, mouse, gamepad, MIDI)
  → ALSA → JACK → PipeWire audio
  → Full screen WuBuOS desktop
```

### Path 3: WSL2 Distro
```
Windows → wsl --install WuBuOS
  → LxssManager → wubu
  → /dev/dxg (paravirt GPU)
  → wslg (Weston Wayland compositor → RDP bridge)
  → wslg PulseAudio bridge
  → Full desktop in Windows window
```

## DISPLAY ARCHITECTURE

### Unified Display API (`wubu_metal.h` / `wubu_display.c`)

All three paths present the same interface:
- `wubu_disp_init(w, h)` — auto-detects backend
- `wubu_disp_flip()` — presents framebuffer
- `wubu_disp_set_mode(w, h, hz)` — changes resolution
- `wubu_disp_poll_events()` — processes display events

### Backend Selection
1. **WSL2**: Uses wslg Wayland socket (`/tmp/wslg/wayland-0`)
2. **Bare-metal**: DRM/KMS direct (`/dev/dri/card0`)
3. **Hosted**: X11 (XOpenDisplay) or Wayland
4. **Legacy**: VBE BIOS modes

### Resolution Scaling (GAAD)
- All resolution changes use GAAD golden subdivision
- 640×480 → any resolution via region mapping
- φ-based zoom for UI scaling
- Per-backend mode switching

## WSL2 GUI ABSTRACTION

### Detection
- Check `/sys/class/dmi/id/sys_vendor` for "Microsoft"
- Check `/proc/version` for "Microsoft"
- Check for `/dev/dxg` existence
- Check for wslg socket paths

### Display Pipeline
```
WuBuOS VBE framebuffer
  → wubu_disp_flip()
  → sub-backend:
    - WSL2: Write to wslg Wayland surface via /dev/dxg
    - Metal: drmModeSetCrtc page flip
    - Hosted: XPutImage to X11 window
```

### Input Pipeline
```
wslg input bridge
  → /dev/input/event* (WSLg virtual devices)
  → wubu_input_poll()
  → WM event routing
```

### GPU Passthrough
- WSL2 uses `/dev/dxg` (paravirt DXGI)
- DXVK translates D3D11 → Vulkan → dxg → Windows GPU driver
- Native performance for GPU-accelerated apps

## AUDIO ENGINE

### Architecture
The audio engine combines three best-in-class components:

```
┌─────────────────────────────────────────────────────────────┐
│                    WuBuOS Audio Engine                       │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐ │
│  │   Furnace     │  │  TinySound   │  │  Ardour DAW      │ │
│  │   Tracker     │  │  Font (SF2)  │  │  Mixer           │ │
│  │              │  │              │  │                  │ │
│  │ NES APU      │  │ sf2repo     │  │ 64 tracks        │ │
│  │ GB DMG       │  │ sf2create   │  │ 16 buses         │ │
│  │ Genesis FM   │  │ sf2 files   │  │ Automation       │ │
│  │ SID 6581     │  │             │  │ Plugin chains    │ │
│  │ SN76489      │  │ MIDI synth  │  │ Region editing   │ │
│  │ +25 chips    │  │ Real-time   │  │ Non-destructive  │ │
│  └──────┬───────┘  └──────┬───────┘  └────────┬─────────┘ │
│         │                 │                    │           │
│         └────────────┬────┴────────────┬───────┘           │
│                      │                 │                    │
│              ┌───────▼───────┐  ┌──────▼──────────────┐   │
│              │  AI Plugin    │  │  Master Bus          │   │
│              │  Container    │  │                      │   │
│              │               │  │  Volume, pan, limiter│   │
│              │  Source sep   │  │  Metering            │   │
│              │  Mastering    │  │                      │   │
│              │  Transcribe   │  └──────────┬───────────┘   │
│              │  Style xfer   │             │               │
│              │  Generate     │             │               │
│              └───────────────┘             │               │
│                                           ▼               │
│                              ┌─────────────────────────┐  │
│                              │  ALSA / JACK / PipeWire  │  │
│                              │  → Speakers / Headphones │  │
│                              └─────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### Furnace Integration
- Full Furnace tracker implementation in C
- 30+ sound chip emulators (NES, GB, Genesis, SID, etc.)
- DefleMask .dmf file format support
- Pattern editor with effects (arpeggio, slide, vibrato)
- Real-time playback via ALSA/JACK
- MIDI input for live keyboard play

### TinySoundFont Integration
- Load .sf2 SoundFont files (from sf2repo / sf2create)
- Real-time MIDI synthesis
- sf2repo integration: download free SoundFonts
- sf2create integration: create custom SoundFonts
- Multiple simultaneous instruments
- Built-in reverb + chorus

### Ardour DAW Integration
- 64-track multitrack recording/editing
- 16 send/return buses
- Non-destructive region editing
- Automation lanes per track
- Plugin chain per track/bus (LV2, VST, CLAP, WUBU-AI)
- JACK audio routing

### USB MIDI Pipeline
```
USB MIDI Controller (e.g. Akai, Novation, Arturia)
  → /dev/snd/seq (ALSA sequencer)
  → /dev/bus/usb/* (raw USB for class-compliant)
  → evdev (keyboards showing as HID)

Routing:
  → Furnace: live note input on tracker
  → SF2: real-time sample playback
  → Ardour: record to MIDI track
  → AI: transcription (audio → MIDI)
```

### AI Plugin Ingestion
AI plugins run as `.wubu` containers with:
- Audio buffer input via JACK/pipe
- GPU access for inference (/dev/dri passthrough)
- Model loaded from /wubu/models/
- Processed audio returned to DAW mix

Plugin types:
1. **Source Separation** — split stems (vocals, drums, bass)
2. **Auto-Mastering** — loudness, EQ, limiting
3. **Transcription** — audio → MIDI notation
4. **Style Transfer** — apply reference track characteristics
5. **Sample Generation** — generate new samples from prompt
6. **Noise Reduction** — remove background noise
7. **Audio Super-Resolution** — upsample low-quality audio

### Ingestion Protocols
Multiple ways to get data INTO the engine:
1. `alsa:seq` — ALSA raw MIDI
2. `evdev:/dev/input/event*` — evdev HID
3. `jack` — JACK audio/MIDI transport
4. `file:path` — file-based (mid, dmf, sf2, wav, flac, ogg, mp4)
5. `usb:bulk` — raw USB bulk transfer
6. `rtp-midi` — network MIDI
7. `container:name` — .wubu container plugin via 9P

## CONTAINER PHILOSOPHY (Inferno OS Pattern)

Every major subsystem is a `.wubu` container:

| Container | Contents | GPU passthrough |
|-----------|----------|-----------------|
| `arch-base` | Minimal Arch Linux | No |
| `proton` | Wine + DXVK + Vulkan | Yes |
| `ardour` | Ardour DAW + JACK | Yes (for AI) |
| `furnace` | Furnace tracker | No |
| `sf2-synth` | TinySoundFont server | No |
| `ai-dsp` | AI model + CUDA | Yes |
| `ffmpeg` | FFmpeg codecs | Yes (NVDEC/NVENC) |

Each container:
- Runs as a host process (fork + chroot + exec)
- Gets its own 9P namespace
- Can be GPU-accelerated via bind mounts
- Communicates via JACK (audio), 9P (files), Wayland (display)

## BARE-METAL BOOT SEQUENCE

### 1. Bootloader (GRUB/systemd-boot)
```
GRUB menu → select WuBuOS
  → load bzImage (Linux kernel)
  → load initramfs.img (cpio archive)
  → boot kernel with: root=/dev/ram0 init=/wubu
```

### 2. Kernel Initialization
```
Linux kernel boots
  → initramfs mounted as root
  → /init script runs
  → mounts /dev/sda1 as /wubu (ext4/btrfs)
  → exec /wubu
```

### 3. WuBuOS Init
```
wubu starts
  → Detect boot environment (metal)
  → wubu_disp_init(): open /dev/dri/card0, set mode
  → wubu_input_init(): open evdev devices
  → wubu_audio_init(): open ALSA PCM
  → Start GUI shell (desktop, taskbar, start menu)
  → Start 9P namespace server
  → Start container manager
```

### 4. Hardware Detection
```
GPU:     /sys/class/dri → DRM driver → mode set
Input:   /dev/input/event* → evdev → classify devices
Audio:   ALSA PCM → JACK server → PipeWire bridge
USB:     /dev/bus/usb → enumerate → mount in containers
MIDI:    /dev/snd/seq → ALSA sequencer → engine
```

## FILESYSTEM LAYOUT

```
/wubu/
├── bin/
│   ├── wubu              # Main binary
│   ├── wubu-init         # Init script
│   └── wubu-install      # Installer
├── boot/
│   ├── bzImage           # Linux kernel
│   ├── initramfs.img     # Initial ramdisk
│   └── grub/             # Bootloader config
├── etc/
│   ├── wubu.conf         # System config
│   ├── themes/           # Theme files
│   └── containers/       # Container configs
├── home/
│   └── user/
│       ├── Documents/
│       ├── Music/        # Audio projects
│       ├── Games/        # Windows games (.wubu)
│       └── Containers/   # User containers
├── var/
│   ├── wubu/
│   │   ├── roots/        # Container rootfs
│   │   ├── plugins/      # AI plugin models
│   │   ├── soundfonts/   # SF2 files
│   │   └── sessions/     # Ardour sessions
│   └── log/
└── tmp/
    └── wubu-ct-*         # Container temp
```

## BUILD SYSTEM

### Bare-Metal Image Creation
```bash
# Build kernel
make kernel

# Build initramfs
make initramfs
  → cpio archive with:
    - /wubu (binary)
    - /init (startup script)
    - /lib/modules/* (kernel modules)
    - /usr/lib/firmware (GPU firmware)

# Create bootable USB
sudo make install-usb /dev/sdX
  → Partition: EFI + WuBuOS
  → Copy bzImage + initramfs.img
  → Install GRUB

# Create WSL2 distro
make wsl2-distro
  → .tar.gz of rootfs
  → wsl --install WuBuOS
```

### Hosted Build
```bash
make all          # Build everything
make test         # Run all 600+ tests
make hosted       # Build hosted binary
./src/hosted/wubu # Run
```

## TESTING STRATEGY

### Unit Tests (existing)
- 600+ tests across 25 test suites
- All pass before commit

### Integration Tests
1. **Display**: Mode set on all 3 backends
2. **Input**: evdev device classification
3. **Audio**: ALSA PCM open, buffer submit
4. **MIDI**: ALSA sequencer read/write
5. **Container**: fork+chroot+exec with GPU bind
6. **Proton**: PE validation, Wine launch
7. **GAAD**: Resolution translation accuracy

### Real-World Tests
1. **Game**: Launch Windows game via Proton container
2. **Audio**: Record MIDI → Furnace → Ardour → export
3. **AI**: Run source separation on audio file
4. **WSL2**: Full desktop in Windows window
5. **Metal**: Boot from USB, full desktop on hardware
