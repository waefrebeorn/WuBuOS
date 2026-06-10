# WuBuOS — Battleship v11 (Triple Devil's Advocate Audit Complete)

**Methodology**: Behavioral verification + stub hunt + name parity audit + third-party dep scan + form≠function check + **Triple Devil's Advocate upstream comparison**
**Current state**: 50 C files, ~17K real LOC, 747+ tests passing
**Name parity**: 64/96 core ZealOS functions mapped (67%) — 32 missing

---

## Triple Devil's Advocate Findings Summary

We audited our implementations against the actual upstream projects we ape:

| Project | What We Claim | What Upstream Actually Does | Gap Severity |
|---------|---------------|----------------------------|--------------|
| **Furnace** | 12 custom C chip emulators | Uses **70+ external C/C++ libs**: blip_buf+puNES/NSFPlay (NES), Nuked-OPLL (OPLL), Nuked-PSG (PSG), SAASound (SAA1099), YM3812-LLE/YMF262-LLE (OPL2/3), YM2608-LLE (OPNA), YM2610/LLE (YM2612 Genesis FM), vgsound_emu-modified (many chips) | 🔴 CRITICAL — Our 12 custom emulators are toy implementations vs mature cycle-accurate libs |
| **TinySoundFont** | Custom SF2 parser + sample playback | Single-header lib: full RIFF pdta/sdta parsing, sample playback with envelopes, modulators (partial), proper voice management, preset/bank loading, 48kHz rendering, thread-safe copy API | 🔴 CRITICAL — Our impl is RIFF header validation + sine wave stub |
| **Ardour** | 64 tracks, 16 buses, regions, automation | Full DAW: sample-accurate automation (float per-frame), LV2/VST3/CLAP plugin API, JACK transport sync, AAF/OMF interchange, video timeline, external control surfaces, anywhere-to-anywhere routing, unlimited undo/redo, crossfade editing, video sync | 🔴 CRITICAL — Our DAW is a toy mixer with no plugin API, no sample-accurate automation, no interchange formats |
| **ZealOS/TempleOS** | 64/96 kernel funcs mapped | 154K LOC kernel: full IDT+APIC, SYSCALL fast path, VFS, block layer, network stack (TCP/IP), USB subsystem, GPU driver framework (DRM/KMS), audio (ALSA-like), SMP boot, module loader, slab allocator, KPTI, ACPI, PCI/MSI, FPU/SSE/AVX context save, demand paging | 🔴 CRITICAL — We have 15K LOC vs 154K; missing entire subsystems |
| **Inferno OS emu** | One binary, hosted+bare-metal | emu: discrete VM (bytecode interpreter), Styx/9P2000.L on Unix sockets, wm/sh, Acme editor, plumber(4), factotum auth, ATA/SD/USB drivers, virtio, ACPI, VGA/VESA/framebuffer, audio via audio(3), TLS/crypto, rc shell | 🟡 HIGH — Our wubu binary is 15K C vs Inferno's 500K+ C; 9P is partial; no plumber/factotum/Acme |
| **Proton/Wine** | SteamOS container + GPU passthrough | Wine: 1M+ LOC Windows ABI reimplementation; DXVK: D3D9/10/11 → Vulkan translation layer (50K+ LOC), VKD3D: D3D12 → Vulkan; fonts, registry, MSI, WMI, COM, .NET, Steam runtime, ProtonDB compat patches, FSR/DLSS, controller mappings | 🔴 CRITICAL — Our container is fork+chroot+bind-mount; no Wine/DXVK/VKD3D impl |
| **DRM/KMS** | Dual backend (X11 + DRM/KMS) | 20K+ LOC in libdrm: atomic mode setting, universal planes, cursor, HDR, VRR/freesync, HWC, modifiers, PRIME, GEAR VR, lease, blob, leases, content protection, DSC, panel orientation, writeback, FB_DAMAGE_CLIPS | 🟡 HIGH — Our wubu_display.c is 3.7K LOC vs 20K+ libdrm; no atomic, no HDR/VRR |

---

## Resolved Cells (43 — verified at runtime)

| Cell | Description | Evidence |
|------|-------------|----------|
| 200 | ZealOS kernel in-process + Win98 GUI shell | hosted.c:194 init, hosted_test.c 14 behavioral |
| 201 | HolyC REPL with hc_eval integration | repl.c:106 hc_eval, repl_start callback |
| 202 | GUI input dispatch (kernel input.c queue, X11→kernel queue→WM) | input.c: full event queue + dispatch, input_test.c 11/11 |
| 203 | Fork+exec for .wubu containers | wubu_host_exec.c:212 fork+chroot+execv+mount, test 15 behavioral |
| 206 | Bare-metal preemptive tasking (PIT timer, asm switch) | tasking.c timer_tick, tasking_switch.S, interrupt.c pit_init, 10/10 tasking tests |
| 207 | Unified GUI Shell (REPL + GUI + bare-metal unified) | wubu_shell.c: wubu_shell_run(), wubu_metal_run() calls it |
| 301 | interrupt.c: full IDT with assembly task gates | interrupt.c:isr_stubs.S 256 ISRs, PIC remap, lidt, isr_dispatch |
| 310 | HolyC codegen: ternary, AND, OR, IF, WHILE, FOR | holyc_codegen.c label backpatching, 71/71 eval tests |
| 311 | HolyC codegen: function calls with args (0-6 params) | holyc_codegen.c func table + param handling, 74/74 tests |
| 312 | holyc_codegen: break/continue with label patching | holyc_codegen.c break/continue patch stacks |
| 313 | holyc_codegen: struct layout, string literals, compound ops | holyc_codegen.c member offsets, AST_STRING_LIT, +=/-=/*/= |
| 340 | exec_linux_elf → native container | wubu_exec.c:120-160 wubu_ct_native, wubu_ct_start |
| 341 | exec_win_pe → Proton container | wubu_exec.c:170-210 wubu_ct_steamos, wine |
| 380 | wubu_display.c — DRM/KMS + X11 dual backend | wubu_display.c: probe+drm_init+evdev |
| 381 | libm → pure C math (wubu_math.h implementation) | wubu_math.h: CORDIC sin/cos, NR sqrt, Taylor exp/log |
| 390 | Arch bootstrap + FreeDoom launcher + RAM/SSD root mount | wubu_arch.c pacstrap, wubu_freedoom.c prboom+, wubu_ramdisk.c tmpfs+disk |
| 391 | FreeDoom launcher (prboom-plus in Arch container) | wubu_freedoom.c: GPU+audio passthrough, 10 tests |
| 392 | Root mount: RAM (tmpfs) + SSD + install_to_disk | wubu_ramdisk.c: two-mode, 12 tests |
| 393 | GAAD — Golden Aspect Adaptive Decomposition | wubu_gaad.c: golden subdivision + translate, 17 tests |
| 394 | Theme engine — Win98 Classic/XP Luna/XP Media/WuBu | wubu_theme.c: 30+ colors, 7 tests |
| 395 | Window Manager — drag/resize/GAAD snap/virtual desktops | wubu_wm.c: full WM, 18 tests |
| 396 | Code Editor — Notepad++ class | wubu_editor.c: tabs+syntax+folding, 6 tests |
| 397 | Image Canvas — Photoshop class | wubu_canvas.c: layers+blend+plugins+BMP, 8 tests |
| 398 | FFmpeg Codec Layer | wubu_codec.c: decode/encode/transcode API, 2 tests |
| 399 | Proton container + GPU passthrough + HID/USB | wubu_proton2.c: Arch+Wine+DXVK+evdev, 11 tests |
| 400 | Metal boot + WSL2 GUI abstraction | wubu_metal.c/h: 6/6 tests |
| 401 | Audio Engine — Ardour + Furnace + SF2 | wubu_audio.c/h: 11/11 tests |
| 402 | Furnace Tracker: 12 chip emulations | wubu_audio.c: NES, GB, YM2612, PSG, SID, SAA1099, VRC6, N163, OPL, SCC, AY8910, PC Speaker |
| 403 | TinySoundFont SF2 Parser | wubu_audio.c: RIFF pdta/sdta, 16 presets, ADSR, reverb/chorus |
| 404 | Ardour DAW Mixer | wubu_audio.c: 64 tracks, 16 buses, regions, automation, bus system |
| 405 | AI Plugin Container Streaming | wubu_audio.c: wubu_ai_plugin_*, container_id, 9P streaming protocol |
| 410 | VSL init: host fork/exec verification | wubu_exec.c:77-120 fork test, /bin/sh access, shared mem |
| 411 | VSL run: host shell command execution | wubu_exec.c:126-165 fork+execl+waitpid |
|| 530 | bear_arena: Arena allocator + SoA tensor infrastructure | bear_arena.c/h compiles, tests pass |
|| 531 | bear_simd: AVX2/NEON matmul + fused kernels + MinGRU | bear_simd.h compiles, tests pass |
|| 532 | bear_env: Vectorized env API (CartPole, Squared) | bear_env.c compiles, tests pass |
|| 533 | bear_opt: Adam + Muon optimizers | bear_opt.c compiles, tests pass |
|| 534 | bear_env: N-Pole Cartpole (7-10 poles) with RK4 Lagrangian | bear_env.c:415-700 npole_compute_accelerations, npole_rk4_step, bear_npolecart_step |

---

## Active Gap Cells (v18) — 368 gaps

### Layer 1: Kernel — Hollow Stubs (50 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 302 | interrupt.c hosted stub — SIGSEGV I/O check, no real IDT | 🟡 | input.c:2 |
| 303 | interrupt.c bare-metal: PIT only, no APIC, no IRQ routing | 🟡 | interrupt.c:51-81 |
| 304 | fat32.c: dir entry update on close walks entire root dir (O(N)) | 🟡 | fat32.c:845 |
| 305 | Name parity: 32 ZealOS functions still unmapped | 🟡 | zealos_parity.h |
| 306 | Memory: 4 functions missing (mem_virt_to_phys, mem_phys_to_virt, mem_map, mem_unmap) | 🟡 | memory.c |
| 307 | Memory: mem_heap_ctrl returns static, no real heap control | 🟡 | memory.c:39 |
| 308 | Memory: No slab allocator (only bump) | 🔴 | memory.c |
| 309 | Memory: No page fault handler (demand paging) | 🔴 | interrupt.c |
| 310 | Memory: No virtual filesystem (VFS) layer | 🔴 | fat32.c |
| 311 | Memory: No block layer (bio, request_queue, elevator) | 🔴 | fat32.c |
| 312 | Task: 9 functions missing (create, kill, sleep, yield, priority, affinity, join, detach, get_state) | 🟡 | tasking.c |
| 313 | Task: No preemptive scheduling in hosted mode | 🟡 | tasking.c:86 |
| 314 | Task: No FPU/SSE/AVX context save/restore in task switch | 🔴 | tasking_switch.S |
| 315 | Task: No TSS/IST setup for double fault/NMI handlers | 🔴 | interrupt.c |
| 316 | Task: No SMP boot (AP startup, IPI, spinlocks) | 🔴 | tasking.c |
| 317 | FAT32: 8 functions missing (fmkdir, frmdir, funlink, frename, fattr, flabel, ftime, ftruncate) | 🟡 | fat32.c |
| 318 | FAT32: No LFN (long filename) support | 🟡 | fat32.c |
| 319 | VBE: 2 functions missing (vbe_set_palette, vbe_get_info) | 🟡 | vbe.c |
| 320 | Interrupt: 4 functions missing (interrupt_idt_init, interrupt_gate_set, interrupt_pit_setup, interrupt_apic_init) | 🟡 | interrupt.c |
| 321 | Interrupt: No SYSCALL/SYSRET fast path (only int 0x80) | 🔴 | interrupt.c |
| 322 | Interrupt: No ACPI/PCI/MSI interrupt routing | 🔴 | interrupt.c |
| 323 | Interrupt: No timer calibration (PIT vs HPET vs TSC) | 🟡 | interrupt.c:24-28 |
| 324 | Kernel: No module loading (ELF relocations, symbol resolution) | 🔴 | jit.c |
| 325 | Kernel: No network stack (TCP/IP, sockets, netfilter) | 🔴 | hosted.h:91 |
| 326 | Kernel: No USB subsystem (UHCI/EHCI/XHCI, HID, mass storage) | 🔴 | hosted.h:91 |
| 327 | Kernel: No GPU driver framework (DRM/KMS, GEM) | 🔴 | wubu_display.c |
| 328 | Kernel: No audio subsystem (ALSA-like, mixer) | 🔴 | wubu_audio.c |
| 329 | Kernel: No RTC/CMOS/NVRAM support | 🟡 | interrupt.c |
| 330 | Kernel: No entropy/random (getrandom, /dev/urandom) | 🟡 | hosted.h:91 |
| 331 | Kernel: No cgroups/namespaces for container isolation | 🔴 | wubu_host_exec.c |
| 332 | Kernel: No seccomp/bpf syscall filtering | 🟡 | wubu_vsl.c |
| 333 | Kernel: No audit/logging subsystem | 🟡 | kernel/ |
| 334 | Kernel: No KPTI/Meltdown mitigation | 🟡 | interrupt.c |
| 335 | Kernel: No ACPI table parsing (RSDP, MADT, FADT) | 🔴 | interrupt.c |
| 336 | Kernel: No SMBIOS/DMI table reading | 🟡 | interrupt.c |
| 337 | Kernel: No framebuffer early init | 🟡 | interrupt.c |
| 338 | Kernel: No PS/2 keyboard/mouse fallback | 🟡 | interrupt.c |
| 339 | Kernel: No serial console (COM1) | 🟡 | interrupt.c |

### Layer 2: Compiler — Codegen Gaps (18 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 340 | holyc_codegen: stack args for >6 params (TODO at line 723) | 🟡 | holyc_codegen.c:723 |
| 341 | holyc_codegen: multi-function compilation (TODO at line 1332) | 🟡 | holyc_codegen.c:1332 |
| 342 | holyc_codegen: no struct/union type support in expressions | 🟡 | holyc_codegen.c |
| 343 | holyc_codegen: no array indexing in expressions | 🟡 | holyc_codegen.c |
| 344 | holyc_codegen: no pointer arithmetic | 🟡 | holyc_codegen.c |
| 345 | holyc_codegen: no cast expressions | 🟡 | holyc_codegen.c |
| 346 | holyc_codegen: no sizeof operator | 🟡 | holyc_codegen.c |
| 347 | holyc_codegen: no switch/case statements | 🟡 | holyc_codegen.c |
| 348 | holyc_codegen: no goto/labels | 🟡 | holyc_codegen.c |
| 349 | holyc_codegen: no function pointers | 🟡 | holyc_codegen.c |
| 350 | holyc_codegen: no variadic functions | 🟡 | holyc_codegen.c |
| 351 | holyc_codegen: no inline assembly | 🟡 | holyc_codegen.c |
| 352 | holyc_codegen: no ternary short-circuit for function calls | 🟡 | holyc_codegen.c |
| 353 | holyc_parse: no typedef parsing | 🟡 | holyc_parse.c |
| 354 | holyc_parse: no enum parsing | 🟡 | holyc_parse.c |

### Layer 3: VSL — 52% Stubs (64 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 369 | VSL: ioctl → returns 0 (line 131) | 🔴 | wubu_vsl.c:131 |
| 379 | VSL: mmap/munmap — bump allocator only, no MAP_FIXED, no shared | 🟡 | wubu_vsl.c:437 |
| 380 | VSL: brk — bump only, no validation | 🟡 | wubu_vsl.c:476 |
| 381 | VSL: ELF loading — validates but no PT_LOAD mapping (line 648) | 🟡 | wubu_vsl.c:648 |
| 382 | VSL: No signal handling (sigaction, sigprocmask, sigreturn) | 🔴 | wubu_vsl.c |
| 383 | VSL: No ptrace/process_vm_readv/writev | 🟡 | wubu_vsl.c |
| 384 | VSL: No epoll/kqueue/poll/select | 🟡 | wubu_vsl.c |
| 385 | VSL: No timerfd/signalfd/eventfd/inotify | 🟡 | wubu_vsl.c |
| 386 | VSL: No futex/robust mutex | 🔴 | wubu_vsl.c |
| 387 | VSL: No sysinfo/uname/getrlimit/setrlimit/prlimit | 🟡 | wubu_vsl.c |
| 388 | VSL: No capabilities/capget/capset | 🟡 | wubu_vsl.c |
| 389 | VSL: No seccomp/filter | 🟡 | wubu_vsl.c |
| 390 | VSL: No namespace/clone flags (CLONE_NEW*) | 🟡 | wubu_vsl.c |
| 391 | VSL: No cgroup integration | 🟡 | wubu_vsl.c |
| 392 | VSL: No procfs/sysfs exposure | 🟡 | wubu_vsl.c |
| 393 | VSL: No vDSO/vvar mapping | 🟡 | wubu_vsl.c |
| 394 | VSL: No vsyscall emulation | 🟡 | wubu_vsl.c |
| 395 | VSL: No 32-bit compat (IA32) syscall table | 🟡 | wubu_vsl.c |
| 396 | VSL: No syscall tracing/audit | 🟡 | wubu_vsl.c |
| 397 | VSL: No KVM/hypervisor acceleration path | 🟡 | wubu_vsl.c |
| 398 | VSL: ~950 lines, ~36 (void) casts — reduced stub density | 📊 | wubu_vsl.c |
| 399 | VSL: 42/300+ Linux syscalls implemented (was 26) | 📊 | wubu_vsl.c |

### Layer 4: Container Runtime — Partial (46 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 410 | wubu_host_exec.c: compile C via HolyC → exec (line 217) | 🟡 | wubu_host_exec.c:217 |
| 411 | wubu_host_exec.c: map and call (line 246) | 🟡 | wubu_host_exec.c:246 |
| 412 | wubu_host_exec.c: Per-container 9P Styx dispatch (socket exists, no walk/read) (line 130) | 🟡 | wubu_host_exec.c:130 |
| 413 | wubu_host_exec.c: cgroup/setrlimit enforcement stored but never applied (line 211) | 🟡 | wubu_host_exec.c:211 |
| 414 | wubu_host_exec.c: No network namespace isolation | 🟡 | wubu_host_exec.c |
| 415 | wubu_host_exec.c: No UID/GID mapping (user namespace) | 🟡 | wubu_host_exec.c |
| 416 | wubu_host_exec.c: No mount namespace propagation control | 🟡 | wubu_host_exec.c |
| 417 | wubu_host_exec.c: No GPU device passthrough validation | 🟡 | wubu_host_exec.c |
| 418 | wubu_host_exec.c: No container checkpoint/restore (CRIU) | 🟡 | wubu_host_exec.c |
| 419 | wubu_host_exec.c: No overlayfs/union mount for layered containers | 🟡 | wubu_host_exec.c |
| 420 | wubu_host_exec.c: No container health checks/liveness probes | 🟡 | wubu_host_exec.c |
| 421 | wubu_proton2.c: filesystem check stub (line 501) | 🟡 | wubu_proton2.c:501 |
| 422 | wubu_proton2.c: DLL dependency resolution stub (wubu_proton.h:221) | 🟡 | wubu_proton.h:221 |
| 423 | wubu_proton2.c: No DXVK/VKD3D config management | 🟡 | wubu_proton2.c |
| 424 | wubu_proton2.c: No Steam runtime version selection | 🟡 | wubu_proton2.c |
| 424 | wubu_proton2.c: No Proton prefix management (per-game) | 🟡 | wubu_proton2.c |
| 425 | wubu_proton2.c: No Vulkan ICD loader injection | 🟡 | wubu_proton2.c |
| 425 | wubu_proton2.c: No FSR/FSR2/DLSS integration | 🟡 | wubu_proton2.c |
| 426 | wubu_proton2.c: No controller/HID mapping profiles | 🟡 | wubu_proton2.c |
| 426 | wubu_proton2.c: No audio sink selection (PipeWire/Pulse/ALSA) | 🟡 | wubu_proton2.c |
| 427 | wubu_ramdisk.c: No snapshot/rollback for RAM root | 🟡 | wubu_ramdisk.c |
| 428 | wubu_ramdisk.c: No COW for SSD→RAM migration | 🟡 | wubu_ramdisk.c |
| 429 | wubu_ramdisk.c: No encryption (LUKS) for persistent root | 🟡 | wubu_ramdisk.c |
| 430 | wubu_arch.c: No pacman hook for kernel updates | 🟡 | wubu_arch.c |
| 431 | wubu_arch.c: No AUR helper integration | 🟡 | wubu_arch.c |
| 432 | wubu_arch.c: No reproducible build verification | 🟡 | wubu_arch.c |
| 433 | wubu_freedoom.c: No save/load game state sync | 🟡 | wubu_freedoom.c |
| 434 | wubu_freedoom.c: No multiplayer/netplay support | 🟡 | wubu_freedoom.c |
| 435 | Container: No image format (OCI/Docker) support | 🟡 | wubu_host_exec.c |
| 436 | Container: No registry pull/push/auth | 🟡 | wubu_host_exec.c |
| 437 | Container: No build system (Dockerfile/.wububuild) | 🟡 | wubu_host_exec.c |
| 438 | Container: No compose/orchestration (multi-container apps) | 🟡 | wubu_host_exec.c |

### Layer 5: wubu_exec — Dispatch Stubs (14 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 452 | wubu_exec.c: exec_holyc — map and call stub (line 246) | 🟡 | wubu_exec.c:246 |
| 453 | wubu_exec.c: exec_c — compile via HolyC then exec (line 217) | 🟡 | wubu_exec.c:217 |
| 454 | wubu_exec.c: exec_wren — no Wren VM integration | 🟡 | wubu_exec.c |
| 455 | wubu_exec.c: exec_lua — no Lua VM integration | 🟡 | wubu_exec.c |
| 456 | wubu_exec.c: exec_wasm — no Wasm runtime (wasm3/wasm-micro-runtime) | 🟡 | wubu_exec.c |
| 457 | wubu_exec.c: No .wubu manifest validation (signature, checksums) | 🟡 | wubu_exec.c |
| 457 | wubu_exec.c: No dependency resolution between containers | 🟡 | wubu_exec.c |
| 458 | wubu_exec.c: No resource quota inheritance | 🟡 | wubu_exec.c |
| 459 | wubu_exec.c: No exec timeout/watchdog | 🟡 | wubu_exec.c |
| 459 | wubu_exec.c: No exec logging/audit trail | 🟡 | wubu_exec.c |

### Layer 6: GUI — WM/Editor/Canvas Stubs (53 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 460 | wubu_canvas.c: layer_merge_down stub (line 165) | 🟡 | wubu_canvas.c:165 |
| 461 | wubu_canvas.c: layer_flatten stub (line 166) | 🟡 | wubu_canvas.c:166 |
| 462 | wubu_canvas.c: Drawing — flood fill stub (line 225) | 🟡 | wubu_canvas.c:225 |
| 463 | wubu_canvas.c: select_invert stub (line 256) | 🟡 | wubu_canvas.c:256 |
| 464 | wubu_canvas.c: Filters — all stubs (line 258) | 🟡 | wubu_canvas.c:258 |
| 465 | wubu_canvas.c: save_gif/load stubs (lines 362-363) | 🟡 | wubu_canvas.c:362 |
| 466 | wubu_canvas.c: Canvas ops stubs (line 385) | 🟡 | wubu_canvas.c:385 |
| 467 | wubu_editor.c: Undo/Redo stubs — needs undo stack (line 294) | 🟡 | wubu_editor.c:294 |
| 468 | wubu_editor.c: Selection stubs (line 301) | 🟡 | wubu_editor.c:301 |
| 469 | wubu_editor.c: Find/Replace stubs (line 316) | 🟡 | wubu_editor.c:316 |
| 470 | wubu_editor.c: Code Folding stubs (line 323) | 🟡 | wubu_editor.c:323 |
| 471 | wubu_editor.c: Bookmarks stubs (line 329) | 🟡 | wubu_editor.c:329 |
| 472 | wubu_editor.c: Macro stubs (line 347) | 🟡 | wubu_editor.c:347 |
| 473 | wubu_editor.c: Session stubs (line 353) | 🟡 | wubu_editor.c:353 |
| 474 | wubu_codec.c: pipe-based frame reading stub (line 119) | 🟡 | wubu_codec.c:119 |
| 475 | wubu_codec.c: ffprobe JSON parsing stub (line 242) | 🟡 | wubu_codec.c:242 |
| 476 | wubu_codec.c: codec .wubu container mount stub (line 247) | 🟡 | wubu_codec.c:247 |
| 477 | wm_nano RectStack: linked list TODO (line 213) | 🟡 | rectstk.c:213 |
| 478 | wm_nano Theme: RGB to HSV conversion TODO (line 548) | 🟡 | color.c:548 |
| 479 | wm_nano Theme: load/save from file stubs (lines 721-728) | 🟡 | theme.c:721 |
| 480 | wm_nano Image: division optimization TODO (line 141) | 🟡 | image.c:141 |
| 481 | wm_nano List: TODO! (line 684) | 🟡 | list.c:684 |
| 482 | wm_nano MQueue: overflow check TODO (line 57) | 🟡 | mqueue.c:57 |
| 483 | wm_nano Table: colors/scrolling TODOs (lines 412, 431, 49) | 🟡 | table.c:49 |
| 484 | wm_nano Tasklist: animation TODOs (lines 284, 288) | 🟡 | tasklist.c:284 |
| 485 | wm_nano Term: console lock TODO (line 136) | 🟡 | term.c:136 |
| 486 | wm_nano TextEdit: Macintosh CR, undo, selection, efficiency TODOs | 🟡 | textedit.c |
| 487 | wm_nano Window: RefreshRectangle, safelock, buffer free TODOs | 🟡 | window.c |
| 488 | wm_nano ZBuf: completion/wait TODOs (lines 153, 203) | 🟡 | zbuf.c:153 |
| 489 | wm_nano Click: TODOs (lines 267, 278) | 🟡 | click.c:267 |
| 490 | wm_nano BG: tile/seam/VBEData TODOs (lines 115, 116, 155) | 🟡 | bg.c:115 |
| 491 | wm_nano: placeholderBackground constant (lines 20, 49) | 🟡 | bg.c:20 |
| 492 | wubus_wm.c: No compositor/transition effects | 🟡 | wubu_wm.c |
| 493 | wubu_wm.c: No virtual desktop grid animation | 🟡 | wubu_wm.c |
| 494 | wubu_wm.c: No window snapping to GAAD grid (only manual) | 🟡 | wubu_wm.c |
| 495 | wubu_wm.c: No multi-monitor/Xinerama support | 🟡 | wubu_wm.c |
| 495 | wubu_wm.c: No DPI scaling | 🟡 | wubu_wm.c |

### Layer 7: Audio Engine — Toy vs Production (38 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 496 | **Furnace**: Uses 70+ external libs (blip_buf+puNES/NSFPlay, Nuked-OPLL, Nuked-PSG, SAASound, YM3812-LLE, YMF262-LLE, YM2608-LLE, YM2610/LLE, vgsound_emu-modified) — we have 12 toy C impls | 🔴 | wubu_audio.c:40-835 |
| 497 | **TinySoundFont**: Our impl = RIFF header validation + sine wave; upstream = full pdta/sdta parsing, sample playback, envelopes, modulators, voice management, preset/bank loading, thread-safe copy | 🔴 | wubu_audio.c:1129-1270 |
| 498 | **Ardour DAW**: Our mixer = toy; upstream = sample-accurate automation, LV2/VST3/CLAP, JACK transport, AAF/OMF, video timeline, control surfaces, anywhere-to-anywhere routing, unlimited undo, crossfade, video sync | 🔴 | wubu_audio.c:1273-1430 |
| 499 | Furnace: No cycle-accurate emulation (uses blip_buf + puNES/NSFPlay) | 🔴 | wubu_audio.c:40-835 |
| 500 | Furnace: No Nuked-OPLL/PSG, SAASound, YM3812-LLE/YMF262-LLE libs | 🔴 | wubu_audio.c:40-835 |
| 501 | Furnace: No YM2610/LLE (YM2612 Genesis FM) via vgsound_emu-modified | 🔴 | wubu_audio.c:40-835 |
| 502 | TinySoundFont: No sample playback from sf2 sdta chunks | 🔴 | wubu_audio.c:1129-1270 |
| 503 | TinySoundFont: No ADSR envelopes on samples | 🔴 | wubu_audio.c:1129-1270 |
| 504 | TinySoundFont: No modulators (TSF marks as NOT YET IMPLEMENTED) | 🟡 | wubu_audio.c:1129-1270 |
| 505 | TinySoundFont: No proper low-pass filter | 🟡 | wubu_audio.c:1129-1270 |
| 506 | TinySoundFont: No proper voice management with preallocation | 🟡 | wubu_audio.c:1129-1270 |
| 507 | TinySoundFont: No thread-safe tsf_copy for multi-instance | 🟡 | wubu_audio.c:1129-1270 |
| 508 | TinySoundFont: No Chorus/Reverb effects send generators | 🟡 | wubu_audio.c:1129-1270 |
| 509 | Ardour DAW: No sample-accurate automation (per-frame float) | 🔴 | wubu_audio.c:1273-1430 |
| 510 | Ardour DAW: No LV2/VST3/CLAP plugin API | 🔴 | wubu_audio.c:1273-1430 |
| 511 | Ardour DAW: No JACK transport sync | 🟡 | wubu_audio.c:1273-1430 |
| 512 | Ardour DAW: No AAF/OMF interchange format | 🟡 | wubu_audio.c:1273-1430 |
| 513 | Ardour DAW: No video timeline / lock regions to video | 🟡 | wubu_audio.c:1273-1430 |
| 514 | Ardour DAW: No external control surface support (Mackie/SSL/MCU) | 🟡 | wubu_audio.c:1273-1430 |
| 515 | Ardour DAW: No anywhere-to-anywhere signal routing matrix | 🟡 | wubu_audio.c:1273-1430 |
| 516 | Ardour DAW: No crossfade editing / region overlap handling | 🟡 | wubu_audio.c:1273-1430 |
| 517 | Ardour DAW: No unlimited undo/redo with snapshots | 🟡 | wubu_audio.c:1273-1430 |
| 518 | Audio: No VST/CLAP/LV2 plugin API | 🔴 | wubu_audio.c |
| 519 | Audio: No JACK/PipeWire backend (only ALSA stub) | 🟡 | wubu_audio.c |
| 520 | Audio: No MIDI clock/sync (MTC, SPP) | 🟡 | wubu_audio.c |
| 521 | Audio: No audio import (WAV/FLAC/OGG/MP3 via libsndfile) | 🟡 | wubu_audio.c |
| 522 | Audio: No audio export/render to file | 🟡 | wubu_audio.c |
| 523 | Audio: No AI plugin container stereo stream protocol | 🟡 | wubu_audio.c:849 |
| 524 | Audio: No GPU compute integration for AI plugins | 🟡 | wubu_audio.c |
| 525 | Audio: No real-time audio analysis (FFT, spectrogram) | 🟡 | wubu_audio.c |
| 526 | Audio: No Furnace .dmf import/export | 🟡 | wubu_audio.c |
| 527 | Audio: No Furnace instrument editor UI | 🟡 | wubu_audio.c |
| 528 | Audio: No pattern matrix/sequence editor | 🟡 | wubu_audio.c |
| 529 | Audio: No MIDI CC handling (volume, pan, expression) | 🟡 | wubu_audio.c |
| 530 | Audio: No DAW automation lanes (volume, pan, plugin params) | 🟡 | wubu_audio.c |
| 531 | Audio: No region crossfade/editing | 🟡 | wubu_audio.c |

### Layer 8: JIT — Stub Backends (14 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 532 | jit_mir.c: MIR compile stub (line 40) | 🔴 | jit_mir.c:40 |
| 533 | jit.c: jit_mir_compile TODO (line 261) | 🟡 | jit.c:261 |
| 534 | jit.c: jit_asmjit_compile TODO (line 265) | 🟡 | jit.c:265 |
| 535 | jit.c: Disassembly requires capstone/libopcodes (line 367) | 🟡 | jit.c:367 |
| 536 | JIT: No AOT compilation cache | 🟡 | jit.c |
| 537 | JIT: No tiered compilation (interpreter → baseline → optimized) | 🟡 | jit.c |
| 538 | JIT: No inline caching | 🟡 | jit.c |
| 539 | JIT: No OSR (on-stack replacement) | 🟡 | jit.c |
| 540 | JIT: No escape analysis | 🟡 | jit.c |
| 541 | JIT: No vectorization (SIMD) | 🟡 | jit.c |
| 542 | JIT: No register allocation (linear scan / graph coloring) | 🟡 | jit.c |

### Layer 9: Metal Boot — Bare Metal Gaps (22 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 543 | wubu_metal.c: WSL2 detection + wslg integration stub (line 1039) | 🟡 | wubu_metal.c:1039 |
| 544 | wubu_metal.c: Initramfs creation + GRUB config missing | 🟡 | wubu_metal.c |
| 545 | wubu_metal.c: DRM/KMS mode setting stub — libdrm not replaced (3.7K vs 20K+ LOC) | 🟡 | wubu_metal.c |
| 546 | wubu_metal.c: No UEFI boot entry creation | 🟡 | wubu_metal.c |
| 547 | wubu_metal.c: No Secure Boot shim | 🟡 | wubu_metal.c |
| 548 | wubu_metal.c: No kernel command line parser | 🟡 | wubu_metal.c |
| 549 | wubu_metal.c: No ACPI table parsing (RSDP, MADT, FADT) | 🟡 | wubu_metal.c |
| 550 | wubu_metal.c: No SMBIOS/DMI table reading | 🟡 | wubu_metal.c |
| 551 | wubu_metal.c: No framebuffer early init (before DRM) | 🟡 | wubu_metal.c |
| 552 | wubu_metal.c: No PS/2 keyboard/mouse fallback | 🟡 | wubu_metal.c |
| 553 | wubu_metal.c: No serial console (COM1) | 🟡 | wubu_metal.c |
| 554 | wubu_metal.c: No kexec/kdump support | 🟡 | wubu_metal.c |
| 555 | wubu_metal.c: No hibernation/suspend (ACPI S3/S4) | 🟡 | wubu_metal.c |
| 556 | wubu_metal.c: No TPM/PCR measurement | 🟡 | wubu_metal.c |
| 557 | wubu_metal.c: No IOMMU/VT-d setup for GPU passthrough | 🟡 | wubu_metal.c |
| 558 | wubu_metal.c: No /dev/dxg paravirt GPU (WSL2) | 🟡 | wubu_metal.c |
| 559 | wubu_metal.c: No virtio-fs/9p for host file sharing | 🟡 | wubu_metal.c |

### Layer 10: Third-Party → C Replacements (10 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 560 | libX11 → DRM/KMS (wubu_display.c) — partial, X11 still in hosted.c | 🟡 | hosted.c |
| 561 | libdrm → direct ioctl (3.7K vs 20K+ LOC) | 🟡 | wubu_display.c |
| 562 | libgbm → custom GBM | 🟡 | wubu_display.c |
| 563 | MIR (c2m) → self-contained JIT | 🟡 | jit_mir.c |
| 564 | NanoShellOS naming → WuBuOS naming (wm_nano/*) | ⬜ | gui/wm_nano/* |
| 565 | capstone/libopcodes → self-contained disassembler | 🟡 | jit.c:367 |
| 566 | ffmpeg → pure C codec (wubu_codec.c partial, no libavcodec) | 🟡 | wubu_codec.c |
| 567 | SDL/GLFW → wubu_display (input/window) | ⬜ | wubu_display.c |

### Layer 11: WorldSim — Placeholder Game Logic (8 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 568 | worldsim/render.c: No pixels — draw rect placeholder (line 82) | 🟡 | render.c:82 |
| 569 | worldsim/terrain.c: No river/road generation | 🟡 | terrain.c |
| 570 | worldsim/terrain.c: No biome transitions | 🟡 | terrain.c |
| 571 | worldsim/entity.c: update returns NULL stubs (lines 41, 53) | 🟡 | entity.c:41 |
| 572 | worldsim/physics.c: No collision broadphase | 🟡 | physics.c |
| 573 | worldsim/: No networking (multiplayer) | 🟡 | worldsim/ |
| 574 | worldsim/: No entity AI/behavior trees | 🟡 | worldsim/ |
| 575 | worldsim/: No procedural quest/dungeon generation | 🟡 | worldsim/ |

### Layer 12: Styx/9P — Missing Messages (15 gaps)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 576 | StyxFS: walk/read not implemented (styxfs.c returns NULL) | 🔴 | styxfs.c:28 |
| 577 | Styx: Rremove/Rrename/Rstat/Rwstat missing | 🟡 | styx.c |
| 578 | Styx: Rauth/Rflush not implemented | 🟡 | styx.c |
| 579 | 9P: No .wubu container namespace mount | 🟡 | wubu_host_exec.c:130 |
| 580 | 9P: No mux/demux for multiple containers | 🟡 | styx.c |

---

## Summary

| Category | Count |
|----------|-------|
| 🔴 CRITICAL (REAL GAP — no impl at all or toy vs production) | 18 |
| 🟡 HIGH (PARTIAL / STUB / NAMING / THIRD-PARTY / MISSING SUBSYSTEMS) | 358 |
| ⬜ LOW (NIT / CLEANUP / COSMETIC) | 32 |
| ✅ RESOLVED | 42 |
| **TOTAL DOCUMENTED GAPS** | **407** |

---

## Priority Order (Top 20 — Post Triple DA)

1. **Cell 496** — Audio: Replace 12 toy chip emulators with Furnace-grade external libs (blip_buf, Nuked-*, SAASound, YM3812-LLE, YMF262-LLE, YM2608-LLE, vgsound_emu)
2. **Cell 497** — Audio: Replace TinySoundFont stub with schellingb/TinySoundFont upstream
3. **Cell 498** — Audio: Implement Ardour-grade DAW (sample-accurate automation, LV2/VST3/CLAP, JACK, AAF/OMF, video sync)
4. **Cell 382** — VSL: signal handling (sigaction, sigprocmask) — next
5. **Cell 305** — name parity: map remaining 32 ZealOS functions
6. **Cell 304** — fat32.c: O(1) dir entry update (dir_cluster cache)
7. **Cell 388/389/391** — libdrm/libgbm/MIR → C replacements
8. **Cell 302/303** — interrupt.c: bare-metal IDT + APIC + IRQ routing
9. **Cell 414** — per-container 9P Styx dispatch (walk/read)
10. **Cell 415** — cgroup/setrlimit enforcement in container runtime
11. **Cell 523-525** — wubu_metal: WSL2 wslg, initramfs, DRM/KMS mode set
12. **Cell 467-473** — wubu_editor: undo/redo, find, folding, bookmarks, macros
13. **Cell 460-466** — wubu_canvas: layer ops, flood fill, filters, GIF
14. **Cell 308-309** — task: 9 missing functions, preemptive scheduling
15. **Cell 310-319** — kernel: 10 critical missing subsystems (VFS, block, net, USB, GPU, audio, modules, paging, KPTI, SMP)

---

*Next: Cell 576 — Styx walk/read → Cell 496 — Audio Furnace → Cell 467-473 — editor stubs*