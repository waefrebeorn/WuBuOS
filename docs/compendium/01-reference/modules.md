<!-- GENERATED FILE -- do not edit by hand.
     Run `make docs` (tools/gen_docs.py) to regenerate. -->

# Modules
> Generated 2026-08-02 10:08 UTC -- recursive src/ walk, 582 modules.

| Tree | Module | Lines | Depends on | Purpose |
|------|--------|------:|------------|---------|
| `apps/` | `app_canvas` | 379 | dosgui_apps.h, dosgui_wm.h, stdio.h, stdlib.h, string.h, vbe.h, wubu_canvas.h, w | app_canvas.c  --  WuBu Canvas: in-shell image editor binding Binds the real layered wubu_canvas engi |
| `apps/` | `app_explorer` | 84 | dosgui_apps.h, dosgui_explorer.h, dosgui_wm.h, stdlib.h, string.h, vbe.h, wubu_t | app_explorer.c  --  WuBuOS File Manager: in-shell Explorer binding Binds the real, Win98/XP-class do |
| `apps/` | `bonzi` | 233 | bonzi.h, ctype.h, dosgui_apps.h, dosgui_wm.h, dosgui_wm_internal.h, stdio.h, std | bonzi.c -- Bonzi Buddy: desktop AGI agent persona (WuBuOS human interface). Real windowed agent: lau |
| `apps/` | `calc` | 390 | calc.h, calc_internal.h, dosgui_wm.h, dosgui_wm_internal.h, math.h, stdio.h, std | calc.c  --  Calculator App (Standard / Scientific / Programmer / Graphing) Real windowed engine: lau |
| `apps/` | `calc_math` | 46 | calc.h, math.h | calc_math.c -- Calculator math evaluation (self-contained). calc_apply_op / calc_apply_func: pure do |
| `apps/` | `calc_test` | 140 | calc.h, math.h, stdio.h, stdlib.h, string.h | calc_test.c  --  WuBuOS Calculator behavioral tests Exercises the real calculation engine (no GUI re |
| `apps/` | `calc_test_support` | 53 | calc.h, dosgui_wm.h, stdlib.h, wubu_theme.h | calc_test_support.c -- headless link support for the calculator unit test. The calculator's draw pat |
| `apps/` | `canvas_standalone` | 105 | dosgui_wm.h, input.h, select.h, stdbool.h, stdio.h, stdlib.h, string.h, unistd.h | canvas_standalone.c -- standalone Linux entry point for the WuBuOS image editor (the real, layered w |
| `apps/` | `cmd` | 334 | cmd.h, dosgui_wm.h, fcntl.h, ioctl.h, pty.h, signal.h, stdio.h, stdlib.h, string | for kill(), ioctl(), pty.h |
| `apps/` | `cmd_test` | 80 | cmd.h, errno.h, stdio.h, stdlib.h, string.h, unistd.h | cmd_test.c -- Real test for the CMD terminal engine (apps/cmd/cmd.c). Proves the engine does real wo |
| `apps/` | `cmd_test_stub` | 25 | stdint.h, string.h | cmd_test_stub.c -- Minimal stubs for the CMD terminal test. The test only exercises pty spawn/read/h |
| `apps/` | `comfy` | 385 | bonzi.h, comfy.h, dosgui_wm.h, dosgui_wm_internal.h, math.h, stdio.h, stdlib.h,  | comfy.c -- Comfy: node-graph visual scripting for the AGI operator. Real node-graph engine: nodes ha |
| `apps/` | `control` | 185 | control.h, dosgui_window_chrome.h, dosgui_wm.h, dosgui_wm_internal.h, stdlib.h,  | control.c  --  Control Panel (9 tabs) - minimal stub |
| `apps/` | `control_test` | 164 | control.h, dosgui_wm.h, dosgui_wm_internal.h, stdio.h, stdlib.h, string.h, vbe.h | control_test.c  --  WuBuOS Control Panel Test Suite Cell 395: Win98-style settings panel. Verifies t |
| `apps/` | `dosgui_apps` | 274 | bonzi.h, calc.h, cmd.h, comfy.h, control.h, dosgui_apps.h, dosgui_dos_window.h,  | dosgui_apps.c  --  Single App Registry Implementation ONE data-driven table (g_app_defs[]) is the so |
| `apps/` | `dosgui_apps_test` | 600 | calc.h, control.h, dosgui_apps.h, editor.h, fm.h, notepad.h, regedit.h, repl.h,  | dosgui_apps_test.c  --  WuBuOS DosGui Apps Test Suite Tests for built-in Win98-style apps: - Task Ma |
| `apps/` | `dosgui_apps_test_stubs` | 151 | dosgui_apps.h, stdarg.h, stddef.h, stdint.h, wubu_compat_db.h, wubu_notify.h, wu | dosgui_apps_test_stubs.c -- no-op link support for dosgui_apps_test. dosgui_apps.c is the app dispat |
| `apps/` | `editor` | 22 | dosgui_wm.h, editor.h, stdlib.h, vbe.h, wubu_theme.h | editor.c  --  Simple Editor Wrapper - minimal stub |
| `apps/` | `edr_dash` | 238 | dosgui_wm.h, dosgui_wm_internal.h, stdio.h, stdlib.h, string.h, vbe.h, wubu_edr. | edr_dash.c -- EDR Activity Dashboard (the disclosure surface) This is the user-facing half of WuBuOS |
| `apps/` | `fm` | 85 | dosgui_wm.h, fm.h, stdlib.h, string.h, vbe.h, wubu_theme.h | fm.c  --  File Manager (9P/Styx Operations) - minimal stub |
| `apps/` | `notepad` | 212 | dosgui_window_chrome.h, dosgui_wm.h, dosgui_wm_internal.h, notepad.h, stdlib.h,  | notepad.c  --  My Seed Notepad (Win98-style text editor, real engine) Genuine multi-line text editin |
| `apps/` | `notepad` | 88 | dosgui_wm.h, notepad.h, stdlib.h, string.h, vbe.h, wubu_theme.h | notepad.c  --  Notepad++ Style Editor (minimal stub) |
| `apps/` | `regedit` | 136 | dosgui_wm.h, regedit.h, stdlib.h, string.h, vbe.h, wubu_theme.h | regedit.c  --  Windows Registry Editor Clone - minimal stub |
| `apps/` | `repl` | 125 | dosgui_wm.h, gui_dbuf.h, holyc.h, repl.h, stdio.h, stdlib.h, string.h, vbe.h | repl.c  --  My Seed HolyC JIT REPL (runs inside GUI window) Uses the HolyC compiler (hc_eval) for ev |
| `apps/` | `repl` | 47 | dosgui_wm.h, repl.h, stdlib.h, string.h, vbe.h, wubu_theme.h | repl.c  --  HolyC REPL Terminal - minimal stub |
| `apps/` | `taskmgr` | 205 | dosgui_window_chrome.h, dosgui_wm.h, dosgui_wm_internal.h, stdlib.h, taskmgr.h,  | taskmgr.c  --  Task Manager (Windows 11 Style) - minimal stub |
| `apps/` | `wubu_apps2_test` | 353 | stdio.h, stdlib.h, string.h, unistd.h, wubu_canvas.h, wubu_codec.h, wubu_editor. | wubu_apps_test.c  --  Tests for Editor, Canvas, and Codec Cell 396/397/398: Notepad++ editor, Photos |
| `apps/` | `wubu_canvas_blend` | 52 | wubu_canvas.h | wubu_canvas_blend.c -- Canvas blend-compositing subsystem (self-contained). blend_channel + wubu_ble |
| `apps/` | `wubu_canvas_draw` | 226 | stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_canvas_internal.h | wubu_canvas_draw.c -- WuBuOS canvas: pixel drawing tools, color pick, and selection. Self-contained: |
| `apps/` | `wubu_canvas_filter` | 128 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_filter.c -- WuBuOS canvas: per-layer pixel filters (blur, sharpen, edge, invert, thresho |
| `apps/` | `wubu_canvas_io` | 658 | stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_image_codec_internal.h, zlib.h | wubu_canvas_io.c  --  WuBuOS Image Editor: File I/O backend Cell 397 (I/O split 2026-07-09): PNG/BMP |
| `apps/` | `wubu_canvas_io_ppm` | 81 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_io_ppm.c -- Canvas PPM format save/load (self-contained). wubu_cv_save_ppm (P6 binary) + |
| `apps/` | `wubu_canvas_layers` | 191 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_layers.c -- WuBuOS canvas: create/destroy + layer ops + compositing. Self-contained: dep |
| `apps/` | `wubu_canvas_plugin` | 40 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_plugin.c -- WuBuOS canvas: plugin registration / run / unregister. Self-contained: depen |
| `apps/` | `wubu_canvas_transform` | 171 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_transform.c -- WuBuOS canvas: viewport (zoom/pan) and geometric layer transforms (resize |
| `apps/` | `wubu_canvas_undo` | 114 | stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_canvas_internal.h | wubu_canvas_undo.c -- WuBuOS canvas: undo/redo history + snapshot hook. Self-contained: owns the und |
| `apps/` | `wubu_codec` | 497 | errno.h, fcntl.h, mount.h, signal.h, stat.h, stdio.h, stdlib.h, string.h, unistd | wubu_codec.c  --  WuBuOS Codec Layer Implementation Cell 398: FFmpeg CLI wrapper + optional libav li |
| `apps/` | `wubu_editor` | 468 | stdio.h, stdlib.h, string.h, wubu_editor.h | wubu_editor.c  --  WuBuOS Code Editor Implementation Cell 396: Tabbed editor with syntax HL, find/re |
| `apps/` | `wubu_editor_bookmark` | 55 | wubu_editor.h | wubu_editor_bookmark.c -- Editor bookmark subsystem (self-contained). wubu_ed_bookmark_toggle/next/p |
| `apps/` | `wubu_editor_find` | 136 | string.h, wubu_editor.h | wubu_editor_find.c -- WuBuOS editor find/replace (split from wubu_editor.c) Self-contained subsystem |
| `apps/` | `wubu_editor_macro` | 42 | stdlib.h, string.h, wubu_editor.h | wubu_editor_macro.c -- WuBuOS editor: keyboard macro record/playback. Extracted from wubu_editor.c ( |
| `apps/` | `wubu_editor_selection` | 146 | stdlib.h, string.h, wubu_editor.h | wubu_editor_selection.c -- WuBuOS editor selection + clipboard (split from wubu_editor.c) Self-conta |
| `apps/` | `wubu_editor_undo` | 130 | stdlib.h, string.h, wubu_editor.h | wubu_editor_undo.c -- WuBuOS editor undo/redo engine (split from wubu_editor.c) Self-contained subsy |
| `apps/` | `wubu_image_codec` | 110 | stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_image_codec_internal.h | wubu_image_codec.c -- Self-contained image-codec leaf for WuBuOS canvas. Pure, dependency-free codec |
| `audio/` | `wubu_audio` | 69 | wubu_audio.h, wubu_audio_internal.h | wubu_audio.c  --  WuBuOS Audio Engine (Facade) Modular audio engine: chip emulations, furnace tracke |
| `audio/` | `wubu_audio_chips` | 408 | wubu_audio_internal.h | wubu_audio_chips.c  --  WuBuOS Chip Emulations All retro chip emulations (NES, GB, YM2612, SID, etc. |
| `audio/` | `wubu_audio_daw` | 210 | wubu_audio_internal.h | wubu_audio_daw.c  --  WuBuOS DAW Mixer (Ardour-style) Track management, bus routing, master processi |
| `audio/` | `wubu_audio_engine` | 365 | wubu_audio_internal.h | wubu_audio_engine.c  --  WuBuOS Audio Engine Main Engine lifecycle, process callback, global state.  |
| `audio/` | `wubu_audio_furnace` | 347 | wubu_audio_internal.h | wubu_audio_furnace.c  --  WuBuOS Furnace Tracker Chip tracker with pattern editor, Furnace-style. Ex |
| `audio/` | `wubu_audio_sf2` | 164 | wubu_audio_internal.h | wubu_audio_sf2.c  --  WuBuOS TinySoundFont SF2 Synthesis Simplified SF2 parser and sample playback e |
| `audio/` | `wubu_audio_test` | 293 | assert.h, stdio.h, stdlib.h, string.h, wubu_audio.h, wubu_math.h | wubu_audio_test.c  --  Tests for audio engine (DAW + Furnace + SF2 + AI) |
| `bear/` | `bear_arena` | 82 | bear_arena.h, stdio.h | bear_arena.c  --  PufferC/BearRL Arena Allocator Implementation |
| `bear/` | `bear_cudnn` | 864 | bear_cudnn.h, cublas_v2.h, cuda_runtime.h, cudnn.h, math.h, stdint.h, stdlib.h,  | bear_cudnn.c  --  cuBLAS/cuDNN Wrapper Implementations HolyC-callable cuBLAS/cuDNN operations Compil |
| `bear/` | `bear_cudnn_cublas` | 220 | bear_cudnn.h, bear_simd.h, math.h, stdlib.h, string.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `bear/` | `bear_cudnn_cuda` | 75 | bear_cudnn.h, stdlib.h, string.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `bear/` | `bear_env` | 430 | bear_arena.h, bear_env.h, bear_env_internal.h, bear_simd.h, math.h, stdio.h, std | bear_env.c  --  PufferC/BearRL Vectorized Environment Implementation |
| `bear/` | `bear_env_npole` | 562 | bear_arena.h, bear_env.h, bear_env_internal.h, bear_simd.h, math.h, stdio.h, std | bear_env_npole.c -- WuBuOS BearRL N-Pole Cartpole (7-10 poles) Extracted from bear_env.c (monolith s |
| `bear/` | `bear_nn_ckpt` | 226 | bear_arena.h, bear_nn.h, stdio.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `bear/` | `bear_nn_policy` | 382 | bear_arena.h, bear_nn.h, bear_simd.h, math.h, stdio.h, stdlib.h, string.h | bear_nn_policy.c -- WuBuOS BearRL policy-network implementation (MLP/minGRU create, forward, sample, |
| `bear/` | `bear_nn_value` | 598 | bear_arena.h, bear_nn.h, math.h, stdio.h, stdlib.h, string.h | bear_nn_value.c -- WuBuOS BearRL value-network implementation (create, forward, orthogonal init, bac |
| `bear/` | `bear_opt` | 326 | bear_arena.h, bear_opt.h, bear_simd.h, math.h, stdlib.h, string.h | bear_opt.c  --  PufferC/BearRL Optimizer Implementation (Adam + Muon) |
| `bear/` | `bear_opt_test` | 151 | bear_arena.h, bear_opt.h, math.h, stdio.h, stdlib.h, string.h | bear_opt_test.c  --  BearRL Optimizer Closure Test (form≠function) Verifies that bear_optimizer_step |
| `bear/` | `bear_ppo_loss` | 241 | bear_arena.h, bear_nn.h, bear_opt.h, bear_ppo.h, bear_simd.h, math.h, stdio.h, s | bear_ppo_loss.c -- BearRL PPO loss computation, gradient clipping, and gradient application (extract |
| `bear/` | `bear_ppo_trainer` | 401 | bear_arena.h, bear_nn.h, bear_opt.h, bear_ppo.h, bear_simd.h, math.h, stdio.h, s | bear_ppo_trainer.c -- BearRL PPO trainer lifecycle, training iteration, and checkpoint save/load (ex |
| `bear/` | `bear_ppo_traj` | 291 | bear_arena.h, bear_nn.h, bear_opt.h, bear_ppo.h, bear_simd.h, math.h, stdio.h, s | bear_ppo_traj.c -- BearRL PPO trajectory buffers, GAE/V-Trace advantages, and minibatch sampler (ext |
| `bear/` | `bear_train` | 193 | bear_arena.h, bear_env.h, bear_nn.h, bear_opt.h, bear_ppo.h, bear_simd.h, math.h | bear_train.c  --  BearRL Training Entry Point Sovereign C11 RL training for N-Pole Cartpole (7-10 po |
| `bear/` | `bear_vulkan_soft` | 459 | bear_arena.h, bear_env.h, bear_nn.h, bear_ppo.h, bear_vulkan.h, math.h, stdio.h, | bear_vulkan_soft.c  --  BearRL Vulkan API Software Fallback (Pure C11) Implements the bear_vulkan.h  |
| `bridge/` | `bridge` | 61 | bridge.h, string.h | bridge.c  --  My Seed Bridge Implementation |
| `bridge/` | `bridge_test` | 207 | bridge.h, stdio.h, string.h | bridge_test.c  --  WuBuOS Bridge/DOS Flip Test Suite Cell 103: Tests DOS flip bridge wiring (Ctrl+Al |
| `bridge/` | `vbe_ws_bridge` | 417 | stdio.h, string.h, vbe_ws_bridge.h | vbe_ws_bridge.c  --  VBE ↔ WorldSim Render Bridge Implementation Cell 070: Wires WorldSim software r |
| `bridge/` | `vbe_ws_bridge_test` | 727 | stdio.h, stdlib.h, string.h, vbe_ws_bridge.h | vbe_ws_bridge_test.c  --  Test Suite for VBE ↔ WorldSim Render Bridge Cell 070: Tests the wiring bet |
| `bridge/` | `wubu_syscall` | 518 | dosgui_wm.h, errno.h, fcntl.h, interrupt.h, prctl.h, signal.h, stat.h, stdio.h,  | wubu_syscall.c  --  WuBuOS HolyC Syscall Bridge Implementation Registers 25 TempleOS/ZealOS-compatib |
| `bridge/` | `wubu_syscall_test` | 127 | dosgui_wm.h, stdio.h, string.h, vbe.h, wubu_syscall.h | wubu_syscall_test.c  --  WuBuOS Syscall Bridge Test Suite Tests the 26 TempleOS/ZealOS-compatible sy |
| `bridge/` | `wubu_syscall_vbe` | 58 | vbe.h, wubu_syscall.h | wubu_syscall_vbe.c -- VBE syscall handlers (self-contained). sys_vbe_*: thin wrappers translating in |
| `compiler/` | `holyc_codegen` | 21 | holyc_codegen.h, holyc_codegen_internal.h | holyc_codegen.c  --  My Seed HolyC Code Generator (Facade) Modular codegen: emit, expr, stmt, api su |
| `compiler/` | `holyc_codegen_api` | 340 | holyc_codegen_internal.h | holyc_codegen_api.c  --  HolyC Code Generator: Public API Top-level compile/eval functions and publi |
| `compiler/` | `holyc_codegen_emit` | 198 | holyc_codegen_internal.h | holyc_codegen_emit.c  --  HolyC Code Generator: x86-64 Emission Helpers Low-level byte emission, ins |
| `compiler/` | `holyc_codegen_expr` | 925 | holyc_codegen_internal.h | holyc_codegen_expr.c  --  HolyC Code Generator: Expression Generation Generates x86-64 machine code  |
| `compiler/` | `holyc_codegen_stmt` | 425 | holyc_codegen_internal.h | holyc_codegen_stmt.c  --  HolyC Code Generator: Statement Generation Generates x86-64 machine code f |
| `compiler/` | `holyc_lexer` | 385 | ctype.h, holyc_types.h, stdarg.h, stdio.h, stdlib.h, string.h | holyc_lexer.c  --  HolyC Lexer Tokenizes HolyC source text into a stream of tokens. Self-contained,  |
| `compiler/` | `holyc_parse` | 759 | holyc.h, stdio.h, stdlib.h, string.h | holyc_parse.c  --  My Seed HolyC Parser + AST Utilities Recursive descent parser: tokens → AST. Port |
| `compiler/` | `holyc_parse_ast` | 136 | holyc_parse_internal.h, stdio.h | holyc_parse_ast.c -- HolyC AST construction/utility helpers (self-contained). hc_ast_new / hc_ast_fr |
| `compiler/` | `holyc_ptx` | 196 | holyc.h, holyc_ptx.h, stdarg.h, stdio.h, stdlib.h, string.h | holyc_ptx.c  --  PTX Backend for HolyC Compiler Emits PTX assembly for NVIDIA GPU Tensor Cores. Targ |
| `compiler/` | `holyc_runtime` | 67 | holyc_codegen.h, stdio.h, stdlib.h, string.h | holyc_runtime.c  --  WuBuOS HolyC personality runtime (host effects) Implements the small set of Hol |
| `compiler/` | `holyc_test` | 258 | holyc.h, stdio.h, stdlib.h, string.h | holyc_test.c  --  My Seed HolyC Compiler Test Suite |
| `compiler/` | `test_holyc_ptx` | 96 | holyc_ptx.h, stdio.h, stdlib.h, string.h | test_holyc_ptx.c  --  PTX Backend Tests Verifies PTX code generation and CUDA integration. |
| `firmware/` | `fw_acpi` | 135 | fw.h, fw_acpi.h, fw_pci.h | fw_acpi.c  --  WuBuFW ACPI table discovery and publication. On a QEMU/x86 machine the firmware norma |
| `firmware/` | `fw_acpiload` | 166 | fw.h, fw_acpi.h, fw_fwcfg.h | fw_acpiload.c  --  Execute QEMU's etc/table-loader linker script. The loader is a sequence of fixed- |
| `firmware/` | `fw_agi` | 132 | fw.h, fw_agi_attest.h, fw_secureboot.h, fw_tpm.h | fw_agi.c  --  WuBuOS AGI OS kernel shim (firmware-resident microkernel). This is the firmware-side h |
| `firmware/` | `fw_ahci` | 277 | fw.h, fw_block.h, fw_pci.h | fw_ahci.c  --  WuBuFW AHCI (SATA) driver. Real port init + command list / FIS / PRDT DMA. AHCI is ho |
| `firmware/` | `fw_ata` | 221 | fw.h, fw_block.h | fw_ata.c  --  WuBuFW ATA PIO block driver (28/48-bit LBA). QEMU's default disk on -drive if=ide is a |
| `firmware/` | `fw_block` | 45 | fw.h, fw_block.h | fw_block.c  --  WuBuFW unified block device layer. fw_media.c used to read straight from ATA PIO, wh |
| `firmware/` | `fw_bs_mem` | 255 | fw.h | fw_bs_mem.c  --  Boot services: TPL, memory, events, timers, misc. |
| `firmware/` | `fw_bs_proto` | 283 | fw.h | fw_bs_proto.c  --  Boot services: protocol database + image services. |
| `firmware/` | `fw_con` | 119 | fw.h | fw_con.c  --  WuBuFW EFI_SIMPLE_TEXT_{INPUT,OUTPUT}_PROTOCOL thunks. These are the ms_abi wrappers o |
| `firmware/` | `fw_drivers` | 81 | fw.h, fw_block.h, fw_pci.h, fw_tpm.h | fw_drivers.c  --  WuBuFW driver manager and boot-stage measurement. Binds real drivers to the device |
| `firmware/` | `fw_e1000` | 263 | fw.h, fw_block.h, fw_pci.h | fw_e1000.c  --  WuBuFW Intel 8254x (e1000) Ethernet driver. QEMU's default NIC is exactly this part  |
| `firmware/` | `fw_fsproto` | 229 | fw.h | fw_fsproto.c  --  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL / EFI_FILE_PROTOCOL bound to a fw_volume. Read-onl |
| `firmware/` | `fw_fwcfg` | 150 | fw.h, fw_fwcfg.h | fw_fwcfg.c  --  QEMU fw_cfg interface and ACPI table installation. QEMU does not place ACPI tables i |
| `firmware/` | `fw_gop` | 161 | fw.h, fw_pci.h | fw_gop.c  --  WuBuFW Graphics Output Protocol over a linear framebuffer. A gaming-console-class firm |
| `firmware/` | `fw_guid` | 38 | efi.h | fw_guid.c  --  WuBuFW well-known EFI GUID definitions. |
| `firmware/` | `fw_handle` | 109 | fw.h | ever an image hands us. |
| `firmware/` | `fw_lib` | 245 | fw.h, stdarg.h | fw_lib.c  --  WuBuFW freestanding string/mem + console (serial + VGA text). The firmware links -nost |
| `firmware/` | `fw_main` | 198 | fw.h, fw_acpi.h, fw_agi.h, fw_block.h, fw_fwcfg.h, fw_pci.h, fw_secureboot.h, fw | fw_main.c  --  WuBuFW entry point and boot manager. Called from reset.S once the CPU is in long mode |
| `firmware/` | `fw_media` | 526 | fw.h, fw_block.h | fw_media.c  --  WuBuFW media stack: MBR/GPT partition scan + FAT12/16/32 read-only filesystem. A UEF |
| `firmware/` | `fw_mem` | 266 | fw.h | fw_mem.c  --  WuBuFW physical memory manager. A real UEFI implementation must hand the OS a coherent |
| `firmware/` | `fw_nvme` | 215 | fw.h, fw_block.h, fw_pci.h | fw_nvme.c  --  WuBuFW NVMe driver. Admin queue + one I/O queue pair, polled completions. Enough to i |
| `firmware/` | `fw_pci` | 257 | fw.h, fw_pci.h | fw_pci.c  --  WuBuFW PCI/PCIe enumeration and resource access. Every real driver (AHCI, NVMe, XHCI,  |
| `firmware/` | `fw_pcires` | 128 | fw.h, fw_pci.h | fw_pcires.c  --  PCI resource (BAR) assignment. On a real boot there is nobody to assign BARs: QEMU  |
| `firmware/` | `fw_pe` | 200 | fw.h | fw_pe.c  --  WuBuFW PE32+ (COFF) loader for EFI applications. Loads a BOOTX64.EFI: validates MZ/PE h |
| `firmware/` | `fw_rt` | 177 | fw.h | fw_rt.c  --  WuBuFW runtime services: time, variables, reset. Variables live in a fixed RAM store (n |
| `firmware/` | `fw_secureboot` | 280 | fw.h, fw_secureboot.h, fw_sha256.h, string.h | fw_secureboot.c  --  WuBuFW authenticated image verification (DB/dbx). Measured boot alone does not  |
| `firmware/` | `fw_sha256` | 115 | fw.h, fw_tpm.h | fw_sha256.c  --  SHA-256 for WuBuFW (measured boot + image verification). FIPS 180-4. Self-contained |
| `firmware/` | `fw_shell` | 210 | fw.h, fw_block.h, fw_pci.h, fw_tpm.h | fw_shell.c  --  WuBuFW interactive EFI-style shell. A real firmware ships an interactive shell: it i |
| `firmware/` | `fw_table` | 189 | fw.h | fw_table.c  --  Assemble the EFI system / boot / runtime service tables. |
| `firmware/` | `fw_time` | 84 | fw.h | fw_time.c  --  WuBuFW timing + CMOS RTC. TSC is calibrated against the PIT (channel 2 gate, no inter |
| `firmware/` | `fw_tpm` | 388 | fw.h, fw_acpi.h, fw_tpm.h, string.h | fw_tpm.c  --  WuBuFW TPM 2.0 driver (TIS/FIFO + CRB) and measured boot. Kernel-level anti-cheat (EAC |
| `firmware/` | `fw_tpmlog` | 119 | fw.h, fw_tpm.h | fw_tpmlog.c  --  TCG 2.0 crypto-agile event log. The event log is what makes PCR values *meaningful* |
| `firmware/` | `fw_xhci` | 128 | fw.h, fw_pci.h | fw_xhci.c  --  WuBuFW USB 3 (xHCI) host controller driver. Firmware-scope USB: bring the controller  |
| `firmware/` | `loader` | 301 | efi.h, fw_agi_attest.h, sha256.h, stddef.h, stdint.h | loader.c -- WuBuOS WuBuFW chainloader (the REAL kernel as measured payload). This is the bridge betw |
| `firmware/` | `sha256` | 94 | sha256.h, string.h | sha256.c -- self-contained SHA-256 for the WuBuOS EFI chainloader. FIPS 180-4. Freestanding C11 (onl |
| `firmware/` | `hello` | 243 | efi.h | hello.c  --  WuBuOS test EFI application. Exercises the firmware for real: ConOut, memory map, Alloc |
| `firmware/` | `mkesp` | 340 | stdint.h, stdio.h, stdlib.h, string.h | mkesp.c  --  WuBuOS FAT32 ESP image builder (C11, self-contained). Creates a GPT-partitioned disk im |
| `firmware/` | `mkpe` | 191 | stdint.h, stdio.h, stdlib.h, string.h | mkpe.c  --  WuBuOS PE32+ EFI image builder (C11, no toolchain deps). GCC on Linux cannot emit PE32+, |
| `framework/` | `test_bonzi_comfy` | 101 | bonzi.h, comfy.h, dosgui_wm.h, stdio.h, string.h, wubufx.h | test_bonzi_comfy.c -- Verify the AGI human-interface plumbing. Proves (headless, no display): 1. Bon |
| `framework/` | `wubufx` | 220 | stdio.h, stdlib.h, string.h, wubu_edr.h, wubu_holyd.h, wubufx.h | wubufx.c -- WuBuFX: WuBuOS Application Framework (implementation) Self-contained. Composition root w |
| `framework/` | `wubufx_apps` | 177 | bonzi.h, comfy.h, dosgui_apps.h, dosgui_wm.h, stdio.h, stdlib.h, string.h, wubu_ | wubufx_apps.c -- WuBuFX application registry (real engines, no placeholders) Phase C binding: the de |
| `framework/` | `wubufx_apps_test` | 60 | dosgui_wm.h, stdio.h, string.h, wubufx.h, wubufx_apps.h | wubufx_apps_test.c -- WuBuFX real-app binding regression tests Proves Phase C: every registered app  |
| `framework/` | `wubufx_test` | 99 | stdint.h, stdio.h, string.h, wubu_edr.h, wubufx.h | wubufx_test.c -- WuBuFX framework regression tests Verifies the namespace-first contract end to end: |
| `gui/` | `dosgui_daemon_panel` | 526 | dosgui_daemon_panel.h, dosgui_desktop.h, dosgui_wm.h, epoll.h, errno.h, fcntl.h, | dosgui_daemon_panel.c  --  WuBuOS Desktop Daemon Integration Panel Cell 400-402: Bridges wubu_archd  |
| `gui/` | `dosgui_daemon_panel_test` | 124 | dosgui_daemon_panel.h, stdint.h, stdio.h, stdlib.h, string.h | dosgui_daemon_panel_test.c  --  Daemon Panel Unit Test Suite Tests daemon panel logic with stubbed G |
| `gui/` | `dosgui_desktop` | 213 | dosgui_apps.h, dosgui_daemon_panel.h, dosgui_desktop.h, dosgui_era_apps.h, dosgu | dosgui_desktop.c  --  WuBuOS DosGui Desktop Implementation Cell 401: THEMED Win98/XP desktop with la |
| `gui/` | `dosgui_dos_window` | 141 | dosgui_wm.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_dos_proc.h | dosgui_dos_window.c -- Render a WuBuOS 16-bit DOS process in a desktop window. The window blits the  |
| `gui/` | `dosgui_dos_window_test` | 106 | dosgui_dos_window.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_container.h, wu | dosgui_dos_window_test.c -- Real end-to-end test for the DOS Box window. Exercises the genuine engin |
| `gui/` | `dosgui_dos_window_test_stub` | 24 | dosgui_wm.h, stdlib.h, string.h | dosgui_dos_window_test_stub.c -- Minimal WM window primitive stubs for the DOS Box window test. We a |
| `gui/` | `dosgui_era_apps` | 230 | dosgui_era_apps.h, dosgui_startmenu.h, dosgui_startmenu_internal.h, stdio.h, std | dosgui_era_apps.c -- One representative application per computing era, each tagged with the VSL sysc |
| `gui/` | `dosgui_era_apps_test` | 111 | dosgui_era_apps.h, dosgui_startmenu.h, stat.h, stdio.h, stdlib.h, string.h, unis | dosgui_era_apps_test.c -- regression test for the "one app per era" registry + launcher. Asserts tha |
| `gui/` | `dosgui_explorer` | 524 | ctype.h, dirent.h, dlfcn.h, dosgui_explorer.h, dosgui_explorer_internal.h, dosgu | dosgui_explorer.c  --  WuBuOS File Manager (Win98/XP Explorer Shell) Phase 5: Full-featured file man |
| `gui/` | `dosgui_explorer_drives` | 63 | dosgui_explorer_internal.h, stdio.h, string.h | dosgui_explorer_drives.c -- Drive / volume enumeration subsystem. Self-contained: enumerates mounted |
| `gui/` | `dosgui_explorer_format` | 65 | dosgui_explorer.h, stdio.h, string.h, time.h | dosgui_explorer_format.c -- Shared type/format helpers for the explorer. These are PUBLIC API (decla |
| `gui/` | `dosgui_explorer_fs` | 66 | dirent.h, stat.h, stdint.h, string.h, types.h | dosgui_explorer_fs.c -- 9P/Styx filesystem backend for the WuBuOS Explorer. Self-contained module ex |
| `gui/` | `dosgui_explorer_fsops` | 135 | dirent.h, dosgui_explorer_internal.h, errno.h, fcntl.h, stat.h, stdio.h, stdlib. | dosgui_explorer_fsops.c -- Explorer file-operation workers. Self-contained module extracted from dos |
| `gui/` | `dosgui_explorer_info` | 100 | dosgui_explorer.h, dosgui_explorer_internal.h, stat.h, stdio.h, string.h | dosgui_explorer_info.c -- WuBuOS explorer: file metadata extraction. Extracted from dosgui_explorer. |
| `gui/` | `dosgui_explorer_input` | 436 | ctype.h, dosgui_explorer.h, dosgui_explorer_internal.h, stdio.h, string.h, vbe.h | dosgui_explorer_input.c -- Explorer input handling (keyboard + mouse) Extracted from dosgui_explorer |
| `gui/` | `dosgui_explorer_ops` | 174 | dosgui_explorer.h, dosgui_explorer_internal.h, errno.h, fcntl.h, stat.h, stdint. | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `gui/` | `dosgui_explorer_preview` | 66 | dosgui_explorer_internal.h, fcntl.h, string.h | dosgui_explorer_preview.c -- File preview subsystem. Self-contained: builds the preview metadata/tex |
| `gui/` | `dosgui_explorer_render` | 506 | dosgui_explorer_internal.h, dosgui_wm.h, stdint.h, stdio.h, string.h, vbe.h | dosgui_explorer_render.c  --  Win98 Explorer render layer Extracted from dosgui_explorer.c — all dra |
| `gui/` | `dosgui_explorer_test` | 533 | assert.h, dosgui_explorer.h, stdint.h, stdio.h, stdlib.h, string.h | dosgui_explorer_test.c  --  Test suite for dosgui_explorer |
| `gui/` | `dosgui_explorer_test_stub` | 106 | dirent.h, dosgui_explorer.h, dosgui_wm.h, stat.h, stdlib.h, string.h, time.h, ty | dosgui_explorer_test_stub.c  --  Stub functions for dosgui_explorer tests Provides minimal implement |
| `gui/` | `dosgui_explorer_tree` | 218 | dosgui_explorer_internal.h, stdio.h, stdlib.h, string.h | dosgui_explorer_tree.c -- Tree view subsystem + shared entry helpers. Owns directory-tree population |
| `gui/` | `dosgui_explorer_zip` | 335 | ctype.h, dlfcn.h, dosgui_explorer_internal.h, fcntl.h, stat.h, stdio.h, stdlib.h | dosgui_explorer_zip.c  --  WuBuOS Explorer Zip Archive Module Extracted from dosgui_explorer.c lines |
| `gui/` | `dosgui_service_mgr` | 156 | ctype.h, dosgui_service_mgr.h, stdio.h, stdlib.h, string.h | dosgui_service_mgr.c -- Desktop-side service/autostart manager (E3). Real wiring of wubu_archd (16/1 |
| `gui/` | `dosgui_service_mgr_test` | 76 | assert.h, dosgui_service_mgr.h, stdio.h, string.h | dosgui_service_mgr_test.c -- Regression test for E3 integration: wubu_archd wired as the Desktop's a |
| `gui/` | `dosgui_startmenu` | 843 | ctype.h, dirent.h, dosgui_desktop.h, dosgui_era_apps.h, dosgui_startmenu.h, dosg | dosgui_startmenu.c  --  WuBuOS DosGui Start Menu Implementation Cell 402: THEMED Cascading Start Men |
| `gui/` | `dosgui_startmenu_db` | 183 | ctype.h, dirent.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_startmenu_intern | dosgui_startmenu_db.c  --  WuBuOS Start Menu model constructor Extracted from dosgui_startmenu.c (Ce |
| `gui/` | `dosgui_startmenu_power` | 45 | dosgui_startmenu_internal.h, stdio.h | dosgui_startmenu_power.c -- Power options subsystem for the Start menu. Self-contained: power action |
| `gui/` | `dosgui_startmenu_search` | 104 | dosgui_startmenu_internal.h, stdlib.h, string.h, wubu_theme.h | dosgui_startmenu_search.c -- Start menu search + recent apps. Self-contained subsystem extracted fro |
| `gui/` | `dosgui_startmenu_test` | 103 | dosgui_startmenu.h, stdio.h, vbe.h, wubu_theme.h | dosgui_startmenu_test.c  --  WuBuOS DosGui Start Menu Test Suite (Cell 402) |
| `gui/` | `dosgui_startmenu_test_stub` | 107 | dosgui_startmenu.h, stdbool.h, wubu_dos_proc.h, wubu_exec.h, wubu_mime.h, wubu_t | dosgui_startmenu_test_stub.c  --  Stubs for dosgui_startmenu_test without desktop integration |
| `gui/` | `dosgui_startmenu_tree` | 105 | dosgui_startmenu_internal.h, stdlib.h, string.h, wubu_theme.h | dosgui_startmenu_tree.c -- Start menu program-tree (All Programs). Self-contained subsystem extracte |
| `gui/` | `dosgui_term` | 502 | ctype.h, dosgui_term.h, dosgui_term_internal.h, dosgui_term_pty.h, dosgui_wm.h,  | dosgui_term.c  --  WuBuOS Terminal (PTY + HolyC REPL + Tabbed) Phase 6: Full-featured terminal with: |
| `gui/` | `dosgui_term_ansi` | 445 | dosgui_term_internal.h, errno.h, string.h, unistd.h | dosgui_term_ansi.c  --  WuBuOS Terminal shared VT100/ANSI parser Extracted from dosgui_term.c (2026- |
| `gui/` | `dosgui_term_pty` | 526 | ctype.h, dosgui_term_internal.h, dosgui_term_pty.h, dosgui_wm.h, errno.h, fcntl. | dosgui_term_pty.c  --  WuBuOS Terminal PTY Backend Implementation Extracted from dosgui_term.c (2026 |
| `gui/` | `dosgui_term_render` | 192 | dosgui_term_internal.h, string.h | dosgui_term_render.c -- WuBuOS terminal rendering layer. Self-contained module extracted from dosgui |
| `gui/` | `dosgui_term_tabs` | 156 | dosgui_term_internal.h, stdio.h, stdlib.h, string.h | dosgui_term_tabs.c -- Terminal tab-management subsystem. Self-contained: tab creation/close/switch/m |
| `gui/` | `dosgui_term_test` | 278 | ctype.h, dosgui_term.h, dosgui_term_internal.h, dosgui_wm.h, fcntl.h, pty.h, sig | dosgui_term_test.c  --  WuBuOS Terminal Test Suite Tests PTY backend, ANSI parser, scrollback, copy/ |
| `gui/` | `dosgui_term_test_stub` | 80 | dosgui_term.h, dosgui_wm.h, stdio.h, stdlib.h, string.h | dosgui_term_test_stub.c  --  Stubs for dosgui_term_test without full dependencies Provides minimal i |
| `gui/` | `dosgui_window_chrome` | 359 | dosgui_wm.h, dosgui_wm_internal.h, string.h, vbe.h, wubu_theme.h | dosgui_window_chrome.c  --  Standardized Window Chrome for WuBuOS Centralized drawing of ALL window  |
| `gui/` | `dosgui_wm` | 90 | dirent.h, dosgui_wm_internal.h, errno.h, stat.h, wubu_wallpaper.h | dosgui_wm.c  --  WuBuOS DosGui Window Manager facade Cell 400: Fable Windowing Agent — THEMED EDITIO |
| `gui/` | `dosgui_wm_clock` | 24 | dosgui_wm_internal.h, stdio.h, time.h | dosgui_wm_clock.c -- WuBuOS DosGui WM: taskbar clock Self-contained concern split out of dosgui_wm_c |
| `gui/` | `dosgui_wm_ctxmenu` | 433 | dosgui_wm_internal.h, stdio.h, stdlib.h, string.h, wubu_trash.h | dosgui_wm_ctxmenu.c  --  WuBuOS DosGui WM: context-menu action wiring Facade that binds the generic  |
| `gui/` | `dosgui_wm_ctxmenu_engine` | 196 | dosgui_wm_internal.h | dosgui_wm_ctxmenu_engine.c -- WuBuOS DosGui WM: context-menu engine Self-contained concern split out |
| `gui/` | `dosgui_wm_desktop` | 318 | dirent.h, dosgui_wm_internal.h, errno.h, fcntl.h, stat.h, stdlib.h, strings.h | dosgui_wm_desktop.c -- Desktop icons subsystem for the WuBuOS WM. Self-contained module extracted fr |
| `gui/` | `dosgui_wm_holyc_term` | 187 | dosgui_wm_holyc_term.h, dosgui_wm_internal.h, holyc_codegen.h, stdio.h, stdlib.h | dosgui_wm_holyc_term.c  --  HolyC Terminal subsystem for dosgui_wm Provides a REPL terminal window f |
| `gui/` | `dosgui_wm_icon_glyphs` | 132 | dosgui_wm.h, dosgui_wm_internal.h, stdint.h, string.h | dosgui_wm_icon_glyphs.c -- Desktop icon glyph rendering (Chicago -> XP). The old desktop drew every  |
| `gui/` | `dosgui_wm_icons` | 79 | dosgui_wm_internal.h | dosgui_wm_icons.c -- Desktop icon subsystem for the window manager. Self-contained: desktop icon gri |
| `gui/` | `dosgui_wm_input` | 409 | dosgui_startmenu.h, dosgui_wm.h, dosgui_wm_internal.h, stdio.h, string.h, time.h | dosgui_wm_input.c -- WuBuOS DosGui WM: input dispatch (key + mouse) Self-contained concern split out |
| `gui/` | `dosgui_wm_layout` | 233 | dosgui_wm.h, dosgui_wm_internal.h, stdint.h, string.h, wubu_theme.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `gui/` | `dosgui_wm_render` | 167 | dosgui_wm.h, dosgui_wm_internal.h, stdbool.h, stdint.h, stdlib.h, string.h | dosgui_wm_render.c -- WuBuOS WM rendering (single module) ONE entry point: dosgui_wm_render(fb, w, h |
| `gui/` | `dosgui_wm_systray` | 207 | dosgui_wm_internal.h | dosgui_wm_systray.c  --  System Tray + Notification Center Extracted from dosgui_wm.c for modularity |
| `gui/` | `dosgui_wm_taskbar` | 196 | dosgui_wm_internal.h, time.h | dosgui_wm_taskbar.c -- Taskbar render + geometry. Self-contained: renders the taskbar (Start button, |
| `gui/` | `dosgui_wm_test` | 881 | dirent.h, dosgui_wm.h, dosgui_wm_internal.h, stat.h, stdio.h, stdlib.h, string.h | dosgui_wm_test.c  --  WuBuOS DosGui Window Manager Test Suite Cell 400: Fable Windowing Agent test s |
| `gui/` | `dosgui_wm_test_stub` | 42 | dosgui_startmenu.h, dosgui_wm.h, hosted.h, stdbool.h, wubu_theme.h | dosgui_wm_test_stub.c  --  Stubs for WM unit tests without full desktop integration. Provides no-op  |
| `gui/` | `dosgui_wm_window` | 270 | dosgui_wm_internal.h, stdio.h, string.h, wubu_settings.h | dosgui_wm_window.c -- WuBuOS DosGui WM: window lifecycle + icons Self-contained concern split out of |
| `gui/` | `dosgui_wm_window_state` | 117 | dosgui_wm_internal.h, string.h | dosgui_wm_window_state.c -- WuBuOS DosGui WM: window state + modal dialogs Self-contained concern sp |
| `gui/` | `gui_dbuf` | 277 | gui_dbuf.h, stdlib.h, string.h | gui_dbuf.c  --  WuBuOS Double-Buffered GUI Renderer Implementation Cell 101: Flicker-free rendering  |
| `gui/` | `gui_dbuf_test` | 343 | gui_dbuf.h, stdio.h, string.h | gui_dbuf_test.c  --  Test Suite for Double-Buffered GUI Renderer Cell 101: Tests double buffering, d |
| `gui/` | `standalone_hosted_shim` | 34 | dosgui_wm.h, hosted.h | standalone_hosted_shim.c -- standalone app shim for hosted-only symbols. The WuBuOS GUI references t |
| `gui/` | `wubu_clipboard` | 297 | hosted.h, poll.h, primary-selection-client.header, stdio.h, stdlib.h, string.h,  | wubu_clipboard.c  --  WuBuOS Clipboard Manager Implementation Phase 2: Wayland data device + primary |
| `gui/` | `wubu_clipboard_mime` | 61 | wubu_clipboard_internal.h | wubu_clipboard_mime.c -- Clipboard MIME-entry helpers (self-contained). Pure helpers for the X/prima |
| `gui/` | `wubu_clipboard_test` | 318 | assert.h, stdio.h, stdlib.h, string.h, wubu_clipboard.h | wubu_clipboard_test.c -- WuBuOS Clipboard Manager Test (logic only) Tests internal clipboard logic w |
| `gui/` | `wubu_clipboard_wl` | 377 | hosted.h, poll.h, primary-selection-client.header, stdio.h, stdlib.h, string.h,  | wubu_clipboard_wl.c -- WuBuOS Clipboard Wayland/X11 protocol transport Extracted from wubu_clipboard |
| `gui/` | `wubu_compositor` | 494 | stdio.h, stdlib.h, string.h, styx.h, unistd.h, vulkan.h, wayland-server-core.h,  | wubu_compositor.c  --  WuBuOS Wayland Compositor Implementation Minimal Wayland compositor using lib |
| `gui/` | `wubu_compositor_standalone` | 414 | egl.h, fcntl.h, gl2.h, mman.h, stdio.h, stdlib.h, string.h, unistd.h, wayland-cl | wubu_compositor_standalone.c  --  Standalone Compositor Implementation Implements the standalone com |
| `gui/` | `wubu_compositor_test` | 135 | stdio.h, stdlib.h, string.h, styx.h, wubu_compositor.h | wubu_compositor_test.c -- Test for WuBuOS Compositor |
| `gui/` | `wubu_deploy` | 520 | dirent.h, errno.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, time.h, unistd. | wubu_deploy.c - WuBuOS Multi-Target Deployment Implementation Build scripts for bare metal, WSL2, OC |
| `gui/` | `wubu_deploy_config` | 77 | string.h, wubu_deploy.h | wubu_deploy_config.c -- WuBuOS deploy: default target config getters. Extracted from wubu_deploy.c ( |
| `gui/` | `wubu_deploy_gen` | 248 | stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_deploy_internal.h | wubu_deploy_gen.c -- Deployment config-file generators. Self-contained module extracted from wubu_de |
| `gui/` | `wubu_deploy_test` | 381 | stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_deploy.h | wubu_deploy_test.c - Unit tests for WuBuOS Multi-Target Deployment |
| `gui/` | `wubu_deploy_util` | 79 | dirent.h, errno.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wait. | wubu_deploy_util.c -- Deployment layer file/command utilities. Self-contained module extracted from  |
| `gui/` | `wubu_gamelib` | 339 | ctype.h, dirent.h, errno.h, glob.h, libgen.h, limits.h, stat.h, stdio.h, stdlib. | Internal State |
| `gui/` | `wubu_gamelib_config` | 66 | stdio.h, string.h, wubu_gamelib.h, wubu_gamelib_internal.h | wubu_gamelib_config.c -- WuBuOS gamelib: config save/load (JSON file I/O). Extracted from wubu_gamel |
| `gui/` | `wubu_gamelib_playtime` | 21 | time.h, wubu_gamelib.h | wubu_gamelib_playtime.c -- WuBuOS gamelib: playtime tracking. Extracted from wubu_gamelib.c (separab |
| `gui/` | `wubu_gamelib_scan` | 264 | dirent.h, glob.h, stat.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wubu_ga | wubu_gamelib_scan.c -- Game library source scanners. Self-contained module extracted from wubu_gamel |
| `gui/` | `wubu_gamelib_startmenu` | 51 | stdio.h, string.h, wubu_gamelib.h, wubu_gamelib_internal.h | wubu_gamelib_startmenu.c -- WuBuOS gamelib: start-menu (.desktop) integration. Extracted from wubu_g |
| `gui/` | `wubu_gamelib_test` | 101 | assert.h, stdio.h, string.h, wubu_gamelib.h | Test adding a game |
| `gui/` | `wubu_json` | 155 | ctype.h, stdlib.h, string.h, wubu_json.h | wubu_json.c -- Minimal JSON parser. Self-contained module extracted from wubu_settings.c. Uses the s |
| `gui/` | `wubu_mime` | 420 | ctype.h, dirent.h, glob.h, stat.h, stdio.h, stdlib.h, string.h, strings.h, unist | Internal State |
| `gui/` | `wubu_mime_desktop` | 135 | ctype.h, stdio.h, stdlib.h, string.h, wubu_mime_internal.h | wubu_mime_desktop.c -- MIME .desktop file parser. Self-contained module extracted from wubu_mime.c:  |
| `gui/` | `wubu_mime_test` | 49 | assert.h, stdio.h, string.h, wubu_mime.h | Test built-in MIME types |
| `gui/` | `wubu_notify` | 416 | dosgui_wm.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, vbe.h, wubu_notify.h | wubu_notify.c  --  WuBuOS Notification Daemon Implementation Phase 2: libnotify-compatible notificat |
| `gui/` | `wubu_pkgmgr` | 291 | wubu_pkgmgr_internal.h | wubu_pkgmgr.c  --  WuBuOS Package Manager (Facade) Submodules: wubu_pkgmgr_pkg.c     - package creat |
| `gui/` | `wubu_pkgmgr_db` | 79 | wubu_pkgmgr_internal.h | wubu_pkgmgr_db.c -- Package manager database subsystem. Self-contained: progress reporting + SQLite  |
| `gui/` | `wubu_pkgmgr_install` | 379 | glob.h, unistd.h, wait.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_install.c  --  WuBuOS Package Manager: Install / Remove / Upgrade Depends on the facade  |
| `gui/` | `wubu_pkgmgr_manifest` | 57 | stdio.h, stdlib.h, string.h, wubu_pkgmgr.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_manifest.c -- WuBuOS pkgmgr: manifest -> JSON serialization. Extracted from wubu_pkgmgr. |
| `gui/` | `wubu_pkgmgr_pkg` | 512 | wubu_pkgmgr_internal.h | wubu_pkgmgr_pkg.c  --  WuBuOS Package Manager: Pkg |
| `gui/` | `wubu_pkgmgr_remote` | 250 | wubu_pkgmgr_internal.h | wubu_pkgmgr_remote.c -- Remote repository index subsystem. Self-contained module: fetches a repo's i |
| `gui/` | `wubu_pkgmgr_resolve` | 148 | stdlib.h, string.h, wubu_pkgmgr.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_resolve.c -- WuBuOS pkgmgr: dependency resolution + conflict check. Extracted from wubu_ |
| `gui/` | `wubu_pkgmgr_test` | 673 | errno.h, in.h, inet.h, socket.h, stat.h, stdio.h, stdlib.h, string.h, time.h, un | wubu_pkgmgr_test.c - Unit tests for WuBuOS Package Manager |
| `gui/` | `wubu_pkgmgr_txn` | 90 | glob.h, unistd.h, wait.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_txn.c  --  WuBuOS Package Manager: Txn |
| `gui/` | `wubu_pkgmgr_verify` | 108 | stdlib.h, string.h, wubu_pkgmgr.h, wubu_pkgmgr_internal.h | wubu_pkgmgr_verify.c -- WuBuOS pkgmgr: installed-package integrity verification. Extracted from wubu |
| `gui/` | `wubu_proton` | 339 | dirent.h, errno.h, ftw.h, glob.h, libgen.h, limits.h, pwd.h, stat.h, stdio.h, st | -- Recursive directory removal (replaces system("rm -rf")) -------- |
| `gui/` | `wubu_proton_config` | 80 | stat.h, stdio.h, stdlib.h, string.h, wubu_proton_internal.h | wubu_proton_config.c -- Proton config persistence. Self-contained module extracted from wubu_proton. |
| `gui/` | `wubu_proton_dxvk` | 109 | stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_dxvk_conf.h, wubu_proton_dxv | wubu_proton_dxvk.c -- Proton DXVK/VKD3D config subsystem (GUI desktop-proton). Thin adapter over the |
| `gui/` | `wubu_proton_exec` | 248 | errno.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h, wubu_pr | wubu_proton_exec.c -- Proton execution / launch subsystem. Self-contained module extracted from wubu |
| `gui/` | `wubu_proton_test` | 73 | assert.h, stdio.h, string.h, wubu_proton.h | Test Steam detection |
| `gui/` | `wubu_proton_util` | 176 | dirent.h, errno.h, ftw.h, glob.h, libgen.h, limits.h, pwd.h, stat.h, stdio.h, st | wubu_proton_util.c -- Proton layer filesystem / parsing utilities. Self-contained module extracted f |
| `gui/` | `wubu_screenshot` | 454 | dosgui_wm.h, hosted.h, limits.h, math.h, stat.h, stdio.h, stdlib.h, string.h, ti | Internal State |
| `gui/` | `wubu_screenshot_clipboard_test` | 70 | assert.h, stdio.h, stdlib.h, string.h, wubu_screenshot.h | Regression test for wubu_screenshot_to_clipboard (was a no-op returning true with nothing stored). B |
| `gui/` | `wubu_screenshot_draw` | 122 | wubu_screenshot_internal.h | wubu_screenshot_draw.c -- Screenshot annotation drawing primitives (self-contained). draw_line (Bres |
| `gui/` | `wubu_screenshot_png` | 239 | stdint.h, stdio.h, stdlib.h, string.h, wubu_screenshot.h, zlib.h | wubu_screenshot_png.c -- Screenshot PNG encoder subsystem. Self-contained module extracted from wubu |
| `gui/` | `wubu_screenshot_test` | 96 | assert.h, stdio.h, stdlib.h, string.h, wubu_screenshot.h, wubu_theme.h | Parse big-endian uint32 from a PNG buffer offset. |
| `gui/` | `wubu_session` | 312 | dosgui_startmenu.h, dosgui_wm.h, stat.h, stdio.h, stdlib.h, string.h, time.h, ty | wubu_session.c  --  WuBuOS Session Manager Implementation Phase 2: Session management, auto-start, s |
| `gui/` | `wubu_session_autostart` | 285 | stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_session_internal.h | wubu_session_autostart.c -- Autostart entries subsystem. Self-contained: owns g_autostart[]/g_autost |
| `gui/` | `wubu_settings` | 70 | stdio.h, string.h, wubu_settings_internal.h, wubu_theme.h | wubu_settings.c  --  WuBuOS Settings Daemon (facade) Owns the settings lifecycle + public accessor A |
| `gui/` | `wubu_settings_defaults` | 136 | errno.h, stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_settings_i | wubu_settings_defaults.c -- WuBuOS settings: state + default factories Self-contained concern split  |
| `gui/` | `wubu_settings_io` | 241 | stdio.h, stdlib.h, string.h, time.h, wubu_settings_internal.h | wubu_settings_io.c -- WuBuOS settings: JSON (de)serialization Self-contained concern split out of wu |
| `gui/` | `wubu_theme` | 282 | stdbool.h, string.h, wubu_theme.h | wubu_theme.c  --  WuBuOS Theme Engine Implementation Cell 394: Runtime-switchable themes. Win98 Clas |
| `gui/` | `wubu_trash` | 516 | dirent.h, errno.h, fcntl.h, libgen.h, stat.h, stdio.h, stdlib.h, string.h, time. | Internal State |
| `gui/` | `wubu_trash_test` | 77 | assert.h, libgen.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_trash.h | Create test file |
| `gui/` | `wubu_ui` | 114 | dosgui_wm.h, string.h, wubu_edr.h, wubu_ui.h | wubu_ui.c -- AGI UI automation layer (see wubu_ui.h). Every action routes through dosgui_wm_handle_m |
| `gui/` | `wubu_ui_test` | 91 | dosgui_wm.h, stdio.h, string.h, vbe.h, wubu_theme.h, wubu_ui.h | wubu_ui_test.c -- AGI UI automation layer test suite. Proves the central AGI-OS claim: a synthetic d |
| `gui/` | `wubu_wallpaper` | 221 | stdio.h, stdlib.h, string.h, strings.h, unistd.h, wubu_wallpaper.h | wubu_wallpaper.c -- WuBuOS Wallpaper Decoder + Placement Real implementation: decodes a BMP (24/32 b |
| `gui/` | `wubu_wallpaper_test` | 120 | stdio.h, stdlib.h, string.h, wubu_wallpaper.h | wubu_wallpaper_test.c -- Real wallpaper decode + placement verification. No GPU needed: writes a rea |
| `gui/` | `wubu_wayland_stub` | 19 | wayland_state.h | wubu_wayland_stub.c  --  weak Wayland-state default for standalone binaries. src/gui/wubu_screenshot |
| `gui/` | `wubu_welcome` | 145 | dosgui_wm.h, errno.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_theme. | wubu_welcome.c  --  WuBuOS Welcome Dialog (UX Stream E) First-run dialog: "Welcome to WuBuOS" with q |
| `gui/` | `wubu_wm` | 213 | stdio.h, stdlib.h, string.h, vbe.h, wubu_theme.h, wubu_wm_internal.h | wubu_wm.c  --  WuBuOS Window Manager (Core) Core WM operations: init, window lifecycle, GAAD snap. S |
| `gui/` | `wubu_wm_desktop` | 49 | wubu_wm_internal.h | wubu_wm_desktop.c  --  WuBuOS Window Manager Virtual Desktops Virtual desktop (workspace) management |
| `gui/` | `wubu_wm_input` | 152 | string.h, vbe.h, wubu_wm_internal.h | wubu_wm_input.c  --  WuBuOS Window Manager Input Handling Keyboard and mouse input dispatch for the  |
| `gui/` | `wubu_wm_render` | 142 | stdio.h, string.h, vbe.h, wubu_wm_internal.h | wubu_wm_render.c  --  WuBuOS Window Manager Rendering Draws the WM desktop: window chrome (title bar |
| `gui/` | `wubu_wm_test` | 242 | assert.h, stdio.h, string.h, wubu_wm.h | wubu_wm_test.c  --  Tests for WuBuOS Window Manager Cell 394/395: Theme engine + WM with drag/snap/d |
| `gui/` | `xdg-shell-client-protocol` | 183 | stdint.h, stdlib.h, wayland-util.h | Generated by wayland-scanner 1.22.0 |
| `hosted/` | `hosted` | 219 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, dosgui_wm_holyc_ter | hosted.c — WuBuOS Hosted Mode Launcher (Inferno emu-style) — facade WuBuOS as a clickable Linux bina |
| `hosted/` | `hosted_pe` | 51 | hosted_internal.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_ct_isolate.h, wub | hosted_pe.c -- WuBuOS hosted-mode Windows/PE launch executor Self-contained concern split out of hos |
| `hosted/` | `hosted_render` | 46 | dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, hosted_internal.h, input.h, s | hosted_render.c -- WuBuOS hosted-mode frame composition + input routing Self-contained concern split |
| `hosted/` | `hosted_run` | 169 | dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, hosted_internal.h, stdio.h, s | hosted_run.c -- WuBuOS hosted-mode run loop, shutdown, blit + accessors Self-contained concern split |
| `hosted/` | `hosted_styxfs` | 155 | dosgui_wm.h, hosted.h, hosted_internal.h, input.h, memory.h, stdlib.h, string.h, | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `hosted/` | `hosted_test` | 462 | dosgui_startmenu.h, dosgui_wm.h, hosted.h, stdio.h, stdlib.h, string.h, styx.h,  | hosted_test.c  --  WuBuOS Hosted Mode Behavioral Test Suite Tests: hosted init/shutdown, Styx namesp |
| `hosted/` | `hosted_wayland` | 89 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, fcntl.h, hosted.h,  | Thin orchestration: owns the public hosted_wl_* entry points declared in hosted_internal.h. SHM pool |
| `hosted/` | `hosted_wayland_input` | 411 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, fcntl.h, hosted.h,  |  |
| `hosted/` | `hosted_wayland_shm` | 105 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, fcntl.h, hosted.h,  |  |
| `hosted/` | `hosted_wayland_surface` | 241 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, fcntl.h, hosted.h,  |  |
| `hosted/` | `primary-selection-private` | 115 | stdint.h, stdlib.h, wayland-util.h | Generated by wayland-scanner 1.22.0 |
| `hosted/` | `wubu_display` | 297 | errno.h, fcntl.h, input.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_display.h | wubu_display.c  --  WuBuOS Display Backend (DRM/KMS + X11 dual) Cell 380: Try DRM/KMS first, fall ba |
| `hosted/` | `wubu_display_test` | 64 | stdio.h, stdlib.h, string.h, wubu_display.h | wubu_drm_direct_test.c  --  Test for direct DRM/KMS implementation (Cells 388/389) |
| `hosted/` | `wubu_gbm` | 185 | errno.h, fcntl.h, ioctl.h, mman.h, stdint.h, stdlib.h, string.h, unistd.h, wubu_ | wubu_gbm.c  --  WuBuOS Custom GBM (Generic Buffer Management) Cell 389: Pure C GBM implementation wi |
| `hosted/` | `wubu_metal` | 485 | dirent.h, dlfcn.h, errno.h, fcntl.h, input.h, interrupt.h, memory.h, mman.h, sta | wubu_metal.c  --  WuBuOS Bare-Metal Boot + WSL2 GUI Abstraction Cell 400: Implementation of unified  |
| `hosted/` | `wubu_metal_audio` | 300 | asoundlib.h, dlfcn.h, error.h, format-utils.h, pipewire.h, simple.h, stdio.h, wu | wubu_metal_audio.c -- WuBuOS Metal audio backends (split from wubu_metal.c). Self-contained: ALSA +  |
| `hosted/` | `wubu_metal_drm` | 369 | drm_fourcc.h, errno.h, fcntl.h, mman.h, stat.h, stdio.h, stdlib.h, string.h, uni | wubu_metal_drm.c -- WuBuOS Metal DRM/KMS display backend (split from wubu_metal.c). Self-contained:  |
| `hosted/` | `wubu_metal_evdev` | 206 | dirent.h, dlfcn.h, errno.h, fb.h, fcntl.h, input.h, ioctl.h, mman.h, stat.h, std | wubu_metal_evdev.c -- WuBuOS evdev input backend (extracted from wubu_metal.c). Mirror of the origin |
| `hosted/` | `wubu_metal_test` | 108 | assert.h, stdio.h, stdlib.h, wubu_metal.h | wubu_metal_test.c  --  Tests for bare-metal + WSL2 abstraction layer |
| `hosted/` | `wubu_metal_vulkan` | 119 | stdio.h, string.h, vulkan.h, vulkan_wayland.h, vulkan_xcb.h, vulkan_xlib.h, wubu | wubu_metal_vulkan.c -- WuBuOS Vulkan surface creation (extracted from wubu_metal.c). Self-contained: |
| `hosted/` | `wubu_metal_x11` | 117 | Xlib.h, Xutil.h, dirent.h, dlfcn.h, errno.h, fb.h, fcntl.h, input.h, ioctl.h, mm | wubu_metal_x11.c -- WuBuOS X11 display backend (extracted from wubu_metal.c). Mirror of the original |
| `hosted/` | `wubu_vulkan_cmd` | 124 | stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_cmd.c -- WuBuOS Vulkan command-pool + queue-submit helpers (extracted from the monolithi |
| `hosted/` | `wubu_vulkan_compute` | 326 | stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_compute.c -- WuBuOS Vulkan compute pipeline + result-string / memory-type utilities (ext |
| `hosted/` | `wubu_vulkan_loader` | 318 | dlfcn.h, stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_loader.c -- WuBuOS Vulkan: dynamic libvulkan loader, instance, physical-device selection |
| `hosted/` | `wubu_vulkan_swapchain` | 206 | stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_swapchain.c -- WuBuOS Vulkan swapchain + presentation (extracted from the monolithic wub |
| `hosted/` | `wubu_vulkan_util` | 60 | stdint.h, wubu_vulkan.h | wubu_vulkan_util.c -- WuBuOS Vulkan shared utilities Standalone helpers used across the Vulkan backe |
| `jit/` | `jit` | 770 | dlfcn.h, jit.h, jit_internal.h, limits.h, mman.h, stdarg.h, stdio.h, stdlib.h, s | jit.c  --  My Seed JIT Runtime Implementation (mmap backend) The always-available, zero-dependency J |
| `jit/` | `jit_encode` | 66 | jit_internal.h, string.h | jit_encode.c -- x86-64 opcode encoding helpers (self-contained). enc_* : emit raw x86-64 opcodes int |
| `jit/` | `jit_minic` | 701 | ctype.h, jit.h, jit_internal.h, stdio.h, stdlib.h, string.h, wubu_x86.h | jit_minic.c  --  WuBuOS Mini C-to-x86-64 Compiler Self-hosted replacement for the MIR JIT backend. P |
| `jit/` | `jit_minic_token` | 99 | ctype.h, jit_internal.h, stdlib.h, string.h | jit_minic_token.c -- Mini-C tokenizer (self-contained lexer). minic_lex_next/init/cur/advance/expect |
| `jit/` | `jit_test` | 904 | assert.h, jit.h, stdio.h, stdlib.h, string.h, wubu_disasm.h, wubu_x86.h, x86_reg | jit_test.c  --  WuBuOS JIT Test Suite Tests: mmap backend, x86-64 encoder roundtrip, trivial disasse |
| `jit/` | `wubu_disasm` | 433 | stdio.h, string.h, wubu_disasm.h, wubu_x86.h | wubu_disasm.c  --  WuBuOS x86-64 Trivial Disassembler Decodes the x86-64 subset emitted by wubu_x86. |
| `jit/` | `wubu_x86` | 499 | stdlib.h, string.h, wubu_x86.h | wubu_x86.c  --  WuBuOS x86-64 Machine Code Encoder Pure C, zero-dependency x86-64 instruction emitte |
| `jit/` | `x86_regalloc` | 198 | string.h, x86_regalloc.h | x86_regalloc.c  --  WuBuOS x86-64 Mini Register Allocator Linear-scan allocator for JIT-compiled fun |
| `kernel/` | `ahci` | 304 | ahci.h, stdio.h, stdlib.h, string.h | ahci.c  --  WuBuOS AHCI (SATA) Disk Driver Implementation Cell 072: AHCI driver with hosted simulati |
| `kernel/` | `ahci_test` | 401 | ahci.h, stdio.h, stdlib.h, string.h | ahci_test.c  --  Test Suite for WuBuOS AHCI (SATA) Disk Driver Cell 072: Tests HBA init, port enumer |
| `kernel/` | `fat32` | 36 | fat32_internal.h, stdlib.h, string.h, time.h | fat32.c  --  WuBuOS FAT32 Filesystem (facade) This file is the public entry point only. The real wor |
| `kernel/` | `fat32_cluster` | 94 | fat32.h, fat32_internal.h, stdint.h, stdlib.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `fat32_dir` | 335 | fat32_internal.h, stdlib.h, string.h, time.h | fat32_dir.c -- directory enumeration, lookup, create, delete (leaf module). Opaque fat32_volume via  |
| `kernel/` | `fat32_fat` | 64 | fat32_internal.h, stdlib.h, string.h | fat32_fat.c -- FAT entry read/write + cache (leaf module). Opaque fat32_volume via fat32_internal.h. |
| `kernel/` | `fat32_file` | 264 | fat32_internal.h, stdlib.h, string.h, time.h | fat32_file.c -- open/close/read/write/seek on a FAT32 file (leaf module). Opaque fat32_volume via fa |
| `kernel/` | `fat32_format` | 210 | fat32_internal.h, stdlib.h, string.h, time.h | fat32_format.c -- mount/unmount/format/validate (volume lifecycle, leaf module). Opaque fat32_volume |
| `kernel/` | `fat32_name` | 80 | ctype.h, fat32.h, fat32_internal.h, stdlib.h, string.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `fat32_test` | 714 | assert.h, fat32.h, fat32_internal.h, stdio.h, stdlib.h, string.h | fat32_test.c  --  My Seed FAT32 Filesystem Test Suite Uses a RAM-backed block device for fast, deter |
| `kernel/` | `input` | 129 | input.h, string.h | input.c  --  My Seed Input Subsystem (hosted stub) Circular buffers for keyboard/mouse events with p |
| `kernel/` | `input_test` | 278 | input.h, stdio.h, string.h | input_test.c  --  Kernel Input Subsystem Test Suite Cell 202: Tests for input queue (keyboard/mouse  |
| `kernel/` | `interrupt` | 624 | interrupt.h, interrupt_apic.h, interrupt_io.h, interrupt_pic.h, memory.h, signal | interrupt.c  --  My Seed IDT/PIC Interrupt Controller Full x86_64 IDT implementation with 256 interr |
| `kernel/` | `interrupt_apic` | 200 | interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, signal.h, stdint.h, str | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `interrupt_pic` | 107 | interrupt_apic.h, interrupt_io.h, interrupt_pic.h, memory.h, stdint.h | interrupt_pic.c -- 8259 PIC layer + IRQ routing for the WuBuOS kernel. Extracted from the monolithic |
| `kernel/` | `interrupt_pic_test` | 55 | interrupt_apic.h, interrupt_pic.h, stdio.h, string.h | interrupt_pic_test.c -- unit test for the extracted PIC + IRQ routing module (interrupt_pic.c). Buil |
| `kernel/` | `interrupt_pit` | 57 | interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, signal.h, stdint.h, str | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `interrupt_syscall` | 41 | interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, signal.h, stdint.h, str | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `interrupt_timer` | 45 | interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, signal.h, stdint.h, str | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `kernel/` | `klog` | 125 | klog.h, stdarg.h, stddef.h, stdint.h | klog.c -- WuBuOS bare-metal kernel log sink (serial COM1) Self-contained freestanding output. Writes |
| `kernel/` | `libc` | 589 | klog.h, stdarg.h, stddef.h, stdint.h | Minimal libc for bare-metal kernel |
| `kernel/` | `memory` | 571 | memory.h, stdio.h, stdlib.h, string.h | memory.c  --  My Seed Kernel Memory Subsystem Implementation Clean C11 reimplementation of ZealOS he |
| `kernel/` | `memory_test` | 293 | assert.h, memory.h, stdio.h, stdlib.h, string.h | memory_test.c  --  Test suite for My Seed Kernel Memory Subsystem |
| `kernel/` | `metal_main` | 354 | input.h, interrupt.h, interrupt_apic.h, klog.h, memory.h, ps2.h, stdint.h, taski | metal_main.c  --  WuBuOS Bare-Metal Kernel Entry Point Called from crt0.S after Limine/Stivale2 boot |
| `kernel/` | `ps2` | 224 | input.h, interrupt.h, ps2.h, stdint.h | ps2.c  --  PS/2 Keyboard and Mouse Driver (Bare Metal) Ported from Mythos Fable (filipvabrousek/osde |
| `kernel/` | `tasking` | 519 | interrupt.h, libc.h, memory.h, setjmp.h, stddef.h, stdint.h, string.h, tasking.h | tasking.c  --  My Seed Kernel Task Management (hosted test impl) Uses setjmp/longjmp for context swi |
| `kernel/` | `tasking_test` | 181 | assert.h, memory.h, stdio.h, stdlib.h, string.h, tasking.h | tasking_test.c  --  Test suite for My Seed Tasking Subsystem |
| `kernel/` | `test_agi_kernel` | 195 | stdio.h, string.h, wubu_agi_kernel.h, wubu_attest.h, wubu_bonzi.h | test_agi_kernel.c -- Verify the WuBuOS bare-metal AGI kernel supervisor runs correctly (hosted harne |
| `kernel/` | `test_agi_kernel_stub` | 59 | input.h, klog.h, stdio.h, tasking.h, vbe.h | test_agi_kernel_stub.c -- Minimal kernel-API shims so wubu_agi_kernel.c links + runs in the HOSTED u |
| `kernel/` | `test_hive` | 183 | stdio.h, stdlib.h, string.h, wubu_hive.h | test_hive.c -- wubu_hive (C11 luddite hive) unit tests. Verifies the three-way tradeoff the hand-dra |
| `kernel/` | `test_theme_hid` | 92 | assert.h, stddef.h, stdint.h, stdio.h, string.h, wubu_hid.c, wubu_hid.h, wubu_th | test_theme_hid.c -- host tests for the /theme namespace + unified HID. Builds the two freestanding k |
| `kernel/` | `test_verifier` | 65 | stdbool.h, stdint.h, stdio.h, wubu_verifier.c, wubu_verifier.h | test_verifier.c -- host tests for the DA-3 independent verifier. Builds wubu_verifier.c with a minim |
| `kernel/` | `txfs` | 345 | stdio.h, stdlib.h, string.h, txfs.h | txfs.c  --  WuBuOS Transactional Filesystem Layer Implementation Cell 100: Journal-based atomic file |
| `kernel/` | `txfs_test` | 572 | stdio.h, stdlib.h, string.h, txfs.h | txfs_test.c  --  Test Suite for WuBuOS Transactional Filesystem Cell 100: Tests journal-based atomic |
| `kernel/` | `vbe` | 591 | klog.h, math.h, memory.h, stdbool.h, stdio.h, stdlib.h, string.h, vbe.h | vbe.c  --  WuBuOS VBE Framebuffer Implementation Two modes: - Kernel mode (default): uses mem_alloc/ |
| `kernel/` | `wubu_agi_kernel` | 393 | klog.h, string.h, tasking.h, vbe.h, wubu_agi_kernel.h, wubu_attest.h, wubu_bonzi | wubu_agi_kernel.c -- WuBuOS Bare-Metal AGI Kernel Supervisor (ring-0). Freestanding C11: NO malloc,  |
| `kernel/` | `wubu_apic` | 106 | interrupt.h, interrupt_apic.h, klog.h, stdint.h, wubu_apic.h | wubu_apic.c -- local APIC + I/O APIC bring-up (q35-correct delivery). Steps (see wubu_apic.h for the |
| `kernel/` | `wubu_attest` | 103 | string.h, wubu_attest.h | wubu_attest.c -- WuBuOS kernel-side firmware attestation consumer (ring-0). Freestanding C11: no mal |
| `kernel/` | `wubu_bonzi` | 361 | input.h, klog.h, stdio.h, string.h, tasking.h, vbe.h, wubu_agi_kernel.h, wubu_at | wubu_bonzi.c -- Bonzi Buddy: bare-metal AGI agent persona (ring-0 task). Freestanding C11. Runs as a |
| `kernel/` | `wubu_console` | 276 | klog.h, libc.h, memory.h, stdint.h, string.h, tasking.h, wubu_agi_kernel.h, wubu | wubu_console.c -- live ring-0 console REPL (TempleOS-style). The metal kernel owns a COM1 interactiv |
| `kernel/` | `wubu_gaad` | 595 | math.h, stdlib.h, string.h, wubu_gaad.h, wubu_math.h | wubu_gaad.c  --  WuBuOS Golden Aspect Adaptive Decomposition Cell 393: GAAD  --  the universal resol |
| `kernel/` | `wubu_gaad_test` | 269 | assert.h, stdio.h, stdlib.h, string.h, wubu_gaad.h | wubu_gaad_test.c  --  GAAD: Golden Aspect Adaptive Decomposition Cell 393: Tests for the universal r |
| `kernel/` | `wubu_hid` | 146 | wubu_hid.h | wubu_hid.c  --  WuBuOS Unified HID Layer (GameInput-style) A single ring of unified events, common t |
| `kernel/` | `wubu_hive` | 211 | string.h, wubu_hive.h | wubu_hive.c -- C11 "luddite hive" (see wubu_hive.h for the design). Linked fixed-capacity blocks + b |
| `kernel/` | `wubu_math` | 581 | math.h, stddef.h, stdint.h, stdio.h | wubu_math.c  --  WuBuOS Pure C Math Library Cell 420: Pure C implementations replacing libm. IEEE 75 |
| `kernel/` | `wubu_pci` | 92 | libc.h, stdint.h, wubu_pci.h | wubu_pci.c -- minimal PCI config-space access (0xCF8/0xCFC). The metal kernel previously had no PCI  |
| `kernel/` | `wubu_theme` | 218 | stddef.h, stdio.h, wubu_theme.h | wubu_theme.c  --  WuBuOS Metal Theme Engine + /theme Namespace The graphic set as a writable node tr |
| `kernel/` | `wubu_verifier` | 107 | wubu_agi_kernel.h, wubu_verifier.h | wubu_verifier.c  --  WuBuOS Independent Verifier (DA-3 promotion gate) Deterministic, kernel-residen |
| `runtime/` | `wubucontainer` | 697 | errno.h, fcntl.h, inet.h, json.h, socket.h, stat.h, stdio.h, stdlib.h, string.h, | wubucontainer.c  --  WuBuContainer Conversion Toolkit C Implementation Implements the C-side interfa |
| `runtime/` | `wubucontainer_registry` | 51 | string.h, wubucontainer_internal.h | wubucontainer_registry.c -- In-memory handler registry for the WuBuContainer agentic layer. This is  |
| `runtime/` | `wubucontainer_test` | 52 | assert.h, stdio.h, string.h, wubucontainer.h | wubucontainer_test.c -- Regression test for wubu_container_register_handler. Verifies the handler re |
| `runtime/` | `ct_iso_cgroup` | 28 | ct_iso_cgroup.h, errno.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, types.h, | ct_iso_cgroup.c  --  WuBuOS container cgroups v2 write helper (Cell 420 split). The cgroup create/se |
| `runtime/` | `ct_iso_ns` | 13 | ct_iso_ns.h, sched.h | ct_iso_ns.c  --  WuBuOS container namespace unshare helper (Cell 420 split). |
| `runtime/` | `ct_iso_seccomp` | 550 | audit.h, ct_iso_ns.h, ct_iso_seccomp.h, errno.h, filter.h, prctl.h, seccomp.h, s | ct_iso_seccomp.c  --  WuBuOS container seccomp-BPF syscall filtering (Cell 420 split). Allowlist app |
| `runtime/` | `edr_core` | 571 | edr_internal.h, sched.h, stdatomic.h | edr_core.c  --  WuBuOS EDR Engine Core Lifecycle, module system, alert buffer, FNV-1a hashing, lock- |
| `runtime/` | `edr_fanotify` | 245 | edr_internal.h, fanotify.h, inotify.h | edr_fanotify.c  --  WuBuOS EDR File Telemetry Pins Two kernel telemetry sources for file and configu |
| `runtime/` | `edr_poller` | 325 | edr_internal.h | edr_poller.c  --  WuBuOS EDR Periodic Poller Periodic /proc scans for telemetry that doesn't have a  |
| `runtime/` | `edr_proc_pin` | 202 | cn_proc.h, connector.h, edr_internal.h | edr_proc_pin.c  --  WuBuOS EDR Process Pin NETLINK_CONNECTOR + cn_proc — the Linux equivalent of Win |
| `runtime/` | `oci_blob_store` | 79 | oci_internal.h | oci_blob_store.c  --  OCI Content-Addressable Blob Storage Extracted from wubu_oci.c (lines 983-1053 |
| `runtime/` | `oci_cleanup` | 147 | oci_internal.h | oci_cleanup.c  --  OCI Layer Cleanup and Garbage Collection Extracted from wubu_oci.c (lines 1634-17 |
| `runtime/` | `oci_convert` | 261 | oci_internal.h | oci_convert.c  --  Convert between .wubu and OCI formats Extracted from wubu_oci.c (lines 1055-1307) |
| `runtime/` | `oci_descriptor` | 19 | oci_internal.h | oci_descriptor.c  --  OCI Descriptor Operations Extracted from wubu_oci.c (lines 506-516). |
| `runtime/` | `oci_hooks` | 28 | oci_internal.h | oci_hooks.c  --  OCI Runtime Hook Operations Extracted from wubu_oci.c (lines 1614-1632). |
| `runtime/` | `oci_http_client` | 368 | oci_internal.h | oci_http_client.c  --  HTTP/TLS Client for OCI Registry Operations Extracted from wubu_oci.c (lines  |
| `runtime/` | `oci_image_config` | 256 | oci_internal.h | oci_image_config.c  --  OCI Image Config Operations Extracted from wubu_oci.c (lines 520-768). |
| `runtime/` | `oci_image_index` | 110 | oci_internal.h | oci_image_index.c  --  OCI Image Index (Multi-arch) Operations Extracted from wubu_oci.c (lines 878- |
| `runtime/` | `oci_image_manifest` | 115 | oci_internal.h | oci_image_manifest.c  --  OCI Image Manifest Operations Extracted from wubu_oci.c (lines 770-876). |
| `runtime/` | `oci_media_types` | 18 | oci_internal.h | oci_media_types.c  --  OCI Media Type String Helpers Extracted from wubu_oci.c (lines 495-505). |
| `runtime/` | `oci_registry` | 216 | oci_internal.h | oci_registry.c  --  OCI Registry Client Operations Extracted from wubu_oci.c (lines 1309-1516). |
| `runtime/` | `oci_runtime_spec` | 87 | oci_internal.h | oci_runtime_spec.c  --  OCI Runtime Spec Operations Extracted from wubu_oci.c (lines 1518-1612). |
| `runtime/` | `styx_enc` | 108 | string.h, styx.h, styx_internal.h | styx_enc.c -- Styx/9P2000 client request encoders + Rerror builder. Self-contained: every function h |
| `runtime/` | `styx_fid` | 33 | string.h, styx.h | styx_fid.c -- Styx fid (file-identifier) management subsystem. Self-contained: styx_fid_alloc / styx |
| `runtime/` | `styx_names` | 35 | string.h, styx.h | styx_names.c -- Styx/9P2000 message-name table + lookup. Self-contained: only depends on styx.h for  |
| `runtime/` | `styx_parse` | 95 | string.h, styx.h | styx_parse.c -- Styx/9P2000 client-side response/send parsers. Self-contained: each function validat |
| `runtime/` | `styx_serve` | 464 | string.h, styx.h, styx_internal.h | styx_serve.c -- Styx/9P2000 server: init + inbound dispatch + R-message response builders. Self-cont |
| `runtime/` | `styx_test` | 636 | stdio.h, stdlib.h, string.h, styx.h | styx_test.c  --  Styx/9P2000 Protocol Test Suite Tests: message building, parsing, server dispatch,  |
| `runtime/` | `styxfs_callbacks` | 382 | dirent.h, errno.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, styxfs.h, styxf | the /n control plane needs -- the namespace bridge writes real files |
| `runtime/` | `styxfs_host` | 117 | stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_internal.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `runtime/` | `styxfs_path` | 50 | stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_internal.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `runtime/` | `styxfs_posix` | 250 | dirent.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_interna | styxfs_posix.c -- StyxFS POSIX-like file API + global server instance. Extracted from the monolithic |
| `runtime/` | `styxfs_server` | 470 | dirent.h, errno.h, fcntl.h, libgen.h, stat.h, stdio.h, stdlib.h, string.h, styx. | styxfs_server.c  --  Styx/9P File Server Implementation A concrete 9P2000 file server that exports a |
| `runtime/` | `styxfs_test` | 257 | stat.h, stdio.h, stdlib.h, string.h, styxfs.h, unistd.h | styxfs_test.c  --  WuBuOS StyxFS Test Suite (Cell 106) Tests StyxFS mount/unmount, file namespace, . |
| `runtime/` | `styxfs_util` | 104 | dirent.h, stdio.h, styxfs_internal.h, time.h, unistd.h | styxfs_util.c -- StyxFS utility subsystem (mount/file resolution, container load). Self-contained: u |
| `runtime/` | `styxfs_vfs` | 270 | dirent.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, styxfs.h, styxfs_interna | styxfs_vfs.c -- StyxFS in-memory VFS tree + open-file table. Extracted from the monolithic styxfs.c. |
| `runtime/` | `wubu_vsl_gpu` | 478 | drm.h, drm_fourcc.h, drm_mode.h, errno.h, fcntl.h, ioctl.h, mman.h, stdio.h, std |  |
| `runtime/` | `test_gpu` | 119 | stdio.h, stdlib.h, unistd.h, wubu_vsl_gpu.h | VSL GPU Test - Validates DRM/KMS backend on WSL |
| `runtime/` | `test_vulkan` | 73 | stdio.h, stdlib.h, wubu_vsl_vulkan.h | VSL Vulkan Test - Validates backend selection and device enumeration Run on WSL to test Venus/Linux  |
| `runtime/` | `wubu_vsl_vulkan` | 584 | dirent.h, dlfcn.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, vulka | VSL Vulkan Implementation - WuBuOS Vulkan Abstraction Backend selection: 1. WSL/VM: VirtIO-GPU Venus |
| `runtime/` | `vsl` | 183 | bpf.h, dlfcn.h, epoll.h, errno.h, eventfd.h, fanotify.h, fcntl.h, if.h, if_tun.h | vsl.c  --  VSL Core Lifecycle & Diagnostics This file contains the main VSL state and lifecycle func |
| `runtime/` | `vsl_driver` | 279 | cuda.h, dlfcn.h, errno.h, fcntl.h, if.h, if_tun.h, ioctl.h, stdio.h, stdlib.h, s | vsl_driver.c  --  VSL Driver Management Implementation |
| `runtime/` | `vsl_elf` | 198 | mman.h, stdint.h, stdio.h, stdlib.h, string.h, vsl_elf.h, vsl_internal.h | vsl_elf.c  --  VSL ELF Loading Implementation |
| `runtime/` | `vsl_file` | 137 | errno.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, vsl_file.h, vsl | vsl_file.c  --  VSL File Operations Implementation |
| `runtime/` | `vsl_gpu_vulkan` | 644 | dlfcn.h, fcntl.h, stdbool.h, stdio.h, stdlib.h, string.h, unistd.h, vsl_driver.h | vsl_gpu_vulkan.c  --  VSL Vulkan GPU Driver Implementation Implements VSL_DRV_GPU_VULKAN driver for  |
| `runtime/` | `vsl_macho` | 319 | errno.h, mman.h, stdio.h, stdlib.h, string.h, unistd.h, vsl_internal.h, vsl_mach | vsl_macho.c  --  VSL Mach-O Binary Loader Implementation Validates and loads macOS Mach-O binaries i |
| `runtime/` | `vsl_macho_test` | 231 | assert.h, stdio.h, stdlib.h, string.h, vsl_macho.h | vsl_macho_test.c  --  VSL Mach-O Loader Tests Tests for Mach-O binary validation, FAT binary extract |
| `runtime/` | `vsl_memory` | 102 | stdint.h, stdlib.h, string.h, vsl_internal.h, vsl_memory.h, vsl_process.h | vsl_memory.c  --  VSL Memory Management Implementation |
| `runtime/` | `vsl_nt_alpc` | 435 | errno.h, fcntl.h, socket.h, stdint.h, stdlib.h, string.h, un.h, unistd.h, vsl_nt | vsl_nt_alpc.c -- Windows 11 ALPC (Advanced Local Procedure Call) syscalls. ALPC is the high-performa |
| `runtime/` | `vsl_nt_atoms` | 296 | vsl_nt_internal.h | Shared atom-name lookup used by add/find/delete/query. |
| `runtime/` | `vsl_nt_enclave` | 147 | errno.h, memfd.h, mman.h, stdint.h, stdlib.h, string.h, unistd.h, vsl_nt_bridge. | vsl_nt_enclave.c -- Windows 11 Enclave (VBS/SGX) syscalls. Enclaves are secure memory regions for VB |
| `runtime/` | `vsl_nt_io` | 972 | vsl_nt_internal.h | vsl_nt_io.c -- NT transliteration Batch 3: file I/O + events + delay. Real VSL/Linux work; part of t |
| `runtime/` | `vsl_nt_ioring` | 165 | errno.h, fcntl.h, io_uring.h, mman.h, stdint.h, stdlib.h, string.h, syscall.h, u | vsl_nt_ioring.c -- Windows 11 IoRing syscalls. IoRing is the modern async I/O ring buffer mechanism  |
| `runtime/` | `vsl_nt_job` | 228 | vsl_nt_internal.h | vsl_nt_job.c -- NT transliteration Batch 2: job objects. Real VSL/Linux work; part of the E1 NT-brid |
| `runtime/` | `vsl_nt_ktm` | 624 | errno.h, fcntl.h, stat.h, stdint.h, stdlib.h, string.h, types.h, unistd.h, vsl_n | vsl_nt_ktm.c -- Windows 11 KTM (Kernel Transaction Manager) syscalls. KTM provides transactional sup |
| `runtime/` | `vsl_nt_misc` | 979 | poll.h, ptrace.h, resource.h, signal.h, sysinfo.h, times.h, utsname.h, vsl_nt_in | vsl_nt_misc.c -- WuBuOS NT transliteration: miscellaneous syscalls (blitz). Implements remaining Rea |
| `runtime/` | `vsl_nt_misc_w11` | 770 | errno.h, stdbool.h, stdint.h, string.h, vsl_nt_bridge.h, vsl_nt_internal.h, wubu | vsl_nt_misc_w11.c  --  Windows 11 (24H2) syscalls not in ReactOS 99 syscalls with real Linux/VSL imp |
| `runtime/` | `vsl_nt_partition` | 178 | errno.h, fcntl.h, stat.h, stdint.h, stdlib.h, string.h, unistd.h, vsl_nt_bridge. | vsl_nt_partition.c -- Windows 11 Partition/CpuPartition syscalls. Partitions are lightweight isolati |
| `runtime/` | `vsl_nt_process` | 194 | vsl_nt_internal.h | vsl_nt_process.c -- WuBuOS NT transliteration: Process lifecycle. Split from vsl_nt_proc.c (which mi |
| `runtime/` | `vsl_nt_registry` | 556 | utime.h, vsl_nt_internal.h, wubu_fs_util.h | mkdir -p semantics: create every component of `path`, tolerating EEXIST. Returns 0 on success, -1 on |
| `runtime/` | `vsl_nt_section` | 122 | vsl_nt_internal.h | vsl_nt_section.c -- WuBuOS NT transliteration: Section (memory-mapped file) objects. Split from vsl_ |
| `runtime/` | `vsl_nt_sync` | 481 | vsl_nt_internal.h | vsl_nt_sync.c -- NT transliteration Batch 6: thread lifecycle + wait/sync + mutant/semaphore. Real V |
| `runtime/` | `vsl_nt_thread` | 170 | vsl_nt_internal.h | vsl_nt_thread.c -- WuBuOS NT transliteration: Thread lifecycle. Split from vsl_nt_proc.c (which mixe |
| `runtime/` | `vsl_nt_timer` | 80 | vsl_nt_internal.h | vsl_nt_timer.c -- WuBuOS NT transliteration: Timer objects. Split from vsl_nt_proc.c (which mixed 5  |
| `runtime/` | `vsl_nt_token` | 568 | vsl_nt_internal.h | vsl_nt_token.c -- WuBuOS NT transliteration: Token / Security subsystem. Real privilege enforcement  |
| `runtime/` | `vsl_nt_vmem` | 206 | vsl_nt_internal.h | vsl_nt_vmem.c -- WuBuOS NT transliteration: Virtual Memory. Split from vsl_nt_proc.c (which mixed 5  |
| `runtime/` | `vsl_nt_wnf` | 306 | errno.h, fcntl.h, inotify.h, stat.h, stdint.h, stdlib.h, string.h, unistd.h, vsl | vsl_nt_wnf.c -- Windows 11 WNF (Windows Notification Facility) syscalls. WNF is state-name based, sc |
| `runtime/` | `vsl_nt_worker` | 205 | errno.h, pthread.h, stdatomic.h, stdint.h, stdlib.h, string.h, unistd.h, vsl_nt_ | vsl_nt_worker.c -- Windows 11 Worker Factory syscalls. Worker factories are kernel-managed thread po |
| `runtime/` | `vsl_process` | 165 | errno.h, mman.h, signal.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, vsl_e | vsl_process.c  --  VSL Process Management Implementation |
| `runtime/` | `vsl_shared` | 34 | stdio.h, stdlib.h, string.h, vsl_internal.h, vsl_shared.h | vsl_shared.c  --  VSL Shared Memory Implementation |
| `runtime/` | `vsl_syscall` | 281 | vsl_syscall_internal.h | vsl_syscall.c  --  VSL Syscall Bridge Facade Table-driven dispatch + helper functions. Handler imple |
| `runtime/` | `vsl_syscall_cpm` | 350 | vsl_syscall_cpm_internal.h | vsl_syscall_cpm.c  --  VSL CP/M BDOS Syscall Personality (Core) Table-driven BDOS dispatch. Every ha |
| `runtime/` | `vsl_syscall_cpm_test` | 130 | stat.h, stdio.h, stdlib.h, string.h, unistd.h, vsl_syscall_cpm_internal.h, vsl_s | vsl_syscall_cpm_test.c  --  CP/M BDOS personality regression test. Builds FCBs + a DMA buffer in pro |
| `runtime/` | `vsl_syscall_fileio` | 395 | vsl_syscall_internal.h | vsl_syscall_fileio.c  --  VSL File I/O Syscalls Read, write, open, close, lseek, stat, fstat, etc. |
| `runtime/` | `vsl_syscall_mac` | 53 | stdio.h, vsl_syscall_mac_internal.h | vsl_syscall_mac.c  --  VSL macOS (XNU) Syscall Dispatch (Core) Central entry point for macOS syscall |
| `runtime/` | `vsl_syscall_mac_bsd` | 868 | vsl_syscall_mac_internal.h | BSD SYSCALL HANDLERS Most BSD syscalls map directly to Linux syscalls. |
| `runtime/` | `vsl_syscall_mac_mach` | 234 | vsl_syscall_mac_internal.h | vsl_syscall_mac_mach.c  --  VSL macOS Mach Trap Handlers Mach trap handlers, port table, mach_msg, a |
| `runtime/` | `vsl_syscall_mac_test` | 229 | stat.h, stdbool.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, vsl_mach_ipc. | vsl_syscall_mac_test.c  --  macOS Syscall Dispatch Tests Tests for the VSL macOS syscall dispatch ta |
| `runtime/` | `vsl_syscall_macclassic` | 193 | vsl_syscall_macclassic_internal.h | vsl_syscall_macclassic.c  --  VSL Classic Mac OS (68K) Trap Personality (Core) Table-driven A-line t |
| `runtime/` | `vsl_syscall_macclassic_test` | 130 | stdint.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, vsl_syscall_internal.h, | vsl_syscall_macclassic_test.c  --  Classic Mac OS (68K A-line) personality test. Drives trap words t |
| `runtime/` | `vsl_syscall_memory` | 113 | shm.h, vsl_syscall_internal.h | vsl_syscall_memory.c  --  VSL Memory Management Syscalls mmap, munmap, brk, mprotect, msync, mremap, |
| `runtime/` | `vsl_syscall_net` | 581 | vsl_syscall_internal.h | vsl_syscall_net.c  --  VSL Socket, Signal, Timer, Namespace & Misc Syscalls Socket family, rt_sigact |
| `runtime/` | `vsl_syscall_nt` | 349 | errno.h, eventfd.h, fcntl.h, futex.h, limits.h, mman.h, pthread.h, semaphore.h,  | vsl_syscall_nt.c -- ReactOS NT syscall -> VSL transliteration layer (E1). FACADE of a decomposed dis |
| `runtime/` | `vsl_syscall_nt_ext_test` | 133 | errno.h, stat.h, stdint.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, vsl_n | vsl_syscall_nt_ext_test.c -- Behavioral regression test for the NT 6.1/W11 extension personalities ( |
| `runtime/` | `vsl_syscall_nt_test` | 1542 | assert.h, errno.h, eventfd.h, fcntl.h, futex.h, mman.h, stdint.h, stdio.h, strin | vsl_syscall_nt_test.c -- Regression test for E1: ReactOS NT syscall transliteration (first 10 syscal |
| `runtime/` | `vsl_syscall_proc` | 424 | sched.h, vsl_syscall_internal.h | vsl_syscall_proc.c  --  VSL Process Management + Identity/Credential Syscalls Fork, clone, exec, wai |
| `runtime/` | `wubu_anticheat` | 366 | errno.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, syscall.h, unistd.h, wubu | we CAN do in userspace: |
| `runtime/` | `wubu_anticheat_test` | 238 | errno.h, stdio.h, stdlib.h, string.h, wubu_anticheat.h | wubu_anticheat_test.c  --  Tests for Anti-Cheat Module Cell 470: Anti-cheat research and stubs. |
| `runtime/` | `wubu_apps_test` | 258 | errno.h, stdio.h, stdlib.h, string.h, wubu_pkg.h, wubu_proton.h, wubu_vsl.h | wubu_apps_test.c  --  WuBuOS App-Level Components Test Suite Cell 107: Package manager (Flatpak-styl |
| `runtime/` | `wubu_arch` | 445 | errno.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h, wubu_ar | wubu_arch.c  --  WuBuOS Arch Linux Bootstrap for Container Roots Cell 390: Arch as the NT-era kernel |
| `runtime/` | `wubu_arch_test` | 146 | assert.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_arch.h | wubu_arch_test.c  --  Tests for Arch Bootstrap and FreeDoom Launcher Cell 390/391: Arch root managem |
| `runtime/` | `wubu_archd_daemon` | 385 | dirent.h, epoll.h, errno.h, fcntl.h, ftw.h, libgen.h, signal.h, socket.h, stat.h | wubu_archd_daemon.c -- WuBuOS archd daemon lifecycle + main entry. Extracted from the monolithic wub |
| `runtime/` | `wubu_archd_fs` | 108 | errno.h, stdio.h, stdlib.h, string.h, wubu_archd.h, wubu_archd_internal.h, wubu_ | wubu_archd_fs.c -- WuBuOS archd: recursive filesystem delete. Extracted from wubu_archd.c (separable |
| `runtime/` | `wubu_archd_loop` | 144 | epoll.h, socket.h, string.h, unistd.h, wubu_archd_internal.h | wubu_archd_loop.c -- WuBuOS archd daemon: main event loop Self-contained request-dispatch concern sp |
| `runtime/` | `wubu_archd_svc` | 433 | dirent.h, epoll.h, errno.h, fcntl.h, signal.h, socket.h, stat.h, stdarg.h, stdio | wubu_archd_svc.c -- WuBuOS archd pkg/repo/svc/aur/hook/health/gpu mgmt. Extracted from the monolithi |
| `runtime/` | `wubu_archd_svc_super` | 484 | errno.h, signal.h, stat.h, stdio.h, stdlib.h, string.h, time.h, types.h, unistd. | wubu_archd_svc_super.c -- Real in-process service supervisor. Closes N1 (popen arch-chroot systemctl |
| `runtime/` | `wubu_archd_test` | 215 | assert.h, stat.h, stdio.h, string.h, unistd.h, wubu_archd.h | wubu_archd_test.c  --  Test suite for wubu_archd (Arch Linux Daemon) Tests daemon init, root lifecyc |
| `runtime/` | `wubu_archd_util` | 74 | wubu_archd_internal.h | wubu_archd_util.c -- Process / filesystem helper utilities for the Arch daemon. Self-contained: run_ |
| `runtime/` | `wubu_bottle_flatpak` | 118 | ctype.h, dirent.h, errno.h, ftw.h, stat.h, stdio.h, stdlib.h, string.h, time.h,  | wubu_bottle_flatpak.c -- WuBuOS Bottles/Lutris: Flatpak manifest + runtime detection. Split from wub |
| `runtime/` | `wubu_bottle_io` | 143 | ctype.h, dirent.h, errno.h, ftw.h, stat.h, stdio.h, stdlib.h, string.h, time.h,  | wubu_bottle_io.c -- WuBuOS Bottles/Lutris: Bottles + Lutris format import/export. Split from wubu_bo |
| `runtime/` | `wubu_bottle_lifecycle` | 348 | ctype.h, dirent.h, errno.h, ftw.h, stat.h, stdio.h, stdlib.h, string.h, time.h,  | wubu_bottle_lifecycle.c -- WuBuOS Bottles/Lutris: Bottle lifecycle + install/run. Split from wubu_bo |
| `runtime/` | `wubu_bottle_ops` | 102 | ctype.h, dirent.h, errno.h, ftw.h, stat.h, stdio.h, stdlib.h, string.h, time.h,  | wubu_bottle_ops.c -- WuBuOS Bottles/Lutris: List + verify installed bottles. Split from wubu_bottles |
| `runtime/` | `wubu_bottle_serialize` | 159 | stdio.h, wubu_bottles_internal.h | wubu_bottle_serialize.c -- WuBuOS Bottles: JSON (de)serialization Self-contained serialization conce |
| `runtime/` | `wubu_bottles_fs` | 20 | errno.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_bottles.h, wubu_bottles_int | wubu_bottles_fs.c -- WuBuOS bottles: recursive filesystem delete. Extracted from wubu_bottles.c (sep |
| `runtime/` | `wubu_bottles_json` | 46 | wubu_bottles_internal.h | wubu_bottles_json.c -- Bottle JSON parsing helpers (self-contained). Pure JSON literal extraction: j |
| `runtime/` | `wubu_bottles_test` | 302 | stat.h, stdio.h, stdlib.h, string.h, types.h, wubu_bottles.h | wubu_bottles_test.c  --  Tests for Bottles/Lutris Integration Cell 480: Bottles and Lutris compatibi |
| `runtime/` | `wubu_cap_handle` | 173 | pthread.h, stdint.h, stdlib.h, string.h, wubu_cap_internal.h | wubu_cap_handle.c -- WuBuOS per-process capability handle table. Userspace (or a WuBuOS agent proces |
| `runtime/` | `wubu_cap_object` | 279 | pthread.h, stdint.h, stdlib.h, string.h, wubu_cap_internal.h | wubu_cap_object.c -- WuBuOS capability core: registry + lifecycle + resolver. Ported from GrahaOS ke |
| `runtime/` | `wubu_cap_revoke` | 61 | stddef.h, stdint.h, stdlib.h, wubu_cap_internal.h | wubu_cap_revoke.c -- WuBuOS capability core revocation cascade. Ported from GrahaOS kernel/cap/revok |
| `runtime/` | `wubu_cap_system` | 65 | string.h, wubu_cap_internal.h | wubu_cap_system.c -- WuBuOS bootcap cascade root (extends wubu_cap). Adopted from GrahaOS kernel/cap |
| `runtime/` | `wubu_cap_test` | 151 | assert.h, stdio.h, string.h, wubu_cap.h, wubu_cap_internal.h | wubu_cap_test.c -- WuBuOS capability core self-test. Exercises the real engine (not stubs): create a |
| `runtime/` | `wubu_cap_token` | 47 | stddef.h, wubu_cap_internal.h | wubu_cap_token.c -- WuBuOS capability core token slow-path helpers. Ported from GrahaOS kernel/cap/t |
| `runtime/` | `wubu_compat_db` | 166 | errno.h, stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_compat_db. | wubu_compat_db.c -- WuBuOS per-app Windows-compat database (SteamOS ProtonDB + shader-cache lesson). |
| `runtime/` | `wubu_compat_db_test` | 71 | stdio.h, stdlib.h, string.h, unistd.h, wubu_compat_db.h | wubu_compat_db_test.c -- WuBuOS per-app compat DB tests (SteamOS ProtonDB + shader-cache lesson). Re |
| `runtime/` | `wubu_container` | 291 | stddef.h, stdio.h, stdlib.h, string.h, wubu_container.h, wubu_crypto.h, wubu_ct_ | wubu_container.c  --  WuBuOS Universal Container Format Implementation .wubu: one extension, infinit |
| `runtime/` | `wubu_container_test` | 377 | holyc.h, stdio.h, stdlib.h, string.h, wubu_container.h, wubu_exec.h, wubu_exec_i | wubu_container_test.c  --  WuBuOS .wubu Container Format Test Suite |
| `runtime/` | `wubu_ct_bwrap` | 184 | errno.h, fcntl.h, limits.h, signal.h, stdio.h, stdlib.h, string.h, types.h, unis | wubu_ct_bwrap.c  --  WuBuOS Bubblewrap Container Runtime (Hosted Demo) Cell 340/391: Container execu |
| `runtime/` | `wubu_ct_isolate` | 96 | audit.h, ct_iso_cgroup.h, ct_iso_ns.h, ct_iso_seccomp.h, errno.h, fcntl.h, filte | wubu_ct_isolate.c  --  WuBuOS Container Isolation (cgroups v2 + seccomp) Cell 420: Security hardenin |
| `runtime/` | `wubu_ct_isolate_cgroup` | 132 | errno.h, fcntl.h, stdint.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_ct_isola | wubu_ct_isolate_cgroup.c -- Container cgroup operations subsystem. Self-contained: cgroup file read/ |
| `runtime/` | `wubu_dos_emu` | 197 | wubu_dos_emu_internal.h | wubu_dos_emu.c -- WuBuOS 8086/DOS shim public API (create/destroy/load/run/capture). |
| `runtime/` | `wubu_dos_emu_alu` | 249 | wubu_dos_emu_internal.h | wubu_dos_emu_alu.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_decode` | 313 | wubu_dos_emu_internal.h | wubu_dos_emu_decode.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_int` | 211 | wubu_dos_emu_internal.h | wubu_dos_emu_int.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_mem` | 30 | wubu_dos_emu_internal.h | wubu_dos_emu_mem.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_regs` | 51 | wubu_dos_emu_internal.h | wubu_dos_emu_regs.c -- WuBuOS 8086/DOS shim leaf module (self-contained C11). |
| `runtime/` | `wubu_dos_emu_smoke` | 49 | stdio.h, string.h, wubu_dos_emu.h | Build a tiny .COM by hand: mov ah, 0x09      ; DOS print '$'-terminated string mov dx, msg       ; o |
| `runtime/` | `wubu_dos_emu_test` | 307 | stdio.h, stdlib.h, string.h, wubu_dos_emu.h | wubu_dos_emu_test.c -- Unit tests for the in-process 8086 + DOS-INT shim. Verifies the real engine:  |
| `runtime/` | `wubu_dos_proc` | 225 | errno.h, stat.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wubu_container.h | wubu_dos_proc.c -- WuBuOS 16-bit DOS process (in-process 8086 compat shim). This is the "compatible  |
| `runtime/` | `wubu_dos_proc_test` | 105 | stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_container.h, wubu_dos_proc.h | wubu_dos_proc_test.c -- Unit test for the in-process 16-bit DOS shim. Builds a real .COM in memory ( |
| `runtime/` | `wubu_dxvk_conf` | 164 | stdio.h, stdlib.h, string.h, wubu_dxvk_conf.h | wubu_dxvk_conf.c -- Shared DXVK config-file core. Implements the config read/write + line set/replac |
| `runtime/` | `wubu_edr` | 17 | edr_internal.h | wubu_edr.c  --  WuBuOS EDR Engine Shim All implementation moved to src/runtime/edr/ subdirectory. Th |
| `runtime/` | `wubu_edr_agent_test` | 105 | assert.h, dosgui_wm.h, stdio.h, string.h, vbe.h, wubu_edr.h, wubu_ui.h | wubu_edr_agent_test.c -- AGI activity -> EDR integration test. This is the proof of the transparency |
| `runtime/` | `wubu_edr_test` | 112 | assert.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wubu_edr.h | wubu_edr_test.c  --  WuBuOS EDR Engine Test Suite |
| `runtime/` | `wubu_exec` | 447 | fcntl.h, holyc.h, mman.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h, | wubu_exec.c  --  WuBuOS Universal Executable Dispatcher Implementation One exec to rule them all. |
| `runtime/` | `wubu_exec_container` | 93 | stdio.h, stdlib.h, string.h, unistd.h, wubu_exec.h, wubu_vsl.h | wubu_exec_container.c -- WuBuOS exec: container-payload execution. Extracted from wubu_exec.c (separ |
| `runtime/` | `wubu_exec_dos` | 76 | stdio.h, stdlib.h, string.h, wubu_dos_proc.h, wubu_exec_internal.h | wubu_exec_dos.c -- WuBuOS exec: DOS 16-bit (.COM/.EXE) backend. Extracted from wubu_exec.c (separabl |
| `runtime/` | `wubu_exec_format` | 64 | wubu_exec_internal.h | wubu_exec_format.c -- Payload format registry + detection (self-contained). wubu_payload_name / wubu |
| `runtime/` | `wubu_exec_macho` | 139 | stat.h, stdbool.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_exec.h, wubu_host | wubu_exec_macho.c -- WuBuOS exec: Mach-O backend (VSL native + Darling fallback). Validates Mach-O m |
| `runtime/` | `wubu_exec_wasm` | 55 | stdio.h, stdlib.h, string.h, unistd.h, wubu_exec.h, wubu_vsl.h | wubu_exec_wasm.c -- WuBuOS exec: WASM backend (wasmtime/wasm3/wasmer). Extracted from wubu_exec.c (s |
| `runtime/` | `wubu_fs_util` | 42 | errno.h, ftw.h, stat.h, stdio.h, stdlib.h, string.h, types.h, wubu_fs_util.h | wubu_fs_util.c -- shared filesystem utilities (dedup home). Canonical implementation of recursive-fo |
| `runtime/` | `wubu_gc` | 202 | stdint.h, stdio.h, stdlib.h, string.h, wubu_gc.h | wubu_gc.c  --  Simple Mark-and-Sweep GC for WuBuOS Userspace Applets Opt-in garbage collector for Ho |
| `runtime/` | `wubu_gc_test` | 201 | stdint.h, stdio.h, string.h, wubu_gc.h | wubu_gc_test.c  --  Userspace GC Test Suite Tests mark-and-sweep for HolyC REPL / container applets. |
| `runtime/` | `wubu_gdpr_age` | 163 | stdio.h, stdlib.h, string.h, time.h, wubu_gdpr_age.h, wubu_uuid.h | wubu_gdpr_age.c — GDPR-compliant age assurance for WuBuOS. Implemented per wubu_gdpr_age.h. No biome |
| `runtime/` | `wubu_hc_eval_stub` | 13 | stdint.h | wubu_hc_eval_stub.c -- Minimal definition of hc_eval for runtime test binaries that link wubu_exec.c |
| `runtime/` | `wubu_holyc_agi` | 76 | stdio.h, stdlib.h, string.h, wubu_edr.h, wubu_holyc_agi.h, wubu_holyd.h | wubu_holyc_agi.c -- Live ring-0 compiler AGI layer (TempleOS "God compiler") See wubu_holyc_agi.h. O |
| `runtime/` | `wubu_holyc_agi_test` | 70 | stdio.h, string.h, wubu_edr.h, wubu_holyc_agi.h, wubu_holyd.h | test_holyc_agi.c -- Live ring-0 compiler AGI layer test. Proves the TempleOS "God compiler" is now C |
| `runtime/` | `wubu_holyd` | 162 | wubu_holyd_internal.h | wubu_holyd.c  --  WuBuOS TempleOS HolyC DOS Daemon (Facade) Manages HolyC DOS sessions: REPL, compil |
| `runtime/` | `wubu_holyd_9p` | 63 | wubu_holyd_internal.h | wubu_holyd_9p.c  --  WuBuOS HolyC DOS Daemon: 9P |
| `runtime/` | `wubu_holyd_event` | 24 | wubu_holyd_internal.h | wubu_holyd_event.c  --  WuBuOS HolyC DOS Daemon: Event |
| `runtime/` | `wubu_holyd_exec` | 229 | ctype.h, stdbool.h, wubu_holyd_internal.h | gives the REPL its persistent state. */ |
| `runtime/` | `wubu_holyd_input` | 102 | wubu_holyd_internal.h | wubu_holyd_input.c  --  WuBuOS HolyC DOS Daemon: Input |
| `runtime/` | `wubu_holyd_lifecycle` | 333 | wubu_holyd_internal.h | wubu_holyd_lifecycle.c  --  WuBuOS HolyC DOS Daemon: Lifecycle |
| `runtime/` | `wubu_holyd_repl` | 242 | stdlib.h, string.h, wubu_holyd_internal.h | wubu_holyd_repl.c -- WuBuOS HolyC DOS Daemon: REPL + macro storage Self-contained REPL concern split |
| `runtime/` | `wubu_holyd_save` | 120 | wubu_holyd_internal.h | wubu_holyd_save.c  --  WuBuOS HolyC DOS Daemon: Save |
| `runtime/` | `wubu_holyd_session` | 123 | wubu_holyd_internal.h | wubu_holyd_session.c  --  WuBuOS HolyC DOS Daemon: Session |
| `runtime/` | `wubu_holyd_test` | 428 | assert.h, jit.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_holyd.h | wubu_holyd_test.c  --  Test suite for wubu_holyd (TempleOS HolyC DOS Daemon) Tests daemon init, sess |
| `runtime/` | `wubu_holyd_window` | 136 | wubu_holyd_internal.h | wubu_holyd_window.c  --  WuBuOS HolyC DOS Daemon: Window |
| `runtime/` | `wubu_host_exec` | 496 | errno.h, fcntl.h, mount.h, prctl.h, resource.h, signal.h, socket.h, stat.h, stdi | wubu_host_exec.c  --  WuBuOS Host Container Execution (Linux) Cell 203: Fork+exec for .wubu containe |
| `runtime/` | `wubu_host_exec_test` | 273 | signal.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_host_exec.h | wubu_host_exec_test.c  --  Cell 203 Behavioral Test Suite Tests: fork+exec container creation, start |
| `runtime/` | `wubu_image` | 559 | dirent.h, fcntl.h, ftw.h, time.h, wait.h, wubu_container.h, wubu_image.h, wubu_i | wubu_image.c  --  WuBuOS Container Image Builder Phase 7: .wubu image builder implementation - WuBuF |
| `runtime/` | `wubu_image_cache` | 61 | fcntl.h, stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_image.h | wubu_image_cache.c -- Layer cache subsystem (self-contained). wubu_layer_cache_get/put/exists: on-di |
| `runtime/` | `wubu_image_manifest` | 165 | fcntl.h, stat.h, unistd.h, wubu_image_internal.h | wubu_image_manifest.c  --  WuBuOS Image Manifest JSON Operations Extracted from wubu_image.c (2026-0 |
| `runtime/` | `wubu_image_ops` | 239 | dirent.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h, wubu_i | wubu_image_ops.c  --  WuBuOS Image Tag/Remove/Inspect/Push/Pull Extracted from wubu_image.c (2026-07 |
| `runtime/` | `wubu_image_parse` | 306 | wubu_image_internal.h | wubu_image_parse.c  --  WuBuOS WuBuFile Parser Extracted from wubu_image.c (2026-07-06): all WuBuFil |
| `runtime/` | `wubu_image_tar` | 157 | dirent.h, fcntl.h, ftw.h, time.h, wait.h, wubu_image.h, wubu_image_internal.h | wubu_image_tar.c -- WuBuOS image builder: TAR layer writer. Extracted from wubu_image.c (separable l |
| `runtime/` | `wubu_launch_test` | 90 | stdio.h, stdlib.h, string.h, wubu_container.h, wubu_ct_isolate.h, wubu_proton.h, | wubu_launch_test.c -- WuBuOS container/Proton launch + session-split tests. Validates Workstream B ( |
| `runtime/` | `wubu_manifest` | 157 | stat.h, stdio.h, stdlib.h, string.h, types.h, wubu_manifest_internal.h | wubu_manifest.c -- WuBuOS unified manifest API (load/resolve/gate/emit). Adopted from GrahaOS etc/gc |
| `runtime/` | `wubu_manifest_json` | 221 | stdint.h, stdio.h, stdlib.h, string.h, wubu_manifest_internal.h | wubu_manifest_json.c -- minimal JSON parser for the WuBuOS manifest. Self-contained C11. Supports on |
| `runtime/` | `wubu_manifest_test` | 85 | stdio.h, string.h, wubu_manifest.h | wubu_manifest_test.c -- unit tests for the WuBuOS unified manifest. Proves: (1) JSON parses, (2) res |
| `runtime/` | `wubu_netlink` | 327 | errno.h, fcntl.h, if.h, if_bridge.h, if_vlan.h, inet.h, ioctl.h, netlink.h, rtne | wubu_netlink.c  --  WuBuOS Netlink RTNETLINK Implementation Extracted from wubu_network.c (2026-07-0 |
| `runtime/` | `wubu_network` | 320 | errno.h, fcntl.h, if.h, inet.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, w | wubu_network.c  --  WuBuOS Container Network Profiles (Real Implementation) Phase 7+: In-memory netw |
| `runtime/` | `wubu_network_cni` | 122 | stdio.h, stdlib.h, string.h, unistd.h, wait.h, wubu_netlink.h, wubu_network.h, w | wubu_network_cni.c -- WuBuOS network: CNI plugin load/exec backend. Extracted from wubu_network.c (s |
| `runtime/` | `wubu_network_create` | 271 | errno.h, stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_netwo | wubu_network_create.c -- WuBuOS network: network-create subsystem. Extracted from wubu_network.c (se |
| `runtime/` | `wubu_network_dns` | 46 | stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_network_intern | wubu_network_dns.c -- WuBuOS network: DNS record management. Extracted from wubu_network.c (separabl |
| `runtime/` | `wubu_network_fw` | 41 | string.h, wubu_network.h, wubu_network_internal.h | wubu_network_fw.c -- Network firewall rule subsystem (self-contained). wubu_network_firewall_add_rul |
| `runtime/` | `wubu_network_qos` | 67 | stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_network_intern | wubu_network_qos.c -- WuBuOS network: QoS / traffic shaping. Extracted from wubu_network.c (separabl |
| `runtime/` | `wubu_network_svc` | 46 | wubu_network_internal.h | wubu_network_svc.c -- Network port-mapping + DNS record subsystem. Self-contained: port map add/remo |
| `runtime/` | `wubu_network_test` | 532 | assert.h, stdio.h, stdlib.h, string.h, wubu_network.h | wubu_network_test.c  --  WuBuOS Container Network Profiles Test Suite Tests all network operations:  |
| `runtime/` | `wubu_network_ts` | 104 | stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_network_intern | wubu_network_ts.c -- WuBuOS network: Tailscale up/down/status backend. Extracted from wubu_network.c |
| `runtime/` | `wubu_network_wg` | 79 | stdio.h, stdlib.h, string.h, wubu_netlink.h, wubu_network.h, wubu_network_intern | wubu_network_wg.c -- WuBuOS network: WireGuard peer management backend. Extracted from wubu_network. |
| `runtime/` | `wubu_ns_9p_test` | 169 | fcntl.h, stat.h, stdio.h, stdlib.h, string.h, styx.h, styxfs.h, types.h, unistd. | wubu_ns_9p_test.c -- prove /n is served over REAL Styx/9P end-to-end. The bridge publishes control-p |
| `runtime/` | `wubu_ns_bridge` | 144 | errno.h, stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_archd.h, w | wubu_ns_bridge.c -- WuBuOS Namespace Bridge: archd services + bottles exposed as a uniform 9P/Styx c |
| `runtime/` | `wubu_ns_bridge_test` | 178 | stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_bottles.h, wubu_ns_ | wubu_ns_bridge_test.c -- verify the Namespace Bridge control plane. Uses dependency-injected mock se |
| `runtime/` | `wubu_ns_fs` | 76 | errno.h, stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_archd.h, w | wubu_ns_fs.c -- WuBuOS Namespace Bridge: filesystem core. The shared /n tree primitives: the root ha |
| `runtime/` | `wubu_ns_kernel` | 124 | stdio.h, stdlib.h, string.h, wubu_ns_bridge.h, wubu_ns_bridge_internal.h | wubu_ns_kernel.c -- WuBuOS Namespace Bridge: kernel + hw control plane (rip off CachyOS kernel-manag |
| `runtime/` | `wubu_ns_kernel_test` | 88 | stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_ns_bridge.h | wubu_ns_kernel_test.c -- verify the kernel + hw control plane (rip off CachyOS kernel-manager / chwd |
| `runtime/` | `wubu_ns_pkg` | 106 | stdio.h, stdlib.h, string.h, wubu_ns_bridge.h, wubu_ns_bridge_internal.h, wubu_p | wubu_ns_pkg.c -- WuBuOS Namespace Bridge: packages as a 9P control plane (rip off pacman/Chaotic-AUR |
| `runtime/` | `wubu_ns_pkg_test` | 96 | stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_ns_bridge.h, wubu_p | wubu_ns_pkg_test.c -- verify the package control plane (rip off pacman/Chaotic-AUR through /n). Driv |
| `runtime/` | `wubu_ns_snap` | 119 | stdio.h, stdlib.h, string.h, wubu_ns_bridge.h, wubu_ns_bridge_internal.h, wubu_s | wubu_ns_snap.c -- WuBuOS Namespace Bridge: snapshots as a 9P control plane (rip off snapper/btrfs ro |
| `runtime/` | `wubu_ns_snap_test` | 117 | stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_ns_bridge.h, wubu_s | wubu_ns_snap_test.c -- verify the snapshot control plane (rip off snapper/btrfs rollback through /n) |
| `runtime/` | `wubu_oci_test` | 249 | stdio.h, stdlib.h, string.h, time.h, unistd.h, wubu_image.h, wubu_oci.h | wubu_oci_test.c  --  Tests for OCI runtime |
| `runtime/` | `wubu_pkg` | 137 | stdio.h, string.h, wubu_pkg.h | wubu_pkg.c  --  WuBuOS Package Manager Implementation |
| `runtime/` | `wubu_proton` | 402 | errno.h, fcntl.h, stat.h, stdio.h, stdlib.h, string.h, strings.h, unistd.h, wubu | wubu_proton.c  --  WuBuOS Proton: Windows Compatibility Layer Implementation Cell 092: Proton-style  |
| `runtime/` | `wubu_proton2` | 351 | dirent.h, errno.h, fcntl.h, input.h, ioctl.h, signal.h, stat.h, stdio.h, stdlib. | wubu_proton2.c  --  WuBuOS Proton: Real Wine/Proton Container Cell 399: Proton runs as an Arch Linux |
| `runtime/` | `wubu_proton2_device` | 151 | dirent.h, errno.h, fcntl.h, input.h, ioctl.h, stat.h, stdio.h, stdlib.h, string. | wubu_proton2_device.c -- WuBuOS proton2: HID/USB/MIDI device enumeration. Extracted from wubu_proton |
| `runtime/` | `wubu_proton2_gamescope` | 100 | stdio.h, string.h, wubu_proton2.h | wubu_proton2_gamescope.c -- WuBuOS proton2: gamescope/wine launch-command builder. Extracted from wu |
| `runtime/` | `wubu_proton2_gpu` | 62 | dirent.h, fcntl.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_proton2.h | wubu_proton2_gpu.c -- GPU detection subsystem (self-contained). wubu_gpu_detect / wubu_gpu_open: sca |
| `runtime/` | `wubu_proton2_launch` | 67 | signal.h, stdio.h, string.h, wubu_proton2.h | wubu_proton2_launch.c -- WuBuOS proton2: app process launch/control. Extracted from wubu_proton2.c ( |
| `runtime/` | `wubu_proton2_test` | 309 | stat.h, stdio.h, string.h, types.h, unistd.h, wubu_proton2.h | wubu_proton2_test.c  --  Tests for Proton container + HID/USB + GPU Cell 399: Proton runs as real Ar |
| `runtime/` | `wubu_proton_api` | 137 | stdio.h, stdlib.h, string.h, strings.h, wubu_proton.h | wubu_proton_api.c -- WuBuOS Proton Win32->VSL API translation subsystem Extracted from wubu_proton.c |
| `runtime/` | `wubu_proton_dll` | 52 | string.h, strings.h, wubu_proton.h | wubu_proton_dll.c -- Proton DLL management subsystem (self-contained). wubu_proton_register_dll/find |
| `runtime/` | `wubu_proton_dxvk` | 172 | stdio.h, stdlib.h, string.h, wubu_dxvk_conf.h, wubu_proton_dxvk.h | wubu_proton_dxvk.c -- canonical Proton DXVK/VKD3D config core (dedup home). Writes/reads the per-pre |
| `runtime/` | `wubu_proton_pe` | 114 | string.h, wubu_proton.h | wubu_proton_pe.c -- PE (Portable Executable) validation/parsing subsystem. Self-contained: wubu_prot |
| `runtime/` | `wubu_proton_test` | 777 | stdbool.h, stdio.h, stdlib.h, string.h, wubu_proton.h | wubu_proton_test.c  --  Test Suite for WuBuOS Proton (Windows Compat Layer) Cell 092: Tests PE valid |
| `runtime/` | `wubu_ramdisk` | 340 | errno.h, mount.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h, wubu_ar | wubu_ramdisk.c  --  WuBuOS Root Mount for Arch Container Roots Cell 392: Two-mode Arch root  --  RAM |
| `runtime/` | `wubu_ramdisk_format` | 42 | stat.h, string.h, wubu_ramdisk.h, wubu_ramdisk_internal.h | wubu_ramdisk_format.c -- WuBuOS ramdisk: image format sniffing. Extracted from wubu_ramdisk.c (separ |
| `runtime/` | `wubu_ramdisk_test` | 208 | assert.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_ramdisk.h | wubu_ramdisk_test.c  --  Tests for Two-Mode Root Mount Cell 392: RAM for containers, SSD for bare me |
| `runtime/` | `wubu_realm` | 115 | stdio.h, stdlib.h, string.h, wubu_archd_svc.h, wubu_realm.h | wubu_realm.c -- Mega OS realm abstraction (Phase B + D). Lifecycle via the supervisor (N1-N4/N8/N9); |
| `runtime/` | `wubu_selfimprove` | 96 | stdlib.h, string.h, wubu_selfimprove.h | wubu_selfimprove.c -- Mega OS self-improvement loop (Phase C). Independent verifier + human gate + f |
| `runtime/` | `wubu_session` | 58 | hosted.h, stdio.h, string.h, wubu_container.h, wubu_session.h | wubu_session.c -- WuBuOS session management (SteamOS gamescope lesson). Implements the host session  |
| `runtime/` | `wubu_snapshot` | 496 | dirent.h, errno.h, fcntl.h, ftw.h, limits.h, stat.h, stdio.h, stdlib.h, string.h | wubu_snapshot.c  --  WuBuOS Container Snapshot Manager (Real Implementation) Phase 7+: In-memory sna |
| `runtime/` | `wubu_snapshot_copy` | 169 | dirent.h, errno.h, fcntl.h, ftw.h, limits.h, sendfile.h, stat.h, stdio.h, stdlib | wubu_snapshot_copy.c -- WuBuOS snapshot manager: recursive fs copy + dir size. Extracted from wubu_s |
| `runtime/` | `wubu_snapshot_diff` | 32 | wubu_snapshot.h, wubu_snapshot_internal.h | wubu_snapshot_diff.c -- Snapshot diff subsystem (self-contained). wubu_snapshot_diff: textual diff b |
| `runtime/` | `wubu_snapshot_fs` | 237 | btrfs.h, errno.h, fcntl.h, fs.h, ioctl.h, libgen.h, mount.h, stat.h, statfs.h, s | wubu_snapshot_fs.c  --  WuBuOS Filesystem Native Snapshot Operations Extracted from wubu_snapshot.c  |
| `runtime/` | `wubu_snapshot_gc` | 126 | string.h, wubu_snapshot.h, wubu_snapshot_internal.h | wubu_snapshot_gc.c -- WuBuOS snapshot: garbage collection + retention rules. Extracted from wubu_sna |
| `runtime/` | `wubu_snapshot_tag` | 54 | wubu_snapshot_internal.h | wubu_snapshot_tag.c -- Snapshot tag operations subsystem. Self-contained: tag create/delete/list. Us |
| `runtime/` | `wubu_snapshot_test` | 465 | assert.h, errno.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_snapshot.h, wubu_ | wubu_snapshot_test.c  --  WuBuOS Container Snapshot Manager Test Suite Tests all snapshot operations |
| `runtime/` | `wubu_snapshot_xport` | 98 | stdio.h, stdlib.h, string.h, wubu_snapshot.h, wubu_snapshot_internal.h | wubu_snapshot_xport.c -- WuBuOS snapshot: tarball export/import. Extracted from wubu_snapshot.c (sep |
| `runtime/` | `wubu_spawn` | 29 | fcntl.h, unistd.h, wait.h, wubu_spawn.h | wubu_spawn.c  --  Shell-free external program launcher. See wubu_spawn.h. Dependency-free: only POSI |
| `runtime/` | `wubu_spawn_test` | 102 | stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wait.h, wubu_netlink.h,  | wubu_spawn_test.c  --  Regression test for the shell-free launcher. Asserts wubu_run_program() actua |
| `runtime/` | `wubu_system` | 138 | errno.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_snapshot.h, wubu_system.h | wubu_system.c -- WuBuOS System Root (immutable / atomic) layer. Implementation wraps wubu_snapshot.c |
| `runtime/` | `wubu_system_test` | 85 | stdio.h, stdlib.h, string.h, unistd.h, wubu_system.h | wubu_system_test.c -- WuBuOS System Root (immutable/atomic) tests. Verifies the SteamOS-style immuta |
| `runtime/` | `wubu_trace` | 133 | stat.h, stdio.h, stdlib.h, string.h, time.h, wubu_trace.h | wubu_trace.c -- Mega OS trace foundation (Phase A). Immutable, versioned, user-owned trace store wit |
| `runtime/` | `wubu_txn` | 210 | pthread.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_txn_internal.h | wubu_txn.c -- WuBuOS transactional speculation engine. Ported from GrahaOS kernel/txn/transaction.c  |
| `runtime/` | `wubu_txn_test` | 87 | stat.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wubu_txn.h | wubu_txn_test.c -- WuBuOS transactional speculation self-test. Proves the engine does real work (not |
| `runtime/` | `wubu_uuid` | 133 | fcntl.h, stdio.h, stdlib.h, string.h, time.h, unistd.h, wubu_uuid.h | wubu_uuid.c — UUIDv7 generation (RFC 9562) for WuBuOS session tracking. Pure C11, no external libs.  |
| `runtime/` | `wubu_uuid_test` | 75 | stdio.h, string.h, time.h, wubu_uuid.h | wubu_uuid_test.c — Test UUIDv7 generation and parsing. |
| `runtime/` | `wubu_verifier_bytropix` | 126 | signal.h, spawn.h, stdbool.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h, wub | wubu_verifier_bytropix.c -- bind bytropix (local C/CUDA inference engine) as the INDEPENDENT verifie |
| `runtime/` | `wubu_vsl` | 14 | vsl_internal.h | wubu_vsl.c  --  WuBuOS VSL Legacy API Shim This file exists ONLY for backward compatibility with the |
| `runtime/` | `wubu_vsl_test` | 364 | errno.h, resource.h, stdio.h, stdlib.h, string.h, unistd.h, vsl_syscall_numbers. | wubu_vsl_test.c  --  WuBuOS VSL (Virtualization Substrate Layer) Test Suite Tests the "Proton within |
| `shell/` | `wubu_shell` | 150 | ctype.h, errno.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_shell_internal.h | wubu_shell.c -- WuBuOS shell: state, REPL driver, main entry Self-contained. Owns the ShellState lif |
| `shell/` | `wubu_shell_complete` | 66 | string.h, wubu_shell_internal.h | wubu_shell_complete.c -- WuBuOS shell tab completion (BATTLESHIP gap 229) Self-contained. Completes  |
| `shell/` | `wubu_shell_exec` | 269 | ctype.h, errno.h, fcntl.h, stdio.h, stdlib.h, string.h, types.h, unistd.h, wait. | wubu_shell_exec.c -- WuBuOS shell pipeline + redirection engine (BATTLESHIP gaps 230 shell_pipe / 23 |
| `shell/` | `wubu_shell_history` | 58 | string.h, wubu_shell_internal.h | wubu_shell_history.c -- WuBuOS shell command history (BATTLESHIP gap 228) Self-contained ring buffer |
| `shell/` | `wubu_shell_test` | 150 | stat.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_shell_internal.h | wubu_shell_test.c -- WuBuOS shell test suite Exercises the four previously-form-not-function shell g |
| `tools/` | `desktop_render_test` | 67 | dosgui_desktop.h, dosgui_wm.h, screenshot.h, stdio.h, stdlib.h, vbe.h, wubu_them | desktop_render_test.c -- Test that desktop rendering produces non-black output |
| `tools/` | `iso9660` | 436 | iso9660.h, stdio.h, stdlib.h, string.h | iso9660.c  --  WuBuOS ISO 9660 / Bootable ISO Builder Cell 060: Builds bootable ISO 9660 images with |
| `tools/` | `iso9660_test` | 441 | iso9660.h, stdio.h, stdlib.h, string.h | iso9660_test.c  --  Test Suite for WuBuOS ISO 9660 / Bootable ISO Builder Cell 060: Tests ISO builde |
| `tools/` | `screenshot` | 405 | dosgui_wm.h, screenshot.h, stb_image_write.h, stdbool.h, stdint.h, stdio.h, stdl | wubu_screenshot.c  --  WuBuOS Screenshot/Snipping Tool Implementation PPM/BMP/PNG capture, GIF anima |
| `tools/` | `screenshot_test` | 149 | assert.h, screenshot.h, stdio.h, stdlib.h, string.h | wubu_screenshot_test.c  --  Tests for screenshot/snipping tool |
| `tools/` | `weight_check` | 90 | stdio.h, stdlib.h, string.h, weight_check.h | weight_check.c  --  WuBuOS Vision Weight Verification Implementation Cell 051: Checks Moondream3 saf |
| `tools/` | `weight_check_test` | 131 | stdio.h, string.h, weight_check.h | weight_check_test.c  --  Test Suite for Vision Weight Verification Cell 051: Tests weight checking,  |
| `worldsim/` | `entity` | 63 | string.h, worldsim.h | Find free slot |
| `worldsim/` | `physics` | 57 | worldsim.h | Apply gravity |
| `worldsim/` | `render` | 125 | string.h, worldsim.h | Biome colors (XRGB8888) |
| `worldsim/` | `sim` | 75 | string.h, worldsim.h | Generate terrain |
| `worldsim/` | `terrain` | 140 | string.h, worldsim.h, wubu_math.h | -- RNG -- |
| `worldsim/` | `test_worldsim` | 301 | assert.h, stdio.h, stdlib.h, string.h, worldsim.h | -- RNG Tests -- |
