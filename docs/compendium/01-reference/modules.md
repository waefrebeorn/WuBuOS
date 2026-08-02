<!-- GENERATED FILE -- do not edit by hand.
     Run `make docs` (tools/gen_docs.py) to regenerate. -->

# Modules
> Auto-generated from the source tree.  Purpose = the module's own header comment.

| Module | Lines | Public API | Depends on | Purpose |
|--------|------:|-----------:|------------|---------|
| `ahci` | 304 | 1 | ahci.h, stdio.h, stdlib.h, string.h | ahci.c  --  WuBuOS AHCI (SATA) Disk Driver Implementation Cell 072: AHCI driver with hosted simulation mode. I |
| `ahci_test` | 401 | 0 | ahci.h, stdio.h, stdlib.h, string.h | ahci_test.c  --  Test Suite for WuBuOS AHCI (SATA) Disk Driver Cell 072: Tests HBA init, port enumeration, IDE |
| `fat32` | 36 | 1 | fat32_internal.h, stdlib.h, string.h, time.h | fat32.c  --  WuBuOS FAT32 Filesystem (facade) This file is the public entry point only. The real work is split |
| `fat32_cluster` | 94 | 4 | fat32.h, fat32_internal.h, stdint.h, stdlib.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `fat32_dir` | 335 | 1 | fat32_internal.h, stdlib.h, string.h, time.h | fat32_dir.c -- directory enumeration, lookup, create, delete (leaf module). Opaque fat32_volume via fat32_inte |
| `fat32_fat` | 64 | 0 | fat32_internal.h, stdlib.h, string.h | fat32_fat.c -- FAT entry read/write + cache (leaf module). Opaque fat32_volume via fat32_internal.h. C11, mini |
| `fat32_file` | 264 | 0 | fat32_internal.h, stdlib.h, string.h, time.h | fat32_file.c -- open/close/read/write/seek on a FAT32 file (leaf module). Opaque fat32_volume via fat32_intern |
| `fat32_format` | 210 | 1 | fat32_internal.h, stdlib.h, string.h, time.h | fat32_format.c -- mount/unmount/format/validate (volume lifecycle, leaf module). Opaque fat32_volume via fat32 |
| `fat32_name` | 80 | 1 | ctype.h, fat32.h, fat32_internal.h, stdlib.h, string.h | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `fat32_test` | 714 | 0 | assert.h, fat32.h, fat32_internal.h, stdio.h, stdlib.h, string.h | fat32_test.c  --  My Seed FAT32 Filesystem Test Suite Uses a RAM-backed block device for fast, deterministic t |
| `input` | 129 | 0 | input.h, string.h | input.c  --  My Seed Input Subsystem (hosted stub) Circular buffers for keyboard/mouse events with proper over |
| `input_test` | 278 | 0 | input.h, stdio.h, string.h | input_test.c  --  Kernel Input Subsystem Test Suite Cell 202: Tests for input queue (keyboard/mouse circular b |
| `interrupt` | 624 | 14 | interrupt.h, interrupt_apic.h, interrupt_io.h, interrupt_pic.h, memory.h, signal.h, stdint | interrupt.c  --  My Seed IDT/PIC Interrupt Controller Full x86_64 IDT implementation with 256 interrupt gates. |
| `interrupt_apic` | 200 | 4 | interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, signal.h, stdint.h, string.h, tas | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `interrupt_pic` | 107 | 3 | interrupt_apic.h, interrupt_io.h, interrupt_pic.h, memory.h, stdint.h | interrupt_pic.c -- 8259 PIC layer + IRQ routing for the WuBuOS kernel. Extracted from the monolithic interrupt |
| `interrupt_pic_test` | 55 | 2 | interrupt_apic.h, interrupt_pic.h, stdio.h, string.h | interrupt_pic_test.c -- unit test for the extracted PIC + IRQ routing module (interrupt_pic.c). Builds in bare |
| `interrupt_pit` | 57 | 1 | interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, signal.h, stdint.h, string.h, tas | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `interrupt_syscall` | 41 | 1 | interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, signal.h, stdint.h, string.h, tas | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `interrupt_timer` | 45 | 2 | interrupt.h, interrupt_apic.h, interrupt_io.h, memory.h, signal.h, stdint.h, string.h, tas | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `klog` | 125 | 1 | klog.h, stdarg.h, stddef.h, stdint.h | klog.c -- WuBuOS bare-metal kernel log sink (serial COM1) Self-contained freestanding output. Writes to the CO |
| `libc` | 589 | 6 | klog.h, stdarg.h, stddef.h, stdint.h | Minimal libc for bare-metal kernel |
| `memory` | 571 | 0 | memory.h, stdio.h, stdlib.h, string.h | memory.c  --  My Seed Kernel Memory Subsystem Implementation Clean C11 reimplementation of ZealOS heap design. |
| `memory_test` | 293 | 0 | assert.h, memory.h, stdio.h, stdlib.h, string.h | memory_test.c  --  Test suite for My Seed Kernel Memory Subsystem |
| `metal_main` | 346 | 4 | input.h, interrupt.h, interrupt_apic.h, klog.h, memory.h, ps2.h, stdint.h, tasking.h, vbe. | metal_main.c  --  WuBuOS Bare-Metal Kernel Entry Point Called from crt0.S after Limine/Stivale2 boot. Initiali |
| `ps2` | 224 | 2 | input.h, interrupt.h, ps2.h, stdint.h | ps2.c  --  PS/2 Keyboard and Mouse Driver (Bare Metal) Ported from Mythos Fable (filipvabrousek/osdev) for WuB |
| `tasking` | 519 | 2 | interrupt.h, libc.h, memory.h, setjmp.h, stddef.h, stdint.h, string.h, tasking.h | tasking.c  --  My Seed Kernel Task Management (hosted test impl) Uses setjmp/longjmp for context switching in  |
| `tasking_test` | 181 | 1 | assert.h, memory.h, stdio.h, stdlib.h, string.h, tasking.h | tasking_test.c  --  Test suite for My Seed Tasking Subsystem |
| `test_agi_kernel` | 195 | 0 | stdio.h, string.h, wubu_agi_kernel.h, wubu_attest.h, wubu_bonzi.h | test_agi_kernel.c -- Verify the WuBuOS bare-metal AGI kernel supervisor runs correctly (hosted harness; the sa |
| `test_agi_kernel_stub` | 49 | 1 | input.h, klog.h, stdio.h, tasking.h, vbe.h | test_agi_kernel_stub.c -- Minimal kernel-API shims so wubu_agi_kernel.c links + runs in the HOSTED unit test w |
| `test_hive` | 183 | 1 | stdio.h, stdlib.h, string.h, wubu_hive.h | test_hive.c -- wubu_hive (C11 luddite hive) unit tests. Verifies the three-way tradeoff the hand-drawn diagram |
| `test_theme_hid` | 92 | 0 | assert.h, stddef.h, stdint.h, stdio.h, string.h, wubu_hid.c, wubu_hid.h, wubu_theme.c, wub | test_theme_hid.c -- host tests for the /theme namespace + unified HID. Builds the two freestanding kernel modu |
| `txfs` | 345 | 6 | stdio.h, stdlib.h, string.h, txfs.h | txfs.c  --  WuBuOS Transactional Filesystem Layer Implementation Cell 100: Journal-based atomic filesystem ope |
| `txfs_test` | 572 | 0 | stdio.h, stdlib.h, string.h, txfs.h | txfs_test.c  --  Test Suite for WuBuOS Transactional Filesystem Cell 100: Tests journal-based atomic operation |
| `vbe` | 591 | 10 | klog.h, math.h, memory.h, stdbool.h, stdio.h, stdlib.h, string.h, vbe.h | vbe.c  --  WuBuOS VBE Framebuffer Implementation Two modes: - Kernel mode (default): uses mem_alloc/mem_free f |
| `wubu_agi_kernel` | 334 | 2 | klog.h, string.h, tasking.h, vbe.h, wubu_agi_kernel.h, wubu_attest.h, wubu_bonzi.h, wubu_c | wubu_agi_kernel.c -- WuBuOS Bare-Metal AGI Kernel Supervisor (ring-0). Freestanding C11: NO malloc, NO pthread |
| `wubu_apic` | 106 | 1 | interrupt.h, interrupt_apic.h, klog.h, stdint.h, wubu_apic.h | wubu_apic.c -- local APIC + I/O APIC bring-up (q35-correct delivery). Steps (see wubu_apic.h for the "why"): 1 |
| `wubu_attest` | 103 | 8 | string.h, wubu_attest.h | wubu_attest.c -- WuBuOS kernel-side firmware attestation consumer (ring-0). Freestanding C11: no malloc, no pt |
| `wubu_bonzi` | 361 | 0 | input.h, klog.h, stdio.h, string.h, tasking.h, vbe.h, wubu_agi_kernel.h, wubu_attest.h, wu | wubu_bonzi.c -- Bonzi Buddy: bare-metal AGI agent persona (ring-0 task). Freestanding C11. Runs as a kernel ta |
| `wubu_console` | 276 | 0 | klog.h, libc.h, memory.h, stdint.h, string.h, tasking.h, wubu_agi_kernel.h, wubu_console.h | wubu_console.c -- live ring-0 console REPL (TempleOS-style). The metal kernel owns a COM1 interactive shell: p |
| `wubu_gaad` | 595 | 2 | math.h, stdlib.h, string.h, wubu_gaad.h, wubu_math.h | wubu_gaad.c  --  WuBuOS Golden Aspect Adaptive Decomposition Cell 393: GAAD  --  the universal resolution tran |
| `wubu_gaad_test` | 269 | 0 | assert.h, stdio.h, stdlib.h, string.h, wubu_gaad.h | wubu_gaad_test.c  --  GAAD: Golden Aspect Adaptive Decomposition Cell 393: Tests for the universal resolution  |
| `wubu_hid` | 146 | 8 | wubu_hid.h | wubu_hid.c  --  WuBuOS Unified HID Layer (GameInput-style) A single ring of unified events, common time base ( |
| `wubu_hive` | 211 | 2 | string.h, wubu_hive.h | wubu_hive.c -- C11 "luddite hive" (see wubu_hive.h for the design). Linked fixed-capacity blocks + bit skipfie |
| `wubu_math` | 581 | 1 | math.h, stddef.h, stdint.h, stdio.h | wubu_math.c  --  WuBuOS Pure C Math Library Cell 420: Pure C implementations replacing libm. IEEE 754 complian |
| `wubu_pci` | 92 | 2 | libc.h, stdint.h, wubu_pci.h | wubu_pci.c -- minimal PCI config-space access (0xCF8/0xCFC). The metal kernel previously had no PCI access at  |
| `wubu_theme` | 218 | 0 | stddef.h, stdio.h, wubu_theme.h | wubu_theme.c  --  WuBuOS Metal Theme Engine + /theme Namespace The graphic set as a writable node tree. Preset |
| `fw_acpi` | 135 | 0 | fw.h, fw_acpi.h, fw_pci.h | fw_acpi.c  --  WuBuFW ACPI table discovery and publication. On a QEMU/x86 machine the firmware normally *build |
| `fw_acpiload` | 166 | 0 | fw.h, fw_acpi.h, fw_fwcfg.h | fw_acpiload.c  --  Execute QEMU's etc/table-loader linker script. The loader is a sequence of fixed-size comma |
| `fw_agi` | 132 | 2 | fw.h, fw_agi_attest.h, fw_secureboot.h, fw_tpm.h | fw_agi.c  --  WuBuOS AGI OS kernel shim (firmware-resident microkernel). This is the firmware-side half of the |
| `fw_ahci` | 277 | 4 | fw.h, fw_block.h, fw_pci.h | fw_ahci.c  --  WuBuFW AHCI (SATA) driver. Real port init + command list / FIS / PRDT DMA. AHCI is how every mo |
| `fw_ata` | 221 | 2 | fw.h, fw_block.h | fw_ata.c  --  WuBuFW ATA PIO block driver (28/48-bit LBA). QEMU's default disk on -drive if=ide is an IDE/ATA  |
| `fw_block` | 45 | 2 | fw.h, fw_block.h | fw_block.c  --  WuBuFW unified block device layer. fw_media.c used to read straight from ATA PIO, which meant  |
| `fw_bs_mem` | 255 | 8 | fw.h | fw_bs_mem.c  --  Boot services: TPL, memory, events, timers, misc. |
| `fw_bs_proto` | 283 | 10 | fw.h | fw_bs_proto.c  --  Boot services: protocol database + image services. |
| `fw_con` | 119 | 0 | fw.h | fw_con.c  --  WuBuFW EFI_SIMPLE_TEXT_{INPUT,OUTPUT}_PROTOCOL thunks. These are the ms_abi wrappers over the ra |
| `fw_drivers` | 81 | 3 | fw.h, fw_block.h, fw_pci.h, fw_tpm.h | fw_drivers.c  --  WuBuFW driver manager and boot-stage measurement. Binds real drivers to the devices PCI enum |
| `fw_e1000` | 263 | 0 | fw.h, fw_block.h, fw_pci.h | fw_e1000.c  --  WuBuFW Intel 8254x (e1000) Ethernet driver. QEMU's default NIC is exactly this part (8086:10D3 |
| `fw_fsproto` | 229 | 0 | fw.h | fw_fsproto.c  --  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL / EFI_FILE_PROTOCOL bound to a fw_volume. Read-only: Write/D |
| `fw_fwcfg` | 150 | 1 | fw.h, fw_fwcfg.h | fw_fwcfg.c  --  QEMU fw_cfg interface and ACPI table installation. QEMU does not place ACPI tables in memory f |
| `fw_gop` | 161 | 0 | fw.h, fw_pci.h | fw_gop.c  --  WuBuFW Graphics Output Protocol over a linear framebuffer. A gaming-console-class firmware must  |
| `fw_guid` | 38 | 0 | efi.h | fw_guid.c  --  WuBuFW well-known EFI GUID definitions. |
| `fw_handle` | 109 | 4 | fw.h | fw_handle.c  --  WuBuFW handle + protocol database. UEFI handles are opaque pointers; ours point into a fixed  |
| `fw_lib` | 245 | 0 | fw.h, stdarg.h | fw_lib.c  --  WuBuFW freestanding string/mem + console (serial + VGA text). The firmware links -nostdlib, so e |
| `fw_main` | 198 | 2 | fw.h, fw_acpi.h, fw_agi.h, fw_block.h, fw_fwcfg.h, fw_pci.h, fw_secureboot.h, fw_tpm.h | fw_main.c  --  WuBuFW entry point and boot manager. Called from reset.S once the CPU is in long mode with a fl |
| `fw_media` | 526 | 3 | fw.h, fw_block.h | fw_media.c  --  WuBuFW media stack: MBR/GPT partition scan + FAT12/16/32 read-only filesystem. A UEFI implemen |
| `fw_mem` | 266 | 3 | fw.h | fw_mem.c  --  WuBuFW physical memory manager. A real UEFI implementation must hand the OS a coherent memory ma |
| `fw_nvme` | 215 | 1 | fw.h, fw_block.h, fw_pci.h | fw_nvme.c  --  WuBuFW NVMe driver. Admin queue + one I/O queue pair, polled completions. Enough to identify th |
| `fw_pci` | 257 | 7 | fw.h, fw_pci.h | fw_pci.c  --  WuBuFW PCI/PCIe enumeration and resource access. Every real driver (AHCI, NVMe, XHCI, GOP) needs |
| `fw_pcires` | 128 | 1 | fw.h, fw_pci.h | fw_pcires.c  --  PCI resource (BAR) assignment. On a real boot there is nobody to assign BARs: QEMU (like real |
| `fw_pe` | 200 | 1 | fw.h | fw_pe.c  --  WuBuFW PE32+ (COFF) loader for EFI applications. Loads a BOOTX64.EFI: validates MZ/PE headers, al |
| `fw_rt` | 177 | 7 | fw.h | fw_rt.c  --  WuBuFW runtime services: time, variables, reset. Variables live in a fixed RAM store (no SPI flas |
| `fw_secureboot` | 280 | 6 | fw.h, fw_secureboot.h, fw_sha256.h, string.h | fw_secureboot.c  --  WuBuFW authenticated image verification (DB/dbx). Measured boot alone does not stop a for |
| `fw_sha256` | 115 | 4 | fw.h, fw_tpm.h | fw_sha256.c  --  SHA-256 for WuBuFW (measured boot + image verification). FIPS 180-4. Self-contained C11; no l |
| `fw_shell` | 210 | 0 | fw.h, fw_block.h, fw_pci.h, fw_tpm.h | fw_shell.c  --  WuBuFW interactive EFI-style shell. A real firmware ships an interactive shell: it is how a us |
| `fw_table` | 189 | 0 | fw.h | fw_table.c  --  Assemble the EFI system / boot / runtime service tables. |
| `fw_time` | 84 | 1 | fw.h | fw_time.c  --  WuBuFW timing + CMOS RTC. TSC is calibrated against the PIT (channel 2 gate, no interrupts) so  |
| `fw_tpm` | 388 | 2 | fw.h, fw_acpi.h, fw_tpm.h, string.h | fw_tpm.c  --  WuBuFW TPM 2.0 driver (TIS/FIFO + CRB) and measured boot. Kernel-level anti-cheat (EAC/BattlEye/ |
| `fw_tpmlog` | 119 | 1 | fw.h, fw_tpm.h | fw_tpmlog.c  --  TCG 2.0 crypto-agile event log. The event log is what makes PCR values *meaningful*: an attes |
| `fw_xhci` | 128 | 0 | fw.h, fw_pci.h | fw_xhci.c  --  WuBuFW USB 3 (xHCI) host controller driver. Firmware-scope USB: bring the controller out of res |
| `app_canvas` | 379 | 0 | dosgui_apps.h, dosgui_wm.h, stdio.h, stdlib.h, string.h, vbe.h, wubu_canvas.h, wubu_theme. | app_canvas.c  --  WuBu Canvas: in-shell image editor binding Binds the real layered wubu_canvas engine (wubu_c |
| `app_explorer` | 84 | 0 | dosgui_apps.h, dosgui_explorer.h, dosgui_wm.h, stdlib.h, string.h, vbe.h, wubu_theme.h | app_explorer.c  --  WuBuOS File Manager: in-shell Explorer binding Binds the real, Win98/XP-class dosgui_explo |
| `canvas_standalone` | 105 | 0 | dosgui_wm.h, input.h, select.h, stdbool.h, stdio.h, stdlib.h, string.h, unistd.h, vbe.h, w | canvas_standalone.c -- standalone Linux entry point for the WuBuOS image editor (the real, layered wubu_canvas |
| `control_test` | 164 | 1 | control.h, dosgui_wm.h, dosgui_wm_internal.h, stdio.h, stdlib.h, string.h, vbe.h, wubu_set | control_test.c  --  WuBuOS Control Panel Test Suite Cell 395: Win98-style settings panel. Verifies the Desktop |
| `dosgui_apps` | 274 | 3 | bonzi.h, calc.h, cmd.h, comfy.h, control.h, dosgui_apps.h, dosgui_dos_window.h, dosgui_wm. | dosgui_apps.c  --  Single App Registry Implementation ONE data-driven table (g_app_defs[]) is the source of tr |
| `dosgui_apps_test` | 600 | 1 | calc.h, control.h, dosgui_apps.h, editor.h, fm.h, notepad.h, regedit.h, repl.h, stdio.h, s | dosgui_apps_test.c  --  WuBuOS DosGui Apps Test Suite Tests for built-in Win98-style apps: - Task Manager (Win |
| `dosgui_apps_test_stubs` | 151 | 0 | dosgui_apps.h, stdarg.h, stddef.h, stdint.h, wubu_compat_db.h, wubu_notify.h, wubu_setting | dosgui_apps_test_stubs.c -- no-op link support for dosgui_apps_test. dosgui_apps.c is the app dispatcher: it r |
| `edr_dash` | 238 | 0 | dosgui_wm.h, dosgui_wm_internal.h, stdio.h, stdlib.h, string.h, vbe.h, wubu_edr.h, wubu_th | edr_dash.c -- EDR Activity Dashboard (the disclosure surface) This is the user-facing half of WuBuOS's securit |
| `notepad` | 212 | 0 | dosgui_window_chrome.h, dosgui_wm.h, dosgui_wm_internal.h, notepad.h, stdlib.h, string.h,  | notepad.c  --  My Seed Notepad (Win98-style text editor, real engine) Genuine multi-line text editing inside a |
| `repl` | 125 | 1 | dosgui_wm.h, gui_dbuf.h, holyc.h, repl.h, stdio.h, stdlib.h, string.h, vbe.h | repl.c  --  My Seed HolyC JIT REPL (runs inside GUI window) Uses the HolyC compiler (hc_eval) for evaluation U |
| `wubu_apps2_test` | 353 | 0 | stdio.h, stdlib.h, string.h, unistd.h, wubu_canvas.h, wubu_codec.h, wubu_editor.h | wubu_apps_test.c  --  Tests for Editor, Canvas, and Codec Cell 396/397/398: Notepad++ editor, Photoship canvas |
| `wubu_canvas_blend` | 52 | 0 | wubu_canvas.h | wubu_canvas_blend.c -- Canvas blend-compositing subsystem (self-contained). blend_channel + wubu_blend: per-ch |
| `wubu_canvas_draw` | 226 | 1 | stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_canvas_internal.h | wubu_canvas_draw.c -- WuBuOS canvas: pixel drawing tools, color pick, and selection. Self-contained: depends o |
| `wubu_canvas_filter` | 128 | 2 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_filter.c -- WuBuOS canvas: per-layer pixel filters (blur, sharpen, edge, invert, threshold, graysc |
| `wubu_canvas_io` | 658 | 0 | stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_image_codec_internal.h, zlib.h | wubu_canvas_io.c  --  WuBuOS Image Editor: File I/O backend Cell 397 (I/O split 2026-07-09): PNG/BMP/PPM/GIF s |
| `wubu_canvas_io_ppm` | 81 | 2 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_io_ppm.c -- Canvas PPM format save/load (self-contained). wubu_cv_save_ppm (P6 binary) + wubu_cv_l |
| `wubu_canvas_layers` | 191 | 0 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_layers.c -- WuBuOS canvas: create/destroy + layer ops + compositing. Self-contained: depends only  |
| `wubu_canvas_plugin` | 40 | 3 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_plugin.c -- WuBuOS canvas: plugin registration / run / unregister. Self-contained: depends only on |
| `wubu_canvas_transform` | 171 | 3 | stdio.h, stdlib.h, string.h, wubu_canvas.h | wubu_canvas_transform.c -- WuBuOS canvas: viewport (zoom/pan) and geometric layer transforms (resize, crop, fl |
| `wubu_canvas_undo` | 114 | 2 | stdio.h, stdlib.h, string.h, wubu_canvas.h, wubu_canvas_internal.h | wubu_canvas_undo.c -- WuBuOS canvas: undo/redo history + snapshot hook. Self-contained: owns the undo/redo sna |
| `wubu_codec` | 497 | 1 | errno.h, fcntl.h, mount.h, signal.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wait.h | wubu_codec.c  --  WuBuOS Codec Layer Implementation Cell 398: FFmpeg CLI wrapper + optional libav linkage. Dir |
| `wubu_editor` | 468 | 2 | stdio.h, stdlib.h, string.h, wubu_editor.h | wubu_editor.c  --  WuBuOS Code Editor Implementation Cell 396: Tabbed editor with syntax HL, find/replace, fol |
| `wubu_editor_bookmark` | 55 | 2 | wubu_editor.h | wubu_editor_bookmark.c -- Editor bookmark subsystem (self-contained). wubu_ed_bookmark_toggle/next/prev. Uses  |
| `wubu_editor_find` | 136 | 0 | string.h, wubu_editor.h | wubu_editor_find.c -- WuBuOS editor find/replace (split from wubu_editor.c) Self-contained subsystem: incremen |
| `wubu_editor_macro` | 42 | 3 | stdlib.h, string.h, wubu_editor.h | wubu_editor_macro.c -- WuBuOS editor: keyboard macro record/playback. Extracted from wubu_editor.c (separable  |
| `wubu_editor_selection` | 146 | 0 | stdlib.h, string.h, wubu_editor.h | wubu_editor_selection.c -- WuBuOS editor selection + clipboard (split from wubu_editor.c) Self-contained subsy |
| `wubu_editor_undo` | 130 | 0 | stdlib.h, string.h, wubu_editor.h | wubu_editor_undo.c -- WuBuOS editor undo/redo engine (split from wubu_editor.c) Self-contained subsystem: the  |
| `wubu_image_codec` | 110 | 5 | stdbool.h, stdint.h, stdio.h, stdlib.h, string.h, wubu_image_codec_internal.h | wubu_image_codec.c -- Self-contained image-codec leaf for WuBuOS canvas. Pure, dependency-free codec primitive |
| `hosted` | 219 | 2 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, dosgui_wm_holyc_term.h, fcntl | hosted.c — WuBuOS Hosted Mode Launcher (Inferno emu-style) — facade WuBuOS as a clickable Linux binary — the " |
| `hosted_pe` | 51 | 1 | hosted_internal.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_ct_isolate.h, wubu_proton.h | hosted_pe.c -- WuBuOS hosted-mode Windows/PE launch executor Self-contained concern split out of hosted.c: the |
| `hosted_render` | 46 | 0 | dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, hosted_internal.h, input.h, stdio.h, vb | hosted_render.c -- WuBuOS hosted-mode frame composition + input routing Self-contained concern split out of ho |
| `hosted_run` | 169 | 5 | dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, hosted_internal.h, stdio.h, stdlib.h, s | hosted_run.c -- WuBuOS hosted-mode run loop, shutdown, blit + accessors Self-contained concern split out of ho |
| `hosted_styxfs` | 155 | 1 | dosgui_wm.h, hosted.h, hosted_internal.h, input.h, memory.h, stdlib.h, string.h, styx.h, v | WuBuOS -- extracted module (auto-split, C11, opaque-safe) |
| `hosted_test` | 462 | 0 | dosgui_startmenu.h, dosgui_wm.h, hosted.h, stdio.h, stdlib.h, string.h, styx.h, unistd.h,  | hosted_test.c  --  WuBuOS Hosted Mode Behavioral Test Suite Tests: hosted init/shutdown, Styx namespace constr |
| `hosted_wayland` | 89 | 2 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, fcntl.h, hosted.h, hosted_int | Thin orchestration: owns the public hosted_wl_* entry points declared in hosted_internal.h. SHM pool, input li |
| `hosted_wayland_input` | 411 | 0 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, fcntl.h, hosted.h, hosted_int |  |
| `hosted_wayland_shm` | 105 | 3 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, fcntl.h, hosted.h, hosted_int |  |
| `hosted_wayland_surface` | 241 | 1 | bridge.h, dosgui_desktop.h, dosgui_startmenu.h, dosgui_wm.h, fcntl.h, hosted.h, hosted_int |  |
| `primary-selection-private` | 115 | 0 | stdint.h, stdlib.h, wayland-util.h | Generated by wayland-scanner 1.22.0 |
| `wubu_display` | 297 | 1 | errno.h, fcntl.h, input.h, stdio.h, stdlib.h, string.h, unistd.h, wubu_display.h, xf86drm. | wubu_display.c  --  WuBuOS Display Backend (DRM/KMS + X11 dual) Cell 380: Try DRM/KMS first, fall back to X11. |
| `wubu_display_test` | 64 | 1 | stdio.h, stdlib.h, string.h, wubu_display.h | wubu_drm_direct_test.c  --  Test for direct DRM/KMS implementation (Cells 388/389) |
| `wubu_gbm` | 185 | 0 | errno.h, fcntl.h, ioctl.h, mman.h, stdint.h, stdlib.h, string.h, unistd.h, wubu_gbm.h, xf8 | wubu_gbm.c  --  WuBuOS Custom GBM (Generic Buffer Management) Cell 389: Pure C GBM implementation without libg |
| `wubu_metal` | 485 | 11 | dirent.h, dlfcn.h, errno.h, fcntl.h, input.h, interrupt.h, memory.h, mman.h, stat.h, stdio | wubu_metal.c  --  WuBuOS Bare-Metal Boot + WSL2 GUI Abstraction Cell 400: Implementation of unified display/in |
| `wubu_metal_audio` | 300 | 7 | asoundlib.h, dlfcn.h, error.h, format-utils.h, pipewire.h, simple.h, stdio.h, wubu_metal_a | wubu_metal_audio.c -- WuBuOS Metal audio backends (split from wubu_metal.c). Self-contained: ALSA + PulseAudio |
| `wubu_metal_drm` | 369 | 3 | drm_fourcc.h, errno.h, fcntl.h, mman.h, stat.h, stdio.h, stdlib.h, string.h, unistd.h, wub | wubu_metal_drm.c -- WuBuOS Metal DRM/KMS display backend (split from wubu_metal.c). Self-contained: the bare-m |
| `wubu_metal_evdev` | 206 | 1 | dirent.h, dlfcn.h, errno.h, fb.h, fcntl.h, input.h, ioctl.h, mman.h, stat.h, stdio.h, stdl | wubu_metal_evdev.c -- WuBuOS evdev input backend (extracted from wubu_metal.c). Mirror of the original wubu_me |
| `wubu_metal_test` | 108 | 0 | assert.h, stdio.h, stdlib.h, wubu_metal.h | wubu_metal_test.c  --  Tests for bare-metal + WSL2 abstraction layer |
| `wubu_metal_vulkan` | 119 | 0 | stdio.h, string.h, vulkan.h, vulkan_wayland.h, vulkan_xcb.h, vulkan_xlib.h, wubu_metal.h | wubu_metal_vulkan.c -- WuBuOS Vulkan surface creation (extracted from wubu_metal.c). Self-contained: forward-d |
| `wubu_metal_x11` | 117 | 5 | Xlib.h, Xutil.h, dirent.h, dlfcn.h, errno.h, fb.h, fcntl.h, input.h, ioctl.h, mman.h, stat | wubu_metal_x11.c -- WuBuOS X11 display backend (extracted from wubu_metal.c). Mirror of the original wubu_meta |
| `wubu_vulkan_cmd` | 124 | 0 | stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_cmd.c -- WuBuOS Vulkan command-pool + queue-submit helpers (extracted from the monolithic wubu_vul |
| `wubu_vulkan_compute` | 326 | 1 | stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_compute.c -- WuBuOS Vulkan compute pipeline + result-string / memory-type utilities (extracted fro |
| `wubu_vulkan_loader` | 318 | 0 | dlfcn.h, stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_loader.c -- WuBuOS Vulkan: dynamic libvulkan loader, instance, physical-device selection, and logi |
| `wubu_vulkan_swapchain` | 206 | 3 | stdio.h, stdlib.h, string.h, wubu_vulkan.h | wubu_vulkan_swapchain.c -- WuBuOS Vulkan swapchain + presentation (extracted from the monolithic wubu_vulkan.c |
| `wubu_vulkan_util` | 60 | 0 | stdint.h, wubu_vulkan.h | wubu_vulkan_util.c -- WuBuOS Vulkan shared utilities Standalone helpers used across the Vulkan backend: VkResu |
