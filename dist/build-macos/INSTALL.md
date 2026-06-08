# WuBuOS on macOS

## Prerequisites

- macOS 11.0+ (Big Sur) for Apple Virtualization
- macOS 13.0+ (Ventura) for Rosetta (x86_64 container support)
- Apple Silicon (M1/M2/M3/M4) OR Intel Mac
- ~500 MB free disk space

## Option 1: Lima (simplest)

```bash
# Install Lima
brew install lima

# Start WuBuOS
lima start wubuos

# SSH into WuBuOS VM
lima ssh wubuos
# Inside: ./wubu   → Win98 GUI (X11 forwarding if XQuartz installed)
```

## Option 2: Apple Virtualization (native)

```bash
# Build the launcher (requires Xcode CLI tools)
xcode-select --install
clang -framework Virtualization -framework Foundation \
      -o wubu-macos-launcher wubu_macos.m

# Download an Arch ARM64 kernel + initrd
# (or use our initramfs-macos.img)

# Run
./wubu-macos-launcher --kernel vmlinuz --initrd initramfs-macos.img
```

## Option 3: WuBuOS .app (double-click)

1. Copy `WuBuOS.app` to `/Applications/`
2. Double-click
3. Arch VM boots → Win98 GUI appears in a macOS window

```bash
open /Applications/WuBuOS.app
```

## Display

- **X11 (GUI)**: Install XQuartz, `ssh -Y wubuos`, run `./wubu`
- **VNC**: VM outputs VNC via port 5900, open `vnc://localhost:5900`
- **Serial**: `ssh wubuos` → runs WuBuOS in terminal/console mode

## Containers

.wubu containers work identically to Linux/WSL2:

```bash
# Inside the WuBuOS VM
./wubu --container SteamOS.wubu
./wubu --container Brave.wubu
./wubu --container Temple.wubu
```

## Cross-platform comparison

| Platform | Host kernel | Guest kernel | GPU access | Container runtime |
|----------|-------------|--------------|------------|-------------------|
| Linux    | Linux       | Linux (same) | DRM/KMS    | fork+exec native |
| Windows  | NT/WSL2     | Linux (WSL2) | /dev/dxg   | fork+exec native |
| macOS    | XNU         | Linux (AVF)  | VirtIO GPU | fork+exec native |

All three run the SAME wubu binary. Same .wubu containers. Same 9P namespace.
