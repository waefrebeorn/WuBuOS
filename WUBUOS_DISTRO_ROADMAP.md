# WuBuOS Distro Roadmap — 412 REAL_GAP Aligned (Triple DA Verified)

## Vision
**Arch NT + Proton + HolyC DOS on modified Linux 6.x kernel — single hosted binary runs everywhere.**

## Phase Map (DA-Gap-Aligned)

### PHASE 0: FOUNDATION ✅ DONE (Cells 200-425)
- Hosted binary: Wayland + xdg-shell + SHM double-buffer
- ZealOS kernel in-process (memory, tasking, VBE, input, interrupt, FAT32, AHCI, TXFS)
- HolyC compiler: lexer, parser, AST, x86_64 codegen, JIT mmap exec
- Styx/9P namespace: client + server (styxfs_server.c — 11/11 tests)
- Container runtime: wubu_ct (chroot) + wubu_ct_bwrap (bubblewrap)
- Proton: wubu_proton2.c (real Wine+DXVK+VKD3D in Arch container)
- Container isolation: cgroups v2 (mem/cpu/pids) + seccomp-bpf profiles
- Fable windowing agent: dosgui_wm, dosgui_desktop, dosgui_startmenu
- VBE primitives: 64-glyph font, gradient/circle/shade/clip, window chrome
- wubu_audio.c: 12 chip emulations [NOT IMPLEMENTED — ARCHITECTURAL GAP]
- BearRL: cartpole physics, GAAD, PPO, holographic optimization
- **GAPS REMAINING**: 412 (DA-verified)

### PHASE 1: RUNTIME CLOSURE (Cells 426-480) — CRITICAL TIER
Target: Close 142 runtime gaps + 67 kernel gaps = 209 gaps

| Cell | Component | DA Gaps | Target |
|------|-----------|---------|--------|
| 426 | wubu_network.c — Netlink | 28 | bridge, macvlan, ipvlan, vxlan, wireguard, tailscale, QoS, DNS via netlink/ioctl |
| 427 | wubu_oci.c — OCI Runtime | 31 | TLS (mbedTLS), streaming blob I/O, base64, registry auth refresh |
| 428 | wubu_snapshot.c — Overlay FS | 24 | real mount/umount error handling, nftw() GC, btrfs/zfs native paths |
| 429 | wubu_holyd.c — HolyC Daemon | 22 | snprintf dynamic alloc, void casts → real logic, eval/compile wired |
| 430 | wubu_vsl.c — Syscall Bridge | 18 | ELF PT_LOAD iter, syscall handler impl, driver (Vulkan/CUDA/NET) wiring |
| 431 | wubu_image.c — Image Builder | 12 | nftw() cleanup, multi-stage exec, base image fetch |
| 432 | wubu_proton.c — Proton Config | 7 | fork+exec Wine launch, DXVK lib loading, prefix management |
| 433 | wubu_archd.c — Arch Daemon | 0 | (functional — defensive returns only) |
| 434 | wubu_bottles.c — Bottles Mgr | 0 | (functional — defensive returns only) |
| 435 | wubu_exec.c — Exec Dispatcher | 0 | (functional) |
| 436 | wubu_proton2.c — Proton PE | 0 | (functional — defensive returns only) |
| 437 | wubu_ramdisk.c — Ramdisk | 0 | (functional — defensive returns only) |

### PHASE 2: KERNEL CLOSURE (Cells 481-520) — CRITICAL TIER
Target: Close 67 kernel gaps

| Cell | Component | DA Gaps | Target |
|------|-----------|---------|--------|
| 481 | interrupt.c — IRQ Subsystem | 23 | CPUID for LAPIC, MSI/MSI-X, SYSCALL_STACK, TSC deadline, IOAPIC MSI |
| 482 | fat32.c — Filesystem | 12 | LFN support, lfn_chk verify, cluster bitmap cache, pre-allocation |
| 483 | txfs.c — Transactional FS | 10 | Atomic commit (rename), replay verification, auto-checkpoint |
| 484 | ahci.c — SATA Driver | 8 | FIS receive setup, real PHY management, interrupt completion |
| 485 | memory.c — Heap | 8 | Multi-region, alloc-time canary, fragmentation stats |
| 486 | tasking.c — Scheduler | 6 | Priority scheduler, FPU/SSE save, preemption |
| 487 | wubu_math.c — Libm | 8 | Taylor series → minimax, atan2 quadrant fix, Newton-Raphson iterations |

### PHASE 3: GUI SHELL CLOSURE (Cells 521-560) — HIGH TIER
Target: Close 89 GUI gaps

| Cell | Component | DA Gaps | Target |
|------|-----------|---------|--------|
| 521 | dosgui_wm.c — Window Manager | 28 | Badge buffer fix, snap-to-grid, focus config, edge resistance |
| 522 | dosgui_explorer.c — File Manager | 18 | nftw() file ops, realpath error handling, find implementation |
| 523 | dosgui_startmenu.c — Start Menu | 10 | Dynamic alloc, fs watcher for programs DB |
| 524 | dosgui_desktop.c — Desktop | 12 | Wallpaper image load, icon auto-arrange, real FS watch |
| 525 | dosgui_term.c — Terminal | 8 | ANSI parser, pty management, HolyC REPL session |
| 526 | wubu_theme.c — Theme Engine | 5 | Theme file loading (INI/JSON), CSS parser, runtime customization |
| 527 | wubu_notify.c — Notifications | 4 | Timeout dismissal, history, actions |
| 528 | wubu_proton/gamelib.c — Game Lib | 4 | Steam/GOG/Epic API stubs |

### PHASE 4: BEAR RL CLOSURE (Cells 561-580) — HIGH TIER
Target: Close 54 Bear gaps

| Cell | Component | DA Gaps | Target |
|------|-----------|---------|--------|
| 561 | bear_nn.c — Neural Net | 18 | Checkpoint save/load, SIMD/AVX, backward pass impl |
| 562 | bear_ppo.c — PPO | 12 | V-Trace off-policy, GAE lambda, adaptive clip |
| 563 | bear_vulkan.c — Vulkan Compute | 14 | Forward/GAE/env dispatch, pipeline composition |
| 564 | bear_cudnn.c — cuDNN | 6 | Handle creation, conv/activation/pooling impl |
| 565 | bear_vulkan_soft.c — CPU Fallback | 4 | API implementations |

### PHASE 5: HOSTED PLATFORM CLOSURE (Cells 581-600) — HIGH TIER
Target: Close 38 Hosted gaps

| Cell | Component | DA Gaps | Target |
|------|-----------|---------|--------|
| 581 | wubu_drm_direct.c — DRM/KMS | 14 | Atomic commit, planes, modifiers, hotplug/EDID, autogenerated uAPI |
| 582 | wubu_vulkan.c — Vulkan Loader | 10 | Loader API chaining, multi-GPU scoring, surface init, swapchain recreate |
| 583 | wubu_metal.c — Metal Abstraction | 8 | DRM/ALSA/Pulse/X11 shutdown impl, GAAD integration |
| 584 | wubu_gbm.c — GBM | 3 | Modifiers, multi-planar, format negotiation |
| 585 | hosted.c — Hosted Binary | 3 | Styx 9P dir read/write impl |

### PHASE 6: COMPILER & APPS (Cells 601-620) — MEDIUM TIER
Target: Close 9 Compiler + 13 Apps gaps

| Cell | Component | DA Gaps | Target |
|------|-----------|---------|--------|
| 601 | holyc_codegen.c — Codegen | 5 | Robust rel32 patching, label management |
| 602 | holyc_ptx.c — PTX Backend | 4 | Shared memory tiling, CUDA driver init, kernel launch |
| 603 | wubu_editor.c — Editor | 5 | Code folding, parser-based highlight, regex search |
| 604 | wubu_canvas.c — Canvas | 4 | Drawing primitives, filters, canvas ops |
| 605 | wubu_codec.c — Codec | 2 | FFmpeg/libav integration |
| 606 | control/explorer/calc — Shutdown | 2 | Real shutdown logic |

### PHASE 7: ARCHITECTURAL GAPS (Entire Subsystems) — STRATEGIC
These are NOT in the 412 — they require new subsystems:

| Cell | Subsystem | Components | Effort |
|------|-----------|------------|--------|
| 701 | Audio Engine | Furnace (12 chips), SF2 parser, Ardour DAW, AI plugins | ~2000 LOC |
| 702 | SteamOS Integration | Steam client, Proton, gamescope, Pressure Vessel, Shader cache | ~5000 LOC |
| 703 | Ubuntu/Arch Integration | systemd, apt/pacman, NetworkManager, D-Bus, PipeWire, Polkit | ~5000 LOC |
| 704 | TempleOS Soul | HolyC JIT, Doc/DolDoc, Compiler-as-library, RedSea FS | ~3000 LOC |

---

## DA Rule
**"Rewriting from scratch in C" = the work.**
- Each cell above = one or more REAL_GAPs from BATTLESHIP.md v14
- Pick ONE cell, close its gaps, run tests, move to next
- 412 → 0 is the victory condition