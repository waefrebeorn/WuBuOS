# WuBuOS OS Studies — Reference Inventory
**Location**: `/home/wubu/.hermes/profiles/mind-palace/home/myseed/os-studies/`
**Updated**: 2026-06-29

---

## ══════════════════════════════════════════════════════════════════════════════════
## STUDY FILES
## ══════════════════════════════════════════════════════════════════════════════════

| OS | File | Size | Purpose |
|----|------|------|---------|
| **GNOME 46+** | `gnome/gnome_study.md` | ~17KB | Mutter, GTK4, libadwaita, D-Bus, systemd, PipeWire, Flatpak, GVfs, Tracker, a11y |
| **SteamOS 3.x** | `steamos/steamos_study.md` | ~20KB | gamescope, Proton (DXVK/VKD3D), Pressure Vessel, Steam Input, Steam Networking, Shader cache |
| **TempleOS/ZealOS** | `templeos/templeos_study.md` | ~17KB | HolyC JIT, Doc/DolDoc, RedSea FS, Ring-0, VGA/VESA, PC Speaker, ZealOS parity |
| **Arch Linux** | `arch/arch_study.md` | ~19KB | pacman/alpm, AUR, ABS/makepkg, systemd, mkinitcpio, NetworkManager, PipeWire, security |

---

## ══════════════════════════════════════════════════════════════════════════════════
## QUICK REFERENCE: KEY GAPS BY OS
## ══════════════════════════════════════════════════════════════════════════════════

### GNOME → WuBuOS
- **D-Bus** (required for everything) → `runtime/wubu_dbus.c`
- **systemd** (services, sockets, timers, journal) → `runtime/wubu_systemd.c`
- **PipeWire + WirePlumber** (audio, Bluetooth, screen capture) → `audio/wubu_audio.c`
- **NetworkManager** (WiFi, VPN, profiles, DNS) → `wubu_network.c`
- **Flatpak + Portals** (sandboxed apps) → `runtime/wubu_flatpak.c`
- **GVfs** (SMB, SFTP, cloud, trash) → `runtime/wubu_gvfs.c`
- **Tracker/SPARQL** (file content search) → `apps/wubu_tracker.c`
- **AT-SPI2** (accessibility) → `gui/wubu_a11y.c`

### SteamOS → WuBuOS
- **gamescope** (VRR, HDR, FSR, nested Wayland) → `hosted/wubu_gamescope.c`
- **DXVK** (D3D9/10/11 → Vulkan) → `runtime/wubu_dxvk.c`
- **VKD3D-Proton** (D3D12 → Vulkan) → `runtime/wubu_vkd3d.c`
- **Pressure Vessel** (full namespaces, seccomp, Steam Runtime) → `wubu_ct_isolate.c`
- **Steam Input** (controller configs, gyro, haptics) → `input/wubu_steaminput.c`
- **Steam Networking** (relay, P2P, SDR) → `net/wubu_steamnet.c`
- **Shader pre-cache** (fossilize, dxvk-cache) → `runtime/wubu_fossilize.c`
- **Proton prefix management** → `runtime/wubu_proton.c`

### TempleOS/ZealOS → WuBuOS
- **HolyC JIT** (AOT + JIT, whole-program opt) → `compiler/holyc_codegen.c`
- **Compiler as library** (Lex/Parse/Opt/CodeGen) → `compiler/holyc_lib.c`
- **Real-time REPL** (immediate execution) → `runtime/wubu_holyd.c`
- **Doc/DolDoc** (hyperlinked docs, graphics, songs) → `apps/wubu_doldoc.c`
- **HolyC language** (class, exception, coroutine, reflection) → `compiler/holyc_features.c`
- **RedSea FS** (contiguous, B-tree, no paths) → `kernel/redsea.c`
- **VGA/VESA direct** (HolyC graphics) → `kernel/wubu_vga.c`
- **PC Speaker + Raw PCM** (HolyC Sound/Music) → `audio/wubu_audio.c`

### Arch Linux → WuBuOS
- **pacman + alpm** (binary packages, deps, hooks, sigs) → `runtime/wubu_pacman.c`
- **PKGBUILD + makepkg** (build from source) → `runtime/wubu_makepkg.c`
- **AUR** (community packages) → `apps/wubu_aur.c`
- **mkinitcpio hooks** (encrypt, lvm, raid, btrfs, zfs) → `deploy/wubu_mkinitcpio.c`
- **systemd-homed/sysusers** (portable homes) → `runtime/wubu_homed.c`
- **systemd-nspawn/podman** (OCI containers) → `wubu_ct_isolate.c`
- **AppArmor/landlock** (MAC, sandboxing) → `kernel/wubu_apparmor.c`
- **Secure Boot** (shim + GRUB/limine) → `deploy/wubu_secureboot.c`

---

## ══════════════════════════════════════════════════════════════════════════════════
## WIKI INTEGRATION
## ═══════════════════════════════════════════════════════════════════════════════════

These study files feed into the **Triple DA Wiki** at:
`/home/wubu/.hermes/profiles/mind-palace/home/myseed/wiki/os-comparison/`

| Wiki File | Purpose |
|-----------|---------|
| `index.md` | This index + quick start |
| `triple_da_wiki.md` | Full 4-way comparison + conflicting requirements + roadmap |
| `gap_analysis_wiki.md` | Actionable gap tracking (Feature → File → REAL_GAPs → Priority) |

---

## ══════════════════════════════════════════════════════════════════════════════════
## HOW TO ADD NEW STUDY DATA
## ═══════════════════════════════════════════════════════════════════════════════════

1. **Find the OS study file** in `os-studies/<os>/`
2. **Add feature details** to the appropriate section
3. **Update the wiki** (`triple_da_wiki.md` + `gap_analysis_wiki.md`)
4. **Add to BATTLESHIP.md** if it's a new REAL_GAP
5. **Log achievement** in `vault/achievements.md` when closed

---

*These studies are the raw intelligence. The wiki is the processed analysis. Keep both current.*