# WuBuOS Distro Roadmap — 2284-GAP Aligned

## Vision
**Arch NT + Proton + HolyC DOS on modified Linux 6.x kernel — single hosted binary runs everywhere.**

## Phase Map (Gap-Aligned)

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
- wubu_audio.c: 12 chip emulations
- BearRL: cartpole physics, GAAD, PPO, holographic optimization
- **GAPS REMAINING**: 2284 (this audit)

### PHASE 1: RUNTIME CLOSURE (Cells 426-480) — CRITICAL TIER
Target: Close 996 runtime gaps + 254 kernel gaps = 1250 gaps

| Cell | Component | Gaps | Target |
|------|-----------|------|--------|
| 426 | wubu_oci.c — OCI Runtime | 84 | manifest, blob, config, registry HTTP, GC |
| 427 | wubu_network.c — Netlink | 122 | bridge, macvlan, ipvlan, vxlan, wireguard, tailscale, QoS, DNS |
| 428 | wubu_snapshot.c — Overlay FS | 82 | real mount/umount, dir_size, restore, branch/tag |
| 429 | wubu_holyd.c — HolyC Daemon | 75 | mouse routing, session save/restore, event loop, accept fix |
| 430 | wubu_vsl.c — Syscall Bridge | 72 | ELF PT_LOAD, fd delegation, syscall translation |
| 431 | wubu_image.c — Image Builder | 67 | multi-stage build, layer cache, base images, export |
| 432 | wubu_archd.c — Arch Daemon | 45 | root create, pacman ops, health, event publish |
| 433 | wubu_bottles.c — Bottles Mgr | 38 | import/export/run, Flatpak/Steam compat |
| 434 | wubu_exec.c — Exec Dispatcher | 35 | memfd_create, C compile, Mach-O, custom handlers |
| 435 | wubu_proton.c — Proton Config | 52 | DXVK HUD/async/nvapi/present/memory, prefix |
| 436 | wubu_proton2.c — Proton PE | 31 | GameScope, PE launch, GPU/HID/USB passthrough |
| 437 | wubu_ramdisk.c — Ramdisk | 32 | create, snapshot, restore |
| 438 | wubu_pkg.c — Package Registry | 26 | find, install, remove, update |
| 439 | wubu_container.c — Container Ops | 13 | load, start, stop |
| 440 | wubu_host_exec.c — Host Exec | 19 | bind, run, wait |
| 441 | wubu_ct_isolate.c — Isolation | 24 | cgroups write, seccomp install, ns unshare |
| 442 | wubu_arch.c — Arch Bootstrap | 16 | bootstrap, chroot, pacman init |
| 443 | wubu_gc.c — Garbage Collector | 1 | collect |
| 444 | styx.c — 9P Protocol | 52 | version, auth, walk, open, read, write, clunk, remove, stat, wstat |
| 445 | styxfs.c — 9P Client | 41 | fid ops, walk, read, write, clunk |
| 446 | styxfs_server.c — 9P Server | 44 | T-version, auth, walk, open, read, write, clunk, remove, stat, wstat |
| 447 | wubu_freedoom.c — FreeDoom | 10 | launch, resume, save |
| 448 | wubu_anticheat.c — Anti-Cheat | 19 | kernel load/unload, hooks, Wine/Proton config |
| 449 | dosgui_daemon_panel.c — Bridge | 10 | subscribe, event handling, render |

### PHASE 2: KERNEL CLOSURE (Cells 450-480) — CRITICAL TIER
| Cell | Component | Gaps | Target |
|------|-----------|------|--------|
| 450 | interrupt.c — ISR/IOAPIC/LAPIC | 111 | ISR assembly, IOAPIC routing, LAPIC timer, TSS |
| 451 | fat32.c — Filesystem | 57 | open, read, write, create, unlink, mkdir, readdir |
| 452 | tasking.c — Scheduler | 22 | spawn, kill, suspend, sleep, yield, priority, parent, queue walk |
| 453 | memory.c — Heap | 15 | walk, validate, used, available, bloom, canaries |
| 454 | ahci.c — Disk | 23 | port init, FIS recv, cmd, read, write |
| 455 | txfs.c — Transactional FS | 18 | mount, journal, txn begin/commit |
| 456 | vbe.c — Display | 6 | fill_rect, fill_circle, draw_text, swap, mode_set |
| 457 | input.c — PS/2 | 7 | scancode, mouse packet, fifo, hooks |
| 458 | ps2.c — PS/2 Driver | ? | keyboard, mouse init |

### PHASE 3: GUI SHELL CLOSURE (Cells 480-520) — HIGH TIER
| Cell | Component | Gaps | Target |
|------|-----------|------|--------|
| 480 | dosgui_wm.c — Window Manager | 44 | input dispatch, holyc_term, create/destroy/focus/render |
| 481 | wubu_proton.c — Proton GUI | 52 | DXVK config UI, prefix mgmt, env setup |
| 482 | wubu_gamelib.c — Game Lib | 36 | scan, startmenu wiring, placeholder |
| 483 | dosgui_explorer.c — Explorer | 22 | tree, breadcrumbs, list, preview, ops, context menu |
| 484 | dosgui_term.c — Terminal | 24 | PTY, tabs, ANSI, scrollback, copy/paste, search |
| 485 | dosgui_startmenu.c — StartMenu | 22 | search, recent, power, tree, shortcuts |
| 486 | dosgui_desktop.c — Desktop | 6 | icons, wallpaper, tray, placeholder |
| 487 | wubu_pkgmgr.c — Package Mgr GUI | 18 | header size/sign/crc, install UI |
| 488 | wubu_mime.c — MIME System | 25 | command_exec, type detection |
| 489 | wubu_wm.c — WM Core | 6 | invalidate, window ops |
| 490 | dosgui_controlpanel.c — Settings | 13 | applets: display, network, sound, theme |
| 491 | wubu_notify.c — Notifications | 6 | init, shutdown, tick, caps, count |
| 492 | wubu_screenshot.c — Screenshot | 26 | region select, GIF, snip tool |
| 493 | wubu_settings.c — Settings | 13 | font_scale, cursor_size, high_contrast |
| 494 | wubu_clipboard.c — Clipboard | 20 | DND action, copy/paste |
| 495 | wubu_session.c — Session Mgmt | 24 | hibernate, suspend, idle inhibit |
| 496 | wubu_trash.c — Trash | 23 | get_size, operations |

### PHASE 4: BEAR RL CLOSURE (Cells 520-560) — HIGH TIER
| Cell | Component | Gaps | Target |
|------|-----------|------|--------|
| 520 | bear_nn.c — Neural Net | 46 | checkpoint save/load, layers, optimizers, zero_grad |
| 521 | bear_vulkan.c — Vulkan Compute | 25 | forward, GAE, env step, pipelines, descriptors |
| 522 | bear_cudnn.c — cuDNN | 40 | handle, conv, activation, pooling, softmax, workspace |
| 523 | bear_cuda.c — CUDA | 24 | malloc/free, policy/value/GAE/n-pole kernels |
| 524 | bear_vulkan_soft.c — CPU Fallback | 29 | GEMM, softmax, GAE, env step implementations |
| 525 | bear_opt.c — Optimizer | 6 | zero_grads, step, LR schedule |
| 526 | bear_ppo.c — PPO | 17 | V-trace, clip, entropy, value loss |
| 527 | bear_gaad.c — GAAD | 1 | Q-learner integration |
| 528 | bear_cartpole_gaad_solve.c — Cartpole | 7 | Q-update, strain level |
| 529 | bear_env.c — Environments | 14 | n-pole, reset, step, render |
| 530 | npole_blog.c — Lagrangian | 1 | Full implementation |

### PHASE 5: HOSTED PLATFORM CLOSURE (Cells 560-600) — HIGH TIER
| Cell | Component | Gaps | Target |
|------|-----------|------|--------|
| 560 | wubu_vulkan.c — Vulkan | 51 | instance, device, swapchain, pipelines, memory |
| 561 | wubu_metal.c — Metal/DRM | 34 | DRM/KMS, ALSA, Pulse, evdev, X11, GBM |
| 562 | hosted.c — Hosted Main | 54 | Wayland frame, SHM, fs reset, display init |
| 563 | wubu_drm_direct.c — DRM Direct | 15 | device open, resources, mode_set_crtc |
| 564 | wubu_display.c — Display | 11 | init, modeset, page_flip |
| 565 | wubu_gbm.c — GBM | 11 | buffer alloc, modifier, format |

### PHASE 6: COMPILER & APPS (Cells 600-640) — MEDIUM TIER
| Cell | Component | Gaps | Target |
|------|-----------|------|--------|
| 600 | holyc_codegen.c — Codegen | 22 | stack args, multi-func, structs, ternary, calls |
| 601 | holyc_ptx.c — PTX Backend | 13 | matrix tiles, runtime exec, MMA |
| 602 | holyc_lexer.c — Lexer | 1 | return -1 paths |
| 603 | jit/wubu_x86.c — X86 Encoder | 30 | instruction emitters |
| 604 | jit/wubu_disasm.c — Disassembler | ? | decode, dump |
| 605 | jit/jit_minic.c — Minic Compiler | 9 | tokenizer, parser, expr, if/while, locals |
| 606 | wubu_editor.c — Editor | 27 | undo/redo, find, folding, bookmarks, macros |
| 607 | wubu_canvas.c — Canvas | 39 | draw, filters, ops, undo/redo, resize/crop/flip |
| 608 | wubu_codec.c — Codec | 34 | mount, video/audio decode/encode, seek |
| 609 | explorer.c — Explorer App | 8 | context menu, F2-F8 ops |
| 610 | terminal.c — Terminal App | 1 | resize |
| 611 | wubu_freedoom.c — FreeDoom App | 10 | launch, resume, save |

### PHASE 7: AUDIO ENGINE (Cells 640-680) — MEDIUM TIER
| Cell | Component | Gaps | Target |
|------|-----------|------|--------|
| 640 | wubu_audio.c — Audio Engine | 24 | SF2 sample playback, mixing, plugin API |
| 641 | Furnace chips (12) | ? | NES, GB, YM2612, SN76489, SID, SAA1099, VRC6, N163, OPL, SCC, AY, PC Speaker |
| 642 | TinySoundFont | ? | RIFF parse, samples, envelopes, modulators |
| 643 | Ardour DAW integration | ? | transport, automation, LV2/VST3, JACK |
| 644 | AI plugins | ? | neural synthesis, style transfer |

### PHASE 8: BARE METAL (Cells 680-720) — LOW TIER
| Cell | Component | Target |
|------|-----------|--------|
| 680 | Modified Linux 6.x config (CONFIG_WUBU_*) | Kernel patches |
| 681 | Limine bootloader + kernel cmdline | wubu.namespace=9p |
| 682 | GPU init: DRM/KMS + GOP fallback | vbe.c metal path |
| 683 | ACPI + PCIe + AHCI + NVMe | Hardware enumeration |
| 684 | Real hardware: ISO build, USB install | Dual-boot support |

### PHASE 9: ECOSYSTEM (Cells 720+) — LOW TIER
| Cell | Component | Target |
|------|-----------|--------|
| 720 | WuBuOS Repository | Signed packages, SBOM, reproducible |
| 721 | Hardware certification | Like Ubuntu Certified |
| 722 | ISV outreach | Game studios, CAD, DAW, AI vendors |
| 723 | Documentation | Wiki, tutorials, API reference |
| 724 | Community | Discord, forums, package maintainers |

## Gap Closure Velocity Target

| Week | Target Gaps Closed | Cumulative |
|------|-------------------|------------|
| 1-2 | 200 (OCI + Network + Snapshot) | 200 |
| 3-4 | 200 (HolyD + VSL + Image) | 400 |
| 5-6 | 200 (ArchD + Bottles + Exec + Proton) | 600 |
| 7-8 | 200 (Interrupt + FAT32 + Tasking) | 800 |
| 9-10 | 200 (GUI WM + Explorer + Terminal) | 1000 |
| 11-12 | 200 (Bear NN + Vulkan + cuDNN) | 1200 |
| 13-14 | 200 (Hosted Vulkan + Metal) | 1400 |
| 15-16 | 200 (Compiler + Apps) | 1600 |
| 17-18 | 200 (Audio + remaining) | 1800 |
| 19-20 | 200 (Bare metal prep) | 2000 |
| 21-22 | 284 (Final closure) | 2284 |

## Success Criteria
- [ ] 2284 → 0 REAL_GAPs
- [ ] All 58 test targets passing
- [ ] Hosted binary runs on Linux, WSL2, macOS AVF
- [ ] Steam + Proton + games work in containers
- [ ] HolyC DOS terminal spawns as GUI window
- [ ] GPU compute from HolyC (PTX → Tensor cores)
- [ ] Audio DAW + 12 chip emulations functional
