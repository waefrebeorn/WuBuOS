# WuBuOS Distro Roadmap — ~3000 REAL_GAP Aligned (Triple DA Verified)

## Vision
**Arch NT + Proton + HolyC DOS on modified Linux 6.x kernel — single hosted binary runs everywhere.**

## Tier Map (DA-Gap-Aligned)

### FOUNDATION — ✅ COMPLETE
- Hosted binary: Wayland + xdg-shell + SHM double-buffer
- ZealOS kernel in-process (memory, tasking, VBE, input, interrupt, FAT32, AHCI, TXFS)
- HolyC compiler: lexer, parser, AST, x86_64 codegen, JIT mmap exec
- Styx/9P namespace: client + server (styxfs_server.c — 11/11 tests)
- Container runtime: wubu_ct (chroot) + wubu_ct_bwrap (bubblewrap)
- Proton: wubu_proton2.c (real Wine+DXVK+VKD3D in Arch container)
- Container isolation: cgroups v2 (mem/cpu/pids) + seccomp-bpf profiles
- Fable windowing agent: dosgui_wm, dosgui_desktop, dosgui_startmenu
- VBE primitives: 64-glyph font, gradient/circle/shade/clip, window chrome
- wubu_audio.c: 30+ chip emulations, Furnace tracker, SF2, DAW, AI plugins — 14/14 tests
- BearRL: cartpole physics, GAAD, PPO, holographic optimization
- **GAPS REMAINING**: ~3000 (DA-verified)

---

### CRITICAL TIER — CODE-LEVEL CLOSURE (1423 gaps)
Target: Close all stub/return/void_cast/system/TODO gaps in existing files

| Component | DA Gaps | Target |
|-----------|---------|--------|
| wubu_holyd.c — HolyC Daemon | 57 | REPL wired, compiler state real, snprintf dynamic, eval/compile |
| wubu_metal.c — Metal Abstraction | 55 | 5 empty `{}` shutdown/flip, audio backends real impl, X11/Vulkan |
| wubu_network.c — Netlink | 52 | system() → netlink/ioctl, system("tc") → QoS, system("wg") → WireGuard |
| styxfs.c — 9P Server | 51 | All callbacks real, offset tracking, walk/read/create/remove/clunk |
| interrupt.c — IRQ Subsystem | 47 | CPUID LAPIC, MSI/MSI-X, SYSCALL_STACK, TSC deadline, IOAPIC MSI |
| wubu_snapshot.c — Overlay FS | 43 | Real mount/umount error handling, nftw() GC, btrfs/zfs native |
| wubu_oci.c — OCI Runtime | 41 | TLS (mbedTLS), streaming blob I/O, no system("cp") |
| styx.c — 9P Protocol | 41 | All handlers real impl |
| wubu_x86.c — JIT Emitter | 36 | Rel32 backpatch robust, JCC/JMP label management |
| vsl_syscall.c — Syscall Bridge | 36 | 173 void casts = ABI (d,e,f); namespaces/fanotify/landlock/bpf |
| wubu_image.c — Image Builder | 33 | nftw() cleanup, multi-stage exec, base image fetch |
| wubu_archd.c — Arch Daemon | 27 | system("pacman") → fork+exec, AUR/PKGBUILD/makepkg |
| wubu_gamelib.c — Game Library | 25 | Steam/GOG/Epic API, const-correct |
| wubu_syscall.c — Bridge | 23 | Trampolines real, fd/Styx/container wiring complete |
| wubu_vulkan.c — Vulkan Loader | 23 | Loader chaining, multi-GPU, surface init, swapchain recreate |
| wubu_proton.c (gui) — Proton | 22 | Wine fork+exec, DXVK lib load, prefix mgmt |
| jit.c — JIT Core | 20 | Encoder/decoder real |
| wubu_proton.c (runtime) — Proton | 20 | PE validation, Wine exec, compat DB |
| holyc_codegen.c — Codegen | 19 | Rel32 patching, label management |
| wubu_exec.c — Exec Dispatcher | 18 | Native exec, C compile, handler routing |
| wubu_audio.c — Audio Engine | 18 | PipeWire/Pulse real, device enumeration, mixer |
| wubu_ct_isolate.c — Container Isolation | 15 | cgroups write, seccomp install, ns unshare |
| wubu_session.c — Session Manager | 15 | Save/restore real, compiler state persist |
| dosgui_startmenu_test_stub.c | 16 | Stubs → real |
| wubu_bottles.c — Bottles Manager | 16 | Import/export real, .wubu manifest |
| wubu_ct_bwrap.c — Bubblewrap | 14 | Args real, env pass, cleanup |
| wubu_pkgmgr.c — Package Manager | 14 | SQLite real, deps graph, hooks fire |
| wubu_ramdisk.c — Ramdisk | 13 | Mount real, cleanup |

---

### CRITICAL TIER — ARCHITECTURAL PARITY CLOSURE (1572 gaps) — PARALLEL STREAMS

**NO PICK ONE — ALL STREAMS PARALLEL UNTIL 3000 → 0**

#### Stream A: SteamOS Parity (~400)
| Component | Target | Key Implementation |
|-----------|--------|-------------------|
| Steam Client CEF UI | Full | Embed Chromium/CEF in hosted binary, store/library/friends/overlay |
| Steam Input | Full | SDL2/GameControllerDB + haptics via Linux uinput, gyro, touch menus |
| Steam Networking | Full | SteamNetworkingSockets lib + relay integration, P2P, NAT traversal |
| Proton (Wine+DXVK+VKD3D) | Full | DXVK/VKD3D + Wine patches + D3DMetal → wubu_proton.c |
| gamescope | Full | Wayland compositor + VRR/HDR/FSR + upscaling → hosted.c + wubu_proton2.c |
| Pressure Vessel | Full | Container runtime + seccomp profiles + namespace mgmt → wubu_ct_isolate.c |
| Shader Pre-cache | Full | fossilize + dxvk-cache background compiler |
| ProtonDB | Full | Compat database + community sync |
| Steam Cloud | Full | Remote storage sync + conflict resolution + app integration |
| Steam Deck UI | Full | Game mode, desktop mode, quick access, notifications, power mgmt |

#### Stream B: Ubuntu/Arch Parity (~450)
| Component | Target | Key Implementation |
|-----------|--------|-------------------|
| systemd | Full | Init + service manager + journal + logind + timers + sockets → wubu_init.c |
| apt/pacman | Full | Package manager + repos + deps + hooks + transactions + signing → extend wubu_pkgmgr.c |
| NetworkManager | Full | WiFi + VPN + DNS + DHCP + bonding + VLAN → extend wubu_network.c |
| Polkit | Full | Authorization + privilege escalation + actions + rules → wubu_polkit.c |
| D-Bus | Full | System/session bus + activation + introspection + monitoring → wubu_dbus.c |
| PipeWire | Full | Audio graph + bluetooth + devices + modules + session manager → extend wubu_audio.c |
| CUPS | Full | Printing + IPP + drivers + backends + PPD → wubu_cups.c |
| AppArmor/SELinux | Full | MAC + profiles + policy management + audit → wubu_apparmor.c |
| systemd-homed | Full | User management + portable home + JSON records |
| mkinitcpio/dracut | Full | Initramfs generation + hooks + modules |
| GRUB/limine | Full | Bootloader + secure boot + TPM + measured boot |

#### Stream C: TempleOS Parity (~350)
| Component | Target | Key Implementation |
|-----------|--------|-------------------|
| HolyC JIT AOT+JIT | Full | Whole-program optimization + inline assembly → holyc_codegen.c + jit.c |
| Doc/DolDoc | Full | Hyperlinked docs + graphics + songs + CTree + forms → wubu_dolDoc.c |
| Compiler-as-library | Full | JIT compile from string + AST manipulation + symbol table API → holyc_lib.c |
| RedSea FS | Full | Database filesystem + no paths + tags/attributes + versioning → styxfs_redsea.c |
| Identity-mapped memory | Full | Ring-0 memory model + no user paging → memory_identity.c |
| Ring-0 everything | Full | Single address space + no protection + HolyC task = VSL process |
| VGA/VESA direct | Full | Software renderer fallback + no GPU drivers |
| PC speaker + raw PCM | Full | Beep + raw PCM → wubu_audio.c PCM backend |

#### Stream D: ZealOS Parity (~200)
| Component | Target | Key Implementation |
|-----------|--------|-------------------|
| Identity-mapped memory | Full | VSL mmap provides identity, not virtual |
| VGA/VESA direct | Full | Software renderer fallback |
| PC speaker audio | Full | Raw PCM beep |
| God word / Oracle | Philosophical | Divine intellect component (documentation) |
| ZealOS name parity | 96/96 DONE | zealos_parity.h complete |

#### Stream E: ReactOS NT Emulation (~162) — NEW MISSION
| Component | ReactOS Source | WuBuOS Target |
|-----------|----------------|---------------|
| Syscall Dispatch | sysfuncs.lst (297) | vsl_syscall_list.h (DONE) |
| Thread Scheduling | ke/thrdschd.c + thrdobj.c | tasking.c + VSL clone/fork |
| Memory Manager (VAD) | mm/*.c | memory.c VAD tree + VSL mmap/brk |
| Object Manager | ob/*.c | Styx9 fid/namespace + VSL fd table |
| I/O Manager (IRP) | io/*.c | VSL read/write/ioctl + Styx9 9P |
| Win32k | user/*.c + gdi/*.c | Win98 WM + dosgui_wm.c |
| NTDLL Stubs | dll/ntdll/*.c | VSL syscall entry points |
| Registry | config/*.c (CM_KEY_BODY) | Styx9 registry namespace |

---

### HIGH TIER — KERNEL CLOSURE (67 gaps)

| Component | DA Gaps | Target |
|-----------|---------|--------|
| interrupt.c — IRQ Subsystem | 23 | CPUID for LAPIC, MSI/MSI-X, SYSCALL_STACK, TSC deadline, IOAPIC MSI |
| fat32.c — Filesystem | 12 | LFN support, lfn_chk verify, cluster bitmap cache, pre-allocation |
| txfs.c — Transactional FS | 10 | Atomic commit (rename), replay verification, auto-checkpoint |
| ahci.c — SATA Driver | 8 | FIS receive setup, real PHY management, interrupt completion |
| memory.c — Heap | 8 | Multi-region, alloc-time canary, fragmentation stats |
| tasking.c — Scheduler | 6 | Priority scheduler, FPU/SSE save, preemption |
| wubu_math.c — Libm | 8 | Taylor series → minimax, atan2 quadrant fix, Newton-Raphson iterations |

---

### HIGH TIER — GUI SHELL CLOSURE (89 gaps)

| Component | DA Gaps | Target |
|-----------|---------|--------|
| dosgui_wm.c — Window Manager | 28 | Badge buffer fix, snap-to-grid, focus config, edge resistance |
| dosgui_explorer.c — File Manager | 18 | nftw() file ops, realpath error handling, find implementation |
| dosgui_startmenu.c — Start Menu | 10 | Dynamic alloc, fs watcher for programs DB |
| dosgui_desktop.c — Desktop | 12 | Wallpaper image load, icon auto-arrange, real FS watch |
| dosgui_term.c — Terminal | 8 | ANSI parser, pty management, HolyC REPL session |
| wubu_theme.c — Theme Engine | 5 | Theme file loading (INI/JSON), CSS parser, runtime customization |
| wubu_notify.c — Notifications | 4 | Timeout dismissal, history, actions |
| wubu_proton/gamelib.c — Game Lib | 4 | Steam/GOG/Epic API stubs |

---

### HIGH TIER — BEAR RL CLOSURE (54 gaps)

| Component | DA Gaps | Target |
|-----------|---------|--------|
| bear_nn.c — Neural Net | 18 | Checkpoint save/load, SIMD/AVX, backward pass impl |
| bear_ppo.c — PPO | 12 | V-Trace off-policy, GAE lambda, adaptive clip |
| bear_vulkan.c — Vulkan Compute | 14 | Forward/GAE/env dispatch, pipeline composition |
| bear_cudnn.c — cuDNN | 6 | Handle creation, conv/activation/pooling impl |
| bear_vulkan_soft.c — CPU Fallback | 4 | API implementations |

---

### HIGH TIER — HOSTED PLATFORM CLOSURE (38 gaps)

| Component | DA Gaps | Target |
|-----------|---------|--------|
| wubu_drm_direct.c — DRM/KMS | 14 | Atomic commit, planes, modifiers, hotplug/EDID, autogenerated uAPI |
| wubu_vulkan.c — Vulkan Loader | 10 | Loader API chaining, multi-GPU scoring, surface init, swapchain recreate |
| wubu_metal.c — Metal Abstraction | 8 | DRM/ALSA/Pulse/X11 shutdown impl, GAAD integration |
| wubu_gbm.c — GBM | 3 | Modifiers, multi-planar, format negotiation |
| hosted.c — Hosted Binary | 3 | Styx 9P dir read/write impl |

---

### MEDIUM TIER — COMPILER & APPS (22 gaps)

| Component | DA Gaps | Target |
|-----------|---------|--------|
| holyc_codegen.c — Codegen | 5 | Robust rel32 patching, label management |
| holyc_ptx.c — PTX Backend | 4 | Shared memory tiling, CUDA driver init, kernel launch |
| wubu_editor.c — Editor | 5 | Code folding, parser-based highlight, regex search |
| wubu_canvas.c — Canvas | 4 | Drawing primitives, filters, canvas ops |
| wubu_codec.c — Codec | 2 | FFmpeg/libav integration |
| control/explorer/calc — Shutdown | 2 | Real shutdown logic |

---

## DA Rule
**"Rewriting from scratch in C" = the work.**
- Each component above = one or more REAL_GAPs from BATTLESHIP.md
- Pick gaps from ALL streams in PARALLEL
- Close gaps, run tests, move to next
- **~3000 → 0 is the victory condition**