# Arch Linux ISO Study — Architecture & Feature Inventory
**Source**: Arch Linux (rolling), archiso, pacman, systemd, mkinitcpio, AUR
**Purpose**: Triple DA comparison for WuBuOS distro/container gap analysis

---

## ═══════════════════════════════════════════════════════════════
## ARCH LINUX ARCHITECTURE OVERVIEW
## ════════════════════════════════════════════════════════════════

### Core Philosophy
```
Arch Linux = Rolling release, minimal, user-centric, KISS
├── pacman (package manager)
├── systemd (init + services)
├── Arch Build System (ABS, makepkg, PKGBUILD)
├── AUR (Arch User Repository)
├── mkinitcpio / dracut (initramfs)
├── GRUB / systemd-boot / limine (bootloader)
├── NetworkManager / systemd-networkd (network)
├── PipeWire / PulseAudio (audio)
├── D-Bus / Polkit (IPC + auth)
└── Archiso (ISO creation)
```

### Archiso (ISO Creation)
```
archiso profile/
├── airootfs/          ← Root filesystem overlay
│   ├── etc/           ← Config files
│   ├── usr/           ← Binaries, libraries
│   └── ...
├── packages.x86_64    ← Package list (pacman -S)
├── packages.both      ← Packages for both arches
├── profiledef.sh      ← Profile metadata
├── build.sh           ← Build script
└── syslinux/ or grub/ ← Bootloader config

Build process:
1. pacstrap → install packages to workdir/airootfs
2. Copy overlay (airootfs/* → workdir/airootfs/)
3. Generate initramfs (mkinitcpio)
4. Create squashfs (airootfs → airootfs.sfs)
5. Create ISO (xorriso) with bootloader
```

---

## ═══════════════════════════════════════════════════════════════
## PACMAN (Package Manager)
## ════════════════════════════════════════════════════════════════

### Features
| Feature | pacman | WuBuOS wubu_pkgmgr |
|---------|--------|---------------------|
| Repositories | core, extra, community, multilib | Stub only |
| Database | Local SQLite (/var/lib/pacman/local) | Missing |
| Dependencies | Automatic resolution (deps (alpm) | Missing |
| Conflicts | Replaces, conflicts, provides | Missing |
| Hooks | Pre/Post transaction (install, upgrade, remove) | Missing |
| Signatures | PGP verification (pacman-key) | Missing |
| Delta updates | Binary diffs (xdelta3) | Missing |
| Parallel downloads | Multiple mirrors, metalink | Missing |
| Cache | /var/cache/pacman/pkg (cleanable) | Missing |
| Rollback | `pacman -U /var/cache/pacman/pkg/...` | Missing |
| AUR helper | yay, paru, pamac (external) | Missing |

### Package Format (.pkg.tar.zst)
```
package/
├── .PKGINFO          ← Metadata (name, version, desc, depends, etc.)
├── .BUILDINFO        ← Build reproducibility info
├── .MTREE            ← File integrity (mtree)
├── .INSTALL          ← Install script (pre/post install/upgrade/remove)
├── .CHANGELOG        ← Optional changelog
├── usr/              ← Files
└── ...
```

### alpm (Arch Linux Package Management) Library
- C library used by pacman, pamac, yay
- Transactions: add, remove, upgrade, sync
- Dependency resolution: SAT solver (libsolv)
- Signature verification: libarchive + gpgme
- Database: SQLite (local), tar.gz (sync)

---

## ═══════════════════════════════════════════════════════════════
## ARCH BUILD SYSTEM (ABS)
## ════════════════════════════════════════════════════════════════

### PKGBUILD Format
```bash
# Maintainer: Name <email>
pkgname=package-name
pkgver=1.0.0
pkgrel=1
pkgdesc="Description"
arch=(x86_64)
url="https://example.com"
license=(GPL)
depends=(dep1 dep2)
makedepends=(build-dep1)
checkdepends=(test-dep1)
optdepends=('opt-dep: description')
provides=(virtual-pkg)
conflicts=(conflicting-pkg)
replaces=(old-pkg)
backup=(etc/config.conf)
source=(https://example.com/src.tar.gz
        local.patch)
sha256sums=(...)

prepare() {
    cd "$srcdir/$pkgname-$pkgver"
    patch -p1 < ../local.patch
}

build() {
    cd "$srcdir/$pkgname-$pkgver"
    ./configure --prefix=/usr
    make
}

check() {
    cd "$srcdir/$pkgname-$pkgver"
    make check
}

package() {
    cd "$srcdir/$pkgname-$pkgver"
    make DESTDIR="$pkgdir" install
    install -Dm644 config.conf "$pkgdir/etc/config.conf"
}
```

### makepkg
- Reads PKGBUILD
- Downloads sources (verifies checksums)
- Prepares (prepare())
- Builds (build()) in clean chroot (optional)
- Tests (check())
- Packages (package()) → .pkg.tar.zst
- Generates .PKGINFO, .BUILDINFO, .MTREE
- Signs package (optional)

### devtools (Clean Chroot Build)
```
extra-x86_64-build  ← Build in clean chroot
multilib-build      ← Multilib chroot
archbuild           ← Generic
```
- Creates minimal chroot (base-devel + deps)
- Builds isolated from host
- Reproducible builds

---

## ═══════════════════════════════════════════════════════════════
## AUR (Arch User Repository)
## ════════════════════════════════════════════════════════════════

### Architecture
```
AUR = Git repositories (one per package)
├── PKGBUILD + sources (patches, .install, etc.)
├── No binary packages (source only)
├── Community maintained
├── Voting/popularity
├── Comments/flags
└── RPC API (JSON)

AUR Helpers (yay, paru, pamac):
1. Search AUR (RPC)
2. Download PKGBUILD + sources
3. Verify checksums
4. Build with makepkg
5. Install with pacman -U
```

### Key Stats
- 80,000+ packages
- Source-only (no binaries)
- Trust model: User verifies PKGBUILD
- Can be malicious (user responsibility)

---

## ═══════════════════════════════════════════════════════════════
## SYSTEMd (Init + Services)
## ════════════════════════════════════════════════════════════════

### Unit Types
| Type | Purpose | WuBuOS |
|------|---------|--------|
| service | Daemons, oneshot | Missing |
| socket | Socket activation | Missing |
| timer | Calendar/monotonic events | Missing |
| target | Grouping (graphical.target) | Missing |
| path | Inotify path watching | Missing |
| automount | On-demand mount | Missing |
| swap | Swap devices | Missing |
| mount | Filesystem mount | Missing |
| device | Kernel device | Missing |
| scope | External process group | Missing |
| slice | Resource control (cgroups) | Missing |

### Key Features
- **Parallel startup**: Dependency graph, socket activation
- **Cgroups v2**: Resource limits (CPU, memory, I/O, devices)
- **Journal**: Structured logging (journald), persistent
- **systemd --user**: Per-user service manager
- **Machinectl**: Container/VM management
- **Portable services**: Rootless, image-based
- **systemd-nspawn**: Lightweight containers
- **systemd-resolved**: DNS, DNSSEC, DoT, DoH
- **systemd-networkd**: Network config (alternative to NM)
- **systemd-homed**: Portable home directories
- **systemd-sysusers**: User/group management

---

## ═══════════════════════════════════════════════════════════════
## INITRAMFS (mkinitcpio / dracut)
## ════════════════════════════════════════════════════════════════

### mkinitcpio (Arch Default)
```
mkinitcpio.conf:
MODULES=(ext4 btrfs nvme)
BINARIES=(/usr/bin/btrfsck)
FILES=(/etc/crypttab)
HOOKS=(base udev autodetect modconf block filesystems keyboard fsck)

Hooks:
base       ← Minimal init, /init
udev       ← Device nodes, firmware
autodetect ← Only needed modules
modconf    ← Module config
block      ← Block devices (sd, nvme, virtio)
filesystems← Mount root fs
keyboard   ← Keyboard for encryption
fsck       ← Filesystem check
encrypt    ← LUKS decryption
lvm2       ← LVM volumes
raid       ← mdadm arrays
btrfs      ← Btrfs subvolumes
zfs        ← ZFS pools
```

### dracut (Fedora/RHEL, Optional on Arch)
- Event-driven (udev)
- Modules: kernel, network, storage, fs, etc.
- More complex, more features (network boot, iSCSI, etc.)

### WuBuOS create-initramfs.sh
- Basic: kernel modules, busybox, mount root
- Missing: hooks, encryption, LVM, RAID, Btrfs, ZFS, network boot

---

## ═══════════════════════════════════════════════════════════════
## BOOTLOADERS (GRUB / systemd-boot / limine)
## ════════════════════════════════════════════════════════════════

### GRUB (Grand Unified Bootloader)
- Multiboot spec
- Modules: filesystem, crypto, video, network
- Config: /boot/grub/grub.cfg (generated)
- Themes, graphical menu
- Secure Boot: shim + GRUB signed
- EFI + BIOS

### systemd-boot (systemd-bootx64.efi)
- UEFI only
- Simple: entries in /boot/loader/entries/*.conf
- No menu editing at boot
- Fast, minimal
- Secure Boot: signed

### limine (WuBuOS Current)
- Modern, fast, UEFI + BIOS
- Config: limine.cfg
- Modules: linux, multiboot, chainload
- Secure Boot: limine signed
- WuBuOS: limine.conf exists ✅

---

## ═══════════════════════════════════════════════════════════════
## NETWORKING (NetworkManager / systemd-networkd)
## ════════════════════════════════════════════════════════════════

### NetworkManager
- D-Bus API
- Devices: Ethernet, WiFi, WWAN, Bluetooth, VPN, WireGuard, VLAN, bond, bridge, team
- Profiles: keyfile, ifcfg, ibft
- Secrets: libsecret, gnome-keyring, kwallet
- DNS: systemd-resolved, dnsmasq, unbound, none
- VPN plugins: openvpn, wireguard, openconnect, strongswan, libreswan, pptp, l2tp
- Dispatcher: scripts on events
- nmcli / nmtui / GUI applets

### systemd-networkd
- .network, .netdev, .link files
- systemd-resolved integration
- Simpler, no D-Bus for config
- No WiFi scanning (use iwd)

### WuBuOS wubu_network.c
- Netlink: bridge, macvlan, ipvlan, vxlan, dummy, geneve, vrf, team, bond
- Missing: WiFi, WWAN, VPN, DNS, DHCP client, profiles, secrets, dispatcher

---

## ═══════════════════════════════════════════════════════════════
## AUDIO (PipeWire / PulseAudio)
## ═══════════════════════════════════════════════════════════════

### PipeWire (Modern Default)
- Graph: nodes (sources/sinks/filters), ports, links
- Session manager: WirePlumber (Lua scripts)
- Bluetooth: BlueZ 5 + pipewire-pulse + pipewire-bluetooth
- Pro Audio: JACK API, low latency
- Video: V4L2, libcamera, screen capture (portal)
- Compatibility: PulseAudio, JACK, ALSA APIs

### PulseAudio (Legacy)
- Network audio (RTP, TCP)
- Module system
- D-Bus API

### WuBuOS
- Missing entirely → audio/wubu_audio.c has 13 void casts + placeholders

---

## ═══════════════════════════════════════════════════════════════
## SECURITY (AppArmor / SELinux / landlock / seccomp)
## ═══════════════════════════════════════════════════════════════

| Feature | Arch | WuBuOS |
|---------|------|--------|
| AppArmor | Optional (apparmor package) | Missing |
| SELinux | Optional (selinux package) | Missing |
| landlock | Kernel 5.13+, unprivileged sandboxing | Missing |
| seccomp-bpf | Kernel, used by containers | wubu_ct_isolate stub |
| capabilities | File capabilities, ambient | Missing |
| secure boot | shim + GRUB/systemd-boot/limine | limine.conf ✅ |
| dm-verity | Read-only root verification | Missing |
| fsverity | File integrity (package verification | Missing |
| TPM2 | systemd-cryptenroll, PCR binding | Missing |

---

## ═══════════════════════════════════════════════════════════════
## USER MANAGEMENT (systemd-homed / systemd-sysusers)
## ════════════════════════════════════════════════════════════════

### systemd-homed
- Portable home directories (LUKS, Btrfs, fscrypt, directory)
- JSON records (not /etc/passwd)
- Roaming: home follows user across machines
- Resource limits per user
- Offline authentication

### systemd-sysusers
- Declarative user/group creation
- /usr/lib/sysusers.d/*.conf
- Runs at package install / boot

### WuBuOS
- Missing → No user management, no portable homes

---

## ═══════════════════════════════════════════════════════════════
## CONTAINERS (systemd-nspawn / podman / docker)
## ════════════════════════════════════════════════════════════════

### systemd-nspawn
- Lightweight: chroot + namespaces + cgroups
- machinectl: list, start, stop, login, pull, raw
- Portable services: rootless, image-based
- --bind, --bind-ro, --private-network, --ephemeral
- systemd inside container (full init)

### podman / docker
- OCI images
- Rootless (podman)
- systemd integration (quadlet)
- Buildah (build)

### WuBuOS wubu_ct_isolate.c
- bwrap fork+exec ✅
- User/PID ns ✅
- Missing: network, IPC, UTS, cgroup ns, seccomp, GPU passthrough, OCI images

---

## ═══════════════════════════════════════════════════════════════
## KEY GAPS FOR WUBUOS (Triple DA Verdict)
## ════════════════════════════════════════════════════════════════

### MUST HAVE (Arch Base Parity)
1. **pacman + alpm** — Package manager, deps, hooks, signatures, rollback
2. **systemd** — Init, services, sockets, timers, cgroups, journal, --user
3. **mkinitcpio / dracut** — Initramfs with hooks (encrypt, lvm, raid, btrfs, zfs)
4. **NetworkManager** — WiFi, VPN, WWAN, profiles, secrets, DNS
5. **PipeWire + WirePlumber** — Audio graph, Bluetooth, pro audio, screen capture
6. **D-Bus + Polkit** — IPC, service activation, privilege escalation
7. **AUR support** — PKGBUILD, makepkg, devtools, AUR helper
8. **Arch Build System** — PKGBUILD format, makepkg, clean chroot builds

### SHOULD HAVE (Distro Completeness)
9. **Bootloader** — GRUB/systemd-boot/limine with Secure Boot
10. **AppArmor/SELinux/landlock** — MAC, sandboxing
11. **systemd-homed/sysusers** — Portable homes, declarative users
12. **systemd-nspawn / podman** — Containers, OCI images
13. **systemd-resolved** — DNS, DoT, DoH, DNSSEC
14. **systemd-networkd** — Alternative network config
15. **CUPS** — Printing, IPP, drivers
16. **Flatpak + Portals** — Sandboxed apps, file chooser, screenshot
17. **GRUB-Btrfs snapshots** — Boot-time rollback

### NICE TO HAVE (Polish)
18. **plymouth** — Boot splash
19. **fwupd** — Firmware updates
20. **tpm2-tools** — TPM2 enrollment, PCR binding
21. **ostree** — Atomic updates (like SteamOS)
22. **zram-generator** — Compressed swap
23. **power-profiles-daemon** — Performance/balanced/power-saver
24. **bolt** — Thunderbolt authorization
25. **fprintd** — Fingerprint auth

---

## ════════════════════════════════════════════════════════════════
## IMPLEMENTATION PRIORITY FOR WUBUOS
## ════════════════════════════════════════════════════════════════

| Phase | Component | WuBuOS File | Est. Effort |
|-------|-------------|-------------|-------------|
| 1 | pacman + alpm (lib) | `runtime/wubu_pacman.c` | 8 sessions |
| 2 | PKGBUILD parser + makepkg | `runtime/wubu_makepkg.c` | 6 sessions |
| 3 | AUR RPC client | `apps/wubu_aur.c` | 3 sessions |
| 4 | systemd (init + units) | `runtime/wubu_systemd.c` | 10 sessions |
| 5 | mkinitcpio hooks | `deploy/wubu_mkinitcpio.c` | 4 sessions |
| 6 | NetworkManager client | `wubu_network.c` (extend) | 6 sessions |
| 7 | PipeWire + WirePlumber | `audio/wubu_audio.c` | 8 sessions |
| 8 | D-Bus daemon + libdbus | `runtime/wubu_dbus.c` | 6 sessions |
| 9 | Polkit agent + backend | `apps/wubu_polkit.c` | 4 sessions |
| 10 | systemd-homed / sysusers | `runtime/wubu_homed.c` | 4 sessions |
| 11 | systemd-nspawn / OCI | `wubu_ct_isolate.c` (extend) | 5 sessions |
| 12 | AppArmor/landlock | `kernel/wubu_apparmor.c` | 4 sessions |
| 13 | Secure Boot (shim + limine) | `deploy/wubu_secureboot.c` | 3 sessions |
| 14 | Flatpak + Portals | `runtime/wubu_flatpak.c` | 6 sessions |
| 15 | CUPS printing | `apps/wubu_cups.c` | 3 sessions |

**Total estimated**: ~86 sessions for full Arch parity