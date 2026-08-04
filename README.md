# WuBuOS — ZealOS Kernel + Win98 Shell + Styx/9P + Arch Containers

```
╔══════════════════════════════════════════════════════════════════════╗
║     🌱  W U B U O S                                                       ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers    ║
║     468 C files · 214 H files · ~105K LOC · 91 test targets             ║
║     (verified 2026-07-19 from `git ls-files` + `make` targets)           ║
╚══════════════════════════════════════════════════════════════════════════╝
```

## What WuBuOS is

**WuBuOS = ZealOS kernel + Win98 shell + Styx/9P + Arch containers**, built as a
single hosted binary that runs on Linux. It merges five lineage studies into one
OS-scale C11 codebase:

- **ZealOS** — the hosted kernel (memory, tasking, VBE, FAT32, AHCI, interrupt, PS/2)
- **Win98/XP shell** — WM, desktop, startmenu, explorer, terminal (DOS-box windows)
- **Styx/9P** — a real filesystem namespace backed by `.wubu` containers (9P2000)
- **Arch containers** — fork+exec into an Arch Linux rootfs (bwrap isolation)
- **HolyC JIT** — self-hosted x86-64 encoder, disassembler, register allocator, minic compiler

Plus three engines that make it an "OS-scale" project rather than a GUI shell:

- **16-bit DOS compatibility** — a real 8086 interpreter + INT 21h/10h/16h DOS layer
  that runs `.COM`/`.EXE` in-process and captures a text screen + RGBA frame for the
  Desktop "compatible window" (`src/runtime/wubu_dos_emu*`, 22/22 regression tests).
- **VSL (Virtual Syscall Layer)** — the bridge: NT → Linux → Styx/9P → ZealOS →
  HolyC JIT. Dispatched through a single machine-readable manifest
  (`src/runtime/wubu_manifest/`) that also generates the Styx9P op enum + HolyC FFI.
- **Bear RL** — PPO training with Vulkan compute pipelines (`src/bear/`).

## Repository layout

```
src/
  kernel/    memory, tasking, VBE, FAT32, AHCI, interrupt, PS/2
  compiler/  HolyC lexer, parser, codegen, PTX backend
  audio/     DAW, Furnace (30+ chips), TinySoundFont, AI plugins
  hosted/    DRM/KMS, Vulkan, X11, WSL2, macOS AVF
  runtime/   Styx/9P, VSL, containers, Arch, network, snapshot,
             16-bit DOS emulator, unified syscall manifest
  gui/       Win98 WM, desktop, startmenu, explorer, terminal
  bear/      RL training, Vulkan/CUDA, n-pole physics
  apps/      Editor, canvas, codec, freedoom, calc, control
  tools/     ISO9660, screenshot, weight_check, demo_record
  bridge/    DOS flip, syscall bridge
  shell/     Unified GUI shell
  worldsim/  GAAD, terrain, entity, physics
```

> **Module hygiene (2026-07-15 → 07-19):** large monoliths were dissolved into
> opaque C11 leaf modules with minimal includes — `wubu_editor.c` → `src/apps/editor/`,
> `wubu_canvas.c` → `src/apps/app_canvas*` + `wubu_canvas_*`, `styx.c` →
> `src/runtime/styx_*`, `wubu_clipboard.c` → `src/gui/wubu_clipboard*`,
> `wubu_proton.c` → `src/runtime/wubu_proton*` + `wubu_proton_dxvk*`, etc.
> See `docs/MONOLITH_DISSOLUTION.md` for the full old→new map. Docs that still name
> the old monolith files are out of date.

## Quick Start

```bash
make all                 # full build
make hosted              # hosted binary (runs on Linux)
./src/hosted/wubu --screenshot /tmp/screenshot.ppm

make test                # all 91 test targets
make test_dos_emu        # 16-bit DOS emulator (22 regression tests)
make test_dos_emu_smoke  # minimal DOS .COM run
make test_manifest       # unified syscall manifest (15 checks)
make test_vsl            # VSL syscalls (87 checks)

# Honest gap scan
python3 scripts/wubu_manifest_gen.py --help   # manifest codegen entry point
```

## Status (verified 2026-07-19)

| Metric | Value | Source |
|--------|-------|--------|
| C source files | 468 | `git ls-files 'src/**/*.c'` |
| Header files | 214 | `git ls-files 'src/**/*.h'` |
| Lines of code (src, tracked) | ~105,459 | `git ls-files … \| xargs wc -l` |
| Test targets (`make test_*`) | 91 | `grep -c '^test_.*:' Makefile` |
| Build | `make runtime` exits 0, clean under `-O2` | verified |
| Repo location | `/home/wubu/wubuos` | canonical |

### Known open work (not "all done")
- **E1 ReactOS NT**: 88 / 297 syscalls transliterated (209 remain).
- **Stub-phrase no-ops** still present: `tasking.c`×2, `wubu_anticheat.c`×2,
  `bear_cudnn.c`×3, `wubu_screenshot.c`, `wubu_pkgmgr.c`, `oci_http_client.c`,
  `holyc_ptx.c`, `wubu_compositor_standalone.c`×2, `wubu_compositor.c`, `wubu_bottles.c`.
- **Bare-metal no-ops**: `tasking.c`×3 (context-switch).

> Historical note: an earlier audit (`BATTLESHIP.md` v22, 2026-07-08) described the
> project as "~40 code gaps + ~370 parity marathons, 64 targets, ~15K LOC". Those
> figures are stale — the repo has since grown to 468 `.c` / ~105K LOC / 91 targets
> through the monolith-dissolution campaign. This README reflects the 2026-07-19
> verified state.

## The Mission

WuBuOS merges TempleOS (HolyC/JIT), ReactOS (NT syscall emulation), SteamOS
(Proton/gamescope), Arch/Ubuntu (pacman/systemd), and Win98 (shell) into one
hosted binary. The VSL is the bridge: NT → Linux → Styx/9P → ZealOS → HolyC JIT.

**Discipline**: opaque structs, minimal includes, C11 only. "Rewriting from scratch
in C" = closing a gap — including every ReactOS-NT syscall and every missing
SteamOS/Ubuntu/TempleOS/ZealOS subsystem, which are reclassified as REAL_GAP
marathons.

## Security note

The `src/runtime/container/wubucontainer` tree is a git **submodule** (WuBuContainer).
On 2026-07-19 its `shToElf/stub.c` self-extracting shell-ELF dropper was removed
(commit `8f480e0` in that repo; parent pin updated). It contains no WuBuOS code.

---

## License

This project is licensed under the **Waefrebeorn Umbrella License v3.0**.
See the [LICENSE](LICENSE) file for the full license text.

The Waefrebeorn Umbrella License is a custom source-available license.
It is not OSI-approved and not FSF-approved.

<!-- repodoc:BEGIN -->
## Source modules (auto-generated 2026-08-04)

| File | Purpose |
|---|---|
| `src/tools/desktop_render_test.c` | printf("=== Desktop Render Test ===
"); |
| `src/tools/iso9660.c` | - Primary Volume Descriptor (sector 16) |
| `src/tools/iso9660_test.c` | El Torito boot catalog, path tables, root directory, and validation. |
| `src/tools/screenshot.c` | static int write_ppm_internal(const char *path, const uint32_t *buf, int w, int h) { |
| `src/tools/screenshot_test.c` | Access static globals from screenshot.c via accessors */ |
| `src/tools/weight_check.c` | int weight_shard_path(int index, char *buf, int bufsz) { |
| `src/tools/weight_check_test.c` | static int g_pass = 0, g_fail = 0, g_total = 0; |
| `src/worldsim/entity.c` | ws_entity_id ws_entity_create(ws_world_t *w) { |
| `src/worldsim/physics.c` | void ws_physics_init(ws_physics_config_t *cfg) { |
| `src/worldsim/render.c` | static const uint32_t biome_colors[] = { |
| `src/worldsim/sim.c` | void ws_sim_init(ws_simulation_t *sim, uint32_t seed) { |
| `src/worldsim/terrain.c` | uint64_t ws_rng_next(uint64_t *state) { |
| `src/worldsim/test_worldsim.c` | static int passed = 0, failed = 0; |
| `src/jit/jit.c` | Uses mmap(PROT_READ|PROT_WRITE|PROT_EXEC) for executable memory |
| `src/jit/jit_encode.c` | the JIT expression compiler and backends. Declared in jit_internal.h. |
| `src/jit/jit_minic.c` | Parses a tiny subset of C and emits x86-64 machine code |
| `src/jit/jit_minic_token.c` | MinicToken/MinicTokType + fn decls in jit_internal.h. Minimal includes. |
| `src/jit/jit_test.c` | register allocator, MIR backend (gcc -shared + dlopen), |
| `src/jit/wubu_disasm.c` | Never claims to be a full x86-64 decoder — that would be 10K+ LOC. |
| `src/jit/wubu_x86.c` | Full REX.W + ModRM + SIB encoding for SysV AMD64 ABI. |
| `src/jit/x86_regalloc.c` | Caller-saved first, callee-saved with spill when exhausted. |
| `src/audio/wubu_audio.c` | Modular audio engine: chip emulations, furnace tracker, SF2 synth, DAW mixer. |
| `src/audio/wubu_audio_chips.c` | All retro chip emulations (NES, GB, YM2612, SID, etc.) |
| `src/audio/wubu_audio_daw.c` | Track management, bus routing, master processing. |
| `src/audio/wubu_audio_engine.c` | Engine lifecycle, process callback, global state. |
| `src/audio/wubu_audio_furnace.c` | Chip tracker with pattern editor, Furnace-style. |
| `src/audio/wubu_audio_sf2.c` | Simplified SF2 parser and sample playback engine. |
| `src/audio/wubu_audio_test.c` | fprintf(stderr, "FAIL: " fmt " at %s:%d
", ##__VA_ARGS__, __FILE__, __LINE__); \ |
| `src/kernel/ahci.c` | In hosted mode, provides simulated SATA ports backed by |
| `src/kernel/ahci_test.c` | sector read/write, simulated disk, and diagnostics. |
| `src/kernel/fat32.c` | self-contained leaf modules behind the opaque fat32_volume (fat32_internal.h): |
| `src/kernel/fat32_cluster.c` | uint32_t fat32_next_cluster(fat32_volume *vol, uint32_t cluster) { |
| `src/kernel/fat32_dir.c` | Opaque fat32_volume via fat32_internal.h. C11, minimal includes. */ |
| `src/kernel/fat32_fat.c` | Opaque fat32_volume via fat32_internal.h. C11, minimal includes. */ |
| `src/kernel/fat32_file.c` | Opaque fat32_volume via fat32_internal.h. C11, minimal includes. */ |
| `src/kernel/fat32_format.c` | Opaque fat32_volume via fat32_internal.h. C11, minimal includes. */ |
| `src/kernel/fat32_name.c` | void name_to_83(const char *src, char name83[11]) { |
| `src/kernel/fat32_test.c` | Tests: format, mount, directory ops, file I/O, cluster chains, |
| `src/kernel/input.c` | Uses count-based full/empty detection to support full QUEUE_SIZE capacity. |
| `src/kernel/input_test.c` | push/poll/wait functionality, modifier tracking. |
| `src/kernel/interrupt.c` | Assembly ISR stubs (isr_stubs.S) provide vector entry points and address table. |
| `src/kernel/interrupt_apic.c` | uint64_t apic_base = rdmsr(MSR_IA32_APIC_BASE); |
| `src/kernel/interrupt_pic.c` | - PIC ICW programming (pic_remap) |
| `src/kernel/interrupt_pic_test.c` | module (interrupt_pic.c). Builds in bare-metal C11 (-DMYSEED_METAL) and |
| `src/kernel/interrupt_pit.c` | if (hz == 0 || hz > 1193182) return -1; |
| `src/kernel/interrupt_syscall.c` | STAR MSR: bits 63:48 = SYSCALL CS/SS, bits 47:32 = SYSRET CS/SS */ |
| `src/kernel/interrupt_timer.c` | uint64_t timer_calibrate_tsc(void) { |
| `src/kernel/klog.c` | kernel can emit diagnostics under -nostdlib. Implements a minimal printf |
| `src/kernel/libc.c` | Minimal libc for bare-metal kernel */ |
| `src/kernel/memory.c` | Simplified from the original for correctness first; |
| `src/kernel/memory_test.c` | static void test_init_shutdown(void) { |
| `src/kernel/metal_main.c` | Initializes all kernel subsystems, then starts the shell. |
| `src/kernel/ps2.c` | Uses inline I/O port functions for portability. |
| `src/kernel/tasking.c` | Real kernel will use assembly task_switch_to in tasking_switch.S. |
| `src/kernel/tasking_test.c` | The watchdog path (tasking.c) calls interrupt_panic_dump() on a stuck |
| `src/kernel/test_acpi.c` | The RSDP scan targets the EBDA/BIOS areas (metal-only); the walk + |
| `src/kernel/test_agi_kernel.c` | runs correctly (hosted harness; the same code runs freestanding on metal). |
| `src/kernel/test_agi_kernel_stub.c` | links + runs in the HOSTED unit test without the real ring-0 scheduler. |
| `src/kernel/test_ahcifat.c` | host repro: FAT32 create through the REAL ahci sim backend |
| `src/kernel/test_ahciseq.c` | host repro of the bootvol's ahci sequence */ |
| `src/kernel/test_as.c` | The PML4 clone + CR3 switch normally touch real page tables; on the |
| `src/kernel/test_blk.c` | a fake device: a 64-sector RAM block with the count convention. */ |
| `src/kernel/test_bonzi_study.c` | printf("=== test_bonzi_study (HX-D companion GUI study) ===
"); |
| `src/kernel/test_bootattach.c` | quick host check of fat32_boot_attach on the fat32_test RAM disk */ |
| `src/kernel/test_fat2.c` | printf("=== test_fat2 (FS-B FAT family, complete) ===
"); |
| `src/kernel/test_hive.c` | - block structure: elements land in fixed blocks (vector locality) |
| `src/kernel/test_hpet.c` | The MMIO probe is metal-only; the pure conversion + register offsets |
| `src/kernel/test_iommu.c` | The engine-capability MMIO reads are metal-only; the DMAR table walk |
| `src/kernel/test_lfn.c` | static int g_pass = 0, g_fail = 0, g_total = 0; |
| `src/kernel/test_psych.c` | printf("=== test_psych (HX human psychology + timing loops) ===
"); |
| `src/kernel/test_recovery.c` | Compiles the freestanding wubu_recovery.c on the host. |
| `src/kernel/test_rtc.c` | (the CMOS port reads are metal-only; the BCD + 12h conversions are |
| `src/kernel/test_sha256.c` | static int hex_eq(const uint8_t *d, const char *hex) |
| `src/kernel/test_smbios.c` | The EPS search targets the BIOS ROM area (metal-only); the structure |
| `src/kernel/test_swap.c` | The phys window is identity-mapped metal memory, so the disk IO is |
| `src/kernel/test_sync.c` | Gap D3 stubs: the real lock's priority-inheritance calls these (the |
| `src/kernel/test_theme_hid.c` | Builds the two freestanding kernel modules with a tiny host shim. */ |
| `src/kernel/test_tutor.c` | printf("=== test_tutor (HX-C learning + recursive loop) ===
"); |
| `src/kernel/test_vdso.c` | The VA mapping is metal-only; the header + the counter refresh are |
| `src/kernel/test_verifier.c` | Builds wubu_verifier.c with a minimal shim (no AGI kernel needed: |
| `src/kernel/test_vmm.c` | registry). The page-table map + demand-fill are METAL-verified (they |
| `src/kernel/test_wdt.c` | The port I/O is metal-only; the pure conversion logic + the mode word |
| `src/kernel/test_xhci.c` | The PCI scan + the BAR0 read are metal-only; the capability parsing |
| `src/kernel/txfs.c` | Write-ahead log ensures crash consistency. |
| `src/kernel/txfs_test.c` | recovery, CRC32 validation, and crash consistency. |
| `src/kernel/vbe.c` | - Kernel mode (default): uses mem_alloc/mem_free from kernel memory.c |
| `src/kernel/wubu_acpi.c` | area pointer chain) or in the 0xE0000..0xFFFFF legacy BIOS region. |
| `src/kernel/wubu_agi_kernel.c` | See wubu_agi_kernel.h for the design (converges realm + trace + selfimprove |
| `src/kernel/wubu_apic.c` | 1. Identity-map 0xFEC00000..0xFF000000 (IOAPIC + LAPIC MMIO) with |
| `src/kernel/wubu_as.c` | the boot tables (the kernel stays visible; the address spaces differ |
| `src/kernel/wubu_attest.c` | a single static snapshot of the firmware attestation (one global -- the |
| `src/kernel/wubu_blk.c` | The agnostic implementation: device table + policy selectors. |
| `src/kernel/wubu_bonzi.c` | (wubu_agi_kernel_run spawns it). Real dispatch — no theater: |
| `src/kernel/wubu_bonzi_study.c` | int wubu_bs_empathy(uint32_t user_mood, uint32_t *response_depth) |
| `src/kernel/wubu_console.c` | echo characters, dispatch whole lines through wubu_console_exec().  The |
| `src/kernel/wubu_crash.c` | heap, ISR-safe). The panic path calls wubu_crash_dump; the boot calls |
| `src/kernel/wubu_fat2.c` | FS-B04: read a FAT entry (12/16/32 by size). */ |
| `src/kernel/wubu_gaad.c` | into squares + golden rectangles. This gives a resolution-independent |
| `src/kernel/wubu_gaad_test.c` | static void test_decompose_640x480(void) { |
| `src/kernel/wubu_hid.c` | Drivers feed; consumers poll; filters hide kinds per device. |
| `src/kernel/wubu_hive.c` | Erase = mark + freelist push (O(1), no moves).  Insert reuses a free |
| `src/kernel/wubu_hpet.c` | base at offset 44 (a Generic Address Structure: space-id, width, and |
| `src/kernel/wubu_iommu.c` | structure records (type 0 = DRHD). A DRHD carries the VT-d engine's |
| `src/kernel/wubu_lfn.c` | [0]    sequence/ordinal: bit 6 = last entry, low 6 bits = index |
| `src/kernel/wubu_math.c` | IEEE 754 compliant approximations using polynomial/CORDIC methods. |
| `src/kernel/wubu_memmap.c` | largest such region bounds the vmm's allocator. |
| `src/kernel/wubu_pci.c` | a hosted simulation).  This gives the kernel real bus discovery: scan |
| `src/kernel/wubu_psych.c` | int wubu_psych_init(wubu_psych_user_t *u) |
| `src/kernel/wubu_recovery.c` | The 5+1 doctrine: five rotating rollback slots + one Jesus-state |
| `src/kernel/wubu_rtc.c` | disable bit (0x80) is set while selecting registers, the Update-In- |
| `src/kernel/wubu_self_test.c` | - heap integrity   (mem_validate_all: the free list is coherent) |
| `src/kernel/wubu_serial.c` | - the 8250 IER bit 0 (RX data available) enabled |
| `src/kernel/wubu_sha256.c` | 64-round Merkle-Damgard construction with the FIPS constants. Used for |
| `src/kernel/wubu_smbios.c` | 8 bytes in; the 64-bit "_SM3_" puts it 16 bytes in (after the |
| `src/kernel/wubu_smp.c` | ICR, wait 10ms, then two SIPIs (0x000C4608 = SIPI to vector 8 at |
| `src/kernel/wubu_swap.c` | volume: 4096 sectors (4 MB) starting at LBA WUBU_SWAP_SECTOR. Each |
| `src/kernel/wubu_sync.c` | interrupt state is preserved (lock in an ISR -> unlock restores IF=0). |
| `src/kernel/wubu_theme.c` | writes go through the node table (EDR-counted); apply() re-derives |
| `src/kernel/wubu_tss.c` | higher-half BSS address) is computed, not linked. |
| `src/kernel/wubu_tutor.c` | static const char *topic_names[] = { |
| `src/kernel/wubu_user.c` | and iretq's to ring 3. The frame: RIP, CS=0x23, RFLAGS (IF set), |
| `src/kernel/wubu_vdso.c` | VA. The timer tick refreshes the counters; the kernel's own tests |
| `src/kernel/wubu_verifier.c` | (DA-3: same-agent grading is a rubber stamp). See wubu_verifier.h. |
| `src/kernel/wubu_vmm.c` | The #PF handler fills demand regions (allocate + map + retry) -- the |
| `src/kernel/wubu_wdt.c` | the count reaches zero. The timer tick feeds (re-arms) it; the panic |
| `src/kernel/wubu_xhci.c` | HCCPARAMS1), then the operational registers at cap_base+cap_length |
| `src/runtime/ct_iso_cgroup.c` | The cgroup create/set/attach ops live in wubu_ct_isolate_cgroup.c. |
| `src/runtime/ct_iso_ns.c` | int wubu_ns_unshare(int flags) { |
| `src/runtime/ct_iso_seccomp.c` | Allowlist approach: deny-by-default, permit explicit syscalls per runtime. |
| `src/runtime/styx_enc.c` | into a wire buffer using the inline helpers from styx.h / |
| `src/runtime/styx_fid.c` | styx_server_t / styx_fid_t / STYX_MAX_FIDS (styx.h). Minimal includes. |
| `src/runtime/styx_names.c` | includes. Originally part of the styx.c monolith; split out so the |
| `src/runtime/styx_parse.c` | extracts its fields using the inline unpack helpers from styx.h. |
| `src/runtime/styx_serve.c` | translation unit; styx_serve() reads a T-message, drives the |
| `src/runtime/styx_test.c` | fid management, error handling, edge cases. |
| `src/runtime/styxfs_callbacks.c` | embedded styx_server_t. They implement a HOST-FILE-BACKED namespace: every |
| `src/runtime/styxfs_host.c` | void styxfs_path_to_host(styxfs_server_t *srv, const char *path, |
| `src/runtime/styxfs_path.c` | void build_path(char *out, size_t out_size, const char *base, const char *name) { |
| `src/runtime/styxfs_posix.c` | Extracted from the monolithic styxfs.c. Depends on styxfs_internal.h for |
| `src/runtime/styxfs_server.c` | Implements the styx_server_t callbacks using host filesystem operations. |
| `src/runtime/styxfs_test.c` | detection, path normalization, and readonly mode. |
| `src/runtime/styxfs_util.c` | container load). Self-contained: uses styxfs_server_t/mount_t/file_t types |
| `src/runtime/styxfs_vfs.c` | Extracted from the monolithic styxfs.c. Self-contained: depends only on |
| `src/runtime/wubu_anticheat.c` | - Kernel modules (vendors must provide .ko for Linux) |
| `src/runtime/wubu_anticheat_test.c` | static void test_ac_info_none(void) { |
| `src/runtime/wubu_apps_test.c` | Cell 108: VSL app launcher (Brave browser model) |
| `src/runtime/wubu_arch.c` | chroot into. No syscall emulation. No VSL compat theater. |
| `src/runtime/wubu_arch_test.c` | (which requires root + network). We test: |
| `src/runtime/wubu_archd_daemon.c` | Extracted from the monolithic wubu_archd.c. Depends on |
| `src/runtime/wubu_archd_fs.c` | Extracted from wubu_archd.c (separable leaf). Self-contained: ftw only. |
| `src/runtime/wubu_archd_loop.c` | Owns the epoll event loop, client request parsing/dispatch, and periodic |
| `src/runtime/wubu_archd_svc.c` | Extracted from the monolithic wubu_archd.c. Depends on |
| `src/runtime/wubu_archd_svc_super.c` | services are fork/exec'd as processes inside their Arch root namespace, |
| `src/runtime/wubu_archd_test.c` | health checks, and GPU detection. |
| `src/runtime/wubu_archd_util.c` | Arch daemon. Self-contained: run_cmd / run_chroot_cmd (fork+exec, no |
| `src/runtime/wubu_bottle_flatpak.c` | Self-contained: real Wine/Proton/Bottles/Lutris/Flatpak work; shares the |
| `src/runtime/wubu_bottle_io.c` | Self-contained: real Wine/Proton/Bottles/Lutris/Flatpak work; shares the |
| `src/runtime/wubu_bottle_lifecycle.c` | Self-contained: real Wine/Proton/Bottles/Lutris/Flatpak work; shares the |
| `src/runtime/wubu_bottle_ops.c` | Self-contained: real Wine/Proton/Bottles/Lutris/Flatpak work; shares the |
| `src/runtime/wubu_bottle_serialize.c` | Owns the bottle JSON schema: dependency-type naming, save (emit) and load |
| `src/runtime/wubu_bottles_fs.c` | Extracted from wubu_bottles.c (separable leaf). Self-contained: ftw only. |
| `src/runtime/wubu_bottles_json.c` | wubu_bottle_save/load and other bottle ops. Types via wubu_bottles.h. |
| `src/runtime/wubu_bottles_test.c` | static void test_bottle_create_wine(void) { |
| `src/runtime/wubu_colonel.c` | static const char *const g_apps[] = { |
| `src/runtime/wubu_compat_db.c` | + shader-cache lesson). Real on-disk JSON store, no SQLite dependency. |
| `src/runtime/wubu_compat_db_test.c` | + shader-cache lesson). Real on-disk round-trip assertions. |
| `src/runtime/wubu_container.c` | CRC32 is in wubu_crypto.h (included above) */ |
| `src/runtime/wubu_container_test.c` | static int g_run = 0, g_pass = 0; |
| `src/runtime/wubu_ct_bwrap.c` | No chroot required  --  uses host filesystem with bind mounts. |
| `src/runtime/wubu_ct_isolate.c` | Not syscall emulation -- real Linux kernel isolation primitives. |
| `src/runtime/wubu_ct_isolate_cgroup.c` | API (create/set memory/cpu/pids/io/attach/destroy). Uses the cgroupfs |
| `src/runtime/wubu_dos_emu.c` | int wubu_dos_emu_exit_code(const WubuDosEmu *e) { return e ? e->exit_code : -1; } |
| `src/runtime/wubu_dos_emu_alu.c` | uint16_t reg_read(WubuDosEmu *e, int modrm, int w) { int r = (modrm >> 3) & 7; return w ?  |
| `src/runtime/wubu_dos_emu_decode.c` | int decode_main(WubuDosEmu *e, uint8_t op) { |
| `src/runtime/wubu_dos_emu_int.c` | void scroll_up(WubuDosEmu *e, int top, int left, int bot, int right, int lines, uint8_t at |
| `src/runtime/wubu_dos_emu_mem.c` | uint32_t phys(WubuDosEmu *e, uint16_t seg, uint16_t off) { |
| `src/runtime/wubu_dos_emu_regs.c` | uint16_t *regp(WubuDosEmu *e, int i) { |
| `src/runtime/wubu_dos_emu_smoke.c` | mov ah, 0x09      ; DOS print '$'-terminated string |
| `src/runtime/wubu_dos_emu_test.c` | string ops, INT 21h text output, and the RGBA frame producer. No external |
| `src/runtime/wubu_dos_proc.c` | WuBuOS as an ordinary process via the real 8086 interpreter (wubu_dos_emu), |
| `src/runtime/wubu_dos_proc_test.c` | / int 21h), runs it through wubu_dos_proc (no QEMU, no disk image), and |
| `src/runtime/wubu_dxvk_conf.c` | logic that was previously duplicated inline in both src/runtime/wubu_proton.c |
| `src/runtime/wubu_edr.c` | This shim exists for backward compatibility with the test target. |
| `src/runtime/wubu_edr_agent_test.c` | through wubu_ui (the SAME path a human uses), every action lands in the EDR |
| `src/runtime/wubu_edr_test.c` | printf("  TEST %-55s ", name); \ |
| `src/runtime/wubu_exec.c` | This replaces the old in-process syscall translation layer. |
| `src/runtime/wubu_exec_container.c` | Extracted from wubu_exec.c (separable leaf). Self-contained: decodes a |
| `src/runtime/wubu_exec_dos.c` | binaries and routes them to the in-process 16-bit compat shim (no second |
| `src/runtime/wubu_exec_format.c` | WUBU_PAYLOAD_TYPE / WUBU_EXEC_RESULT / WUBU_HEADER (wubu_exec.h) and the |
| `src/runtime/wubu_exec_macho.c` | Validates Mach-O magic, loads via VSL Mach-O loader if available, |
| `src/runtime/wubu_exec_wasm.c` | Extracted from wubu_exec.c (separable leaf). Self-contained: validates WASM |
| `src/runtime/wubu_fs_util.c` | creation, previously duplicated (byte-for-byte) as `rm_rf` in |
| `src/runtime/wubu_gc.c` | Kernel stays manual (memory.c)  --  this is purely userspace. |
| `src/runtime/wubu_gc_test.c` | static int g_pass = 0, g_fail = 0, g_total = 0; |
| `src/runtime/wubu_gdpr_age.c` | third-party verification. Self-declaration + knowledge question. |
| `src/runtime/wubu_hc_eval_stub.c` | binaries that link wubu_exec.c (which calls hc_eval for HolyC JIT exec) |
| `src/runtime/wubu_holyc_agi.c` | "default" session; bridges the HolyC Terminal and the AGI to the real |
| `src/runtime/wubu_holyc_agi_test.c` | Terminal and the AGI both author HolyC source that compiles + runs LIVE via |
| `src/runtime/wubu_holyd.c` | input routing, 9P namespace, auto-save, and desktop integration. |
| `src/runtime/wubu_holyd_9p.c` | int wubu_holyd_mount(WubuHoly *d, const char *session, const char *path) { |
| `src/runtime/wubu_holyd_event.c` | int wubu_holyd_publish_event(WubuHoly *d, const char *event_type, |
| `src/runtime/wubu_holyd_exec.c` | int wubu_holyd_eval(WubuHoly *d, const char *session, |
| `src/runtime/wubu_holyd_input.c` | The daemon stays decoupled from the WM. The composition root (hosted |
| `src/runtime/wubu_holyd_lifecycle.c` | int wubu_holyd_init(WubuHoly *d, const WubuHolyConfig *config) { |
| `src/runtime/wubu_holyd_repl.c` | read-eval-print loop (repl_start/repl_eval/repl_stop) and the per-session |
| `src/runtime/wubu_holyd_save.c` | int wubu_holyd_session_save(WubuHoly *d, const char *session) { |
| `src/runtime/wubu_holyd_session.c` | int wubu_holyd_session_create(WubuHoly *d, const char *name, |
| `src/runtime/wubu_holyd_test.c` | 9P namespace, auto-save, and event publishing. |
| `src/runtime/wubu_holyd_window.c` | int wubu_holyd_window_create(WubuHoly *d, const char *session, |
| `src/runtime/wubu_host_exec.c` | Arch base → SteamOS compat → rip through Linux drivers. |
| `src/runtime/wubu_host_exec_test.c` | All tests exercise REAL host process creation. |
| `src/runtime/wubu_hwdetect.c` | CPUID via inline asm (freestanding-safe); GPU probing via stat(). |
| `src/runtime/wubu_image.c` | - WuBuFile (Dockerfile-like) parsing |
| `src/runtime/wubu_image_cache.c` | CACHE_DIR. Uses WUBU_MAX_PATH (wubu_image.h). Minimal includes. |
| `src/runtime/wubu_image_manifest.c` | deserialization, save/load, and ID computation. |
| `src/runtime/wubu_image_ops.c` | tagging, removal, pruning, inspection, history, push/pull. |
| `src/runtime/wubu_image_parse.c` | logic moved here. The original wubu_image.c had the parser inline |
| `src/runtime/wubu_image_tar.c` | Extracted from wubu_image.c (separable leaf). Self-contained: only |
| `src/runtime/wubu_launch_test.c` | (container/Proton as the default Windows-launch path). Real assertions |
| `src/runtime/wubu_netlink.c` | operations and low-level network interface manipulation. |
| `src/runtime/wubu_network.c` | - WubuNetworkManager holds fixed arrays of WubuNetworkProfile and WubuEndpoint |
| `src/runtime/wubu_network_cni.c` | Extracted from wubu_network.c (separable leaf). Self-contained: uses the |
| `src/runtime/wubu_network_create.c` | Extracted from wubu_network.c (separable leaf). Self-contained: wubu_network_create |
| `src/runtime/wubu_network_dns.c` | Extracted from wubu_network.c (separable leaf). Self-contained: add/remove/query |
| `src/runtime/wubu_network_fw.c` | (wubu_network_internal.h) + WubuNetworkProfile/Manager (wubu_network.h). |
| `src/runtime/wubu_network_qos.c` | Extracted from wubu_network.c (separable leaf). Self-contained: tc-based QoS |
| `src/runtime/wubu_network_svc.c` | find_endpoint/find_network resolvers (declared in wubu_network_internal.h) |
| `src/runtime/wubu_network_test.c` | preset creation, endpoints, port maps, DNS, QoS, firewall, |
| `src/runtime/wubu_network_ts.c` | Extracted from wubu_network.c (separable leaf). Self-contained: uses the |
| `src/runtime/wubu_network_wg.c` | Extracted from wubu_network.c (separable leaf). Self-contained: uses the |
| `src/runtime/wubu_ns_9p_test.c` | exports that same dir over 9P. This test acts as a 9P *client* (using the |
| `src/runtime/wubu_ns_bridge.c` | exposed as a uniform 9P/Styx control plane (rip off systemd/Flatpak/ |
| `src/runtime/wubu_ns_bridge_test.c` | routing (rip-off-systemd play) WITHOUT shelling out to arch-chroot/ |
| `src/runtime/wubu_ns_fs.c` | and file-write helpers, and wubu_ns_bridge_create() that lays down /n/svc |
| `src/runtime/wubu_ns_kernel.c` | (rip off CachyOS kernel-manager / chwd, do it better through /n). |
| `src/runtime/wubu_ns_kernel_test.c` | CachyOS kernel-manager / chwd through /n). Uses an injected mock GPU |
| `src/runtime/wubu_ns_pkg.c` | plane (rip off pacman/Chaotic-AUR "prebuilt binaries, no local compile" |
| `src/runtime/wubu_ns_pkg_test.c` | pacman/Chaotic-AUR through /n). Drives the REAL wubu_pkg_* API so routing |
| `src/runtime/wubu_ns_snap.c` | plane (rip off snapper/btrfs rollback, do it better through /n). |
| `src/runtime/wubu_ns_snap_test.c` | snapper/btrfs rollback through /n). |
| `src/runtime/wubu_oci_test.c` | static void test_media_types(void) { |
| `src/runtime/wubu_pkg.c` | void pkg_init(PkgManager *mgr) { |
| `src/runtime/wubu_proton.c` | WuBuOS -> VSL -> Proton -> Windows PE |
| `src/runtime/wubu_proton2.c` | as the PE parser. This module handles: |
| `src/runtime/wubu_proton2_device.c` | Extracted from wubu_proton2.c (separable leaf). Self-contained: scans /dev + /sys |
| `src/runtime/wubu_proton2_gamescope.c` | Extracted from wubu_proton2.c (separable leaf). Self-contained: builds a |
| `src/runtime/wubu_proton2_gpu.c` | open /dev/dri. Minimal includes. |
| `src/runtime/wubu_proton2_launch.c` | Extracted from wubu_proton2.c (separable leaf). Self-contained: builds the |
| `src/runtime/wubu_proton2_test.c` | static void test_gpu_detect(void) { |
| `src/runtime/wubu_proton_api.c` | table, built-in DLL catalog, and the register/translate/load-default APIs). |
| `src/runtime/wubu_proton_dll.c` | proton_dll_t / proton_dll_type_t (wubu_proton.h). Minimal includes. |
| `src/runtime/wubu_proton_dxvk.c` | API used by both the runtime (VSL-proton layout) and the GUI |
| `src/runtime/wubu_proton_pe.c` | PE type/define vocabulary from wubu_proton.h (PE_MAGIC, pe_coff_header_t, |
| `src/runtime/wubu_proton_test.c` | PE execution pipeline, and diagnostics. |
| `src/runtime/wubu_ramdisk.c` | Bare metal mode: SSD at /var/wubu/roots/arch-base → persistent, real OS |
| `src/runtime/wubu_ramdisk_format.c` | Extracted from wubu_ramdisk.c (separable leaf). Self-contained: stat + |
| `src/runtime/wubu_ramdisk_test.c` | or running pacstrap (needs root + network). We validate: |
| `src/runtime/wubu_realm.c` | Lifecycle via the supervisor (N1-N4/N8/N9); boundary EDR via an immutable |
| `src/runtime/wubu_selfimprove.c` | Independent verifier + human gate + freeze + failure-weighting. |
| `src/runtime/wubu_session.c` | dedicated GAME mode. In GAME mode the shell chrome is bypassed and the |
| `src/runtime/wubu_snapshot.c` | - WubuSnapshotManager holds fixed arrays of WubuSnapshot, WubuBranch, WubuTag |
| `src/runtime/wubu_snapshot_copy.c` | Extracted from wubu_snapshot.c (separable leaf). Self-contained: filesystem + |
| `src/runtime/wubu_snapshot_diff.c` | + wubu_snapshot_type_str (wubu_snapshot_internal.h). Minimal includes. |
| `src/runtime/wubu_snapshot_fs.c` | snapshot creation and overlayfs mount/unmount logic. |
| `src/runtime/wubu_snapshot_gc.c` | Extracted from wubu_snapshot.c (separable leaf). Self-contained: applies |
| `src/runtime/wubu_snapshot_tag.c` | snapshot_now (static inline in wubu_snapshot_internal.h) and the |
| `src/runtime/wubu_snapshot_test.c` | tagging, export/import, diff, rollback, restore, GC, tree. |
| `src/runtime/wubu_snapshot_xport.c` | Extracted from wubu_snapshot.c (separable leaf). Self-contained: serialize a |
| `src/runtime/wubu_spawn.c` | so any target can link this without pulling in container/compiler code. |
| `src/runtime/wubu_spawn_test.c` | returns real exit codes, and that no /bin/sh is ever spawned (so shell |
| `src/runtime/wubu_system.c` | root is a single "container" tracked by the manager; each commit is a |
| `src/runtime/wubu_system_test.c` | snapshot manager: commit creates a read-only baseline, active label tracks |
| `src/runtime/wubu_trace.c` | Immutable, versioned, user-owned trace store with grepable plain-text mirror. |
| `src/runtime/wubu_uuid.c` | for 74 bits of randomness (only used once at startup — subsequent UUIDs |
| `src/runtime/wubu_uuid_test.c` | Test 1: Generate UUIDv7, verify format */ |
| `src/runtime/wubu_verifier_bytropix.c` | INDEPENDENT verifier for the Mega-OS self-improvement loop (DA-3). |
| `src/runtime/wubu_vsl.c` | list. All actual VSL implementation lives in the vsl/ submodules. |
| `src/runtime/wubu_vsl_test.c` | static int g_run = 0, g_pass = 0; |
| `src/bridge/bridge.c` | static BridgeMode g_mode = MODE_GUI; |
| `src/bridge/bridge_test.c` | mode switching, clipboard, and IPC. |
| `src/bridge/vbe_ws_bridge.c` | so WorldSim renders directly into VBE. After rendering, |
| `src/bridge/vbe_ws_bridge_test.c` | - Bridge init/wire/unwire lifecycle |
| `src/bridge/wubu_syscall.c` | Provides trampolines for HolyC compiler to call into kernel. |
| `src/bridge/wubu_syscall_test.c` | Tests the C handler functions directly (not via kernel syscall). |
| `src/bridge/wubu_syscall_vbe.c` | Declared in wubu_syscall.h; vbe_* in kernel/vbe.h. Minimal includes. |
| `src/gui/dosgui_cp_sound.c` | C11. Implements the header-declared dosgui_cp_create_sound_applet() |
| `src/gui/dosgui_daemon_panel.c` | desktop. Shows daemon status in the system tray, container list in a |
| `src/gui/dosgui_daemon_panel_test.c` | We provide weak stubs for all GUI functions the panel calls, |
| `src/gui/dosgui_desktop.c` | ZealOS kernel runs in-process, Fable sauce renders the desktop, |
| `src/gui/dosgui_dos_window.c` | window. The window blits the guest's captured VGA framebuffer (RGBA from |
| `src/gui/dosgui_dos_window_test.c` | 1. launch a real 16-bit .COM via wubu_dos_proc_launch (in-process 8086), |
| `src/gui/dosgui_dos_window_test_stub.c` | DOS Box window test. We are testing the DOS-window glue, not the WM |
| `src/gui/dosgui_era_apps.c` | each tagged with the VSL syscall personality it exercises so the WuBuOS |
| `src/gui/dosgui_era_apps_test.c` | registry + launcher. Asserts that each RUNNABLE era app actually executes |
| `src/gui/dosgui_explorer.c` | - Tree view sidebar (folder hierarchy, expand/collapse) |
| `src/gui/dosgui_explorer_drives.c` | (ex_9p_opendir/ex_9p_stat, declared in dosgui_explorer_internal.h). |
| `src/gui/dosgui_explorer_format.c` | ONCE here, so every submodule (fs, fsops, zip, render, tree, drives) links |
| `src/gui/dosgui_explorer_fs.c` | 9P/Styx shim (ex_9p_*) that maps POSIX-style file ops onto the Styx 9P |
| `src/gui/dosgui_explorer_fsops.c` | move / delete + ex_handle_file_op dispatcher. Uses the shared g_explorer |
| `src/gui/dosgui_explorer_info.c` | Extracted from dosgui_explorer.c (separable leaf). Self-contained: stat + |
| `src/gui/dosgui_explorer_input.c` | handle_key ~220 lines, handle_mouse ~195 lines).  Input is a distinct |
| `src/gui/dosgui_explorer_ops.c` | void dosgui_explorer_copy(void) { |
| `src/gui/dosgui_explorer_preview.c` | selected entry. Uses g_explorer (extern), ex_get_extension (dosgui_explorer_tree.c), |
| `src/gui/dosgui_explorer_render.c` | - Tree view sidebar render (ex_draw_tree_node, ex_render_tree) |
| `src/gui/dosgui_explorer_test.c` | Builds a minimal but spec-valid uncompressed (STORE) ZIP at `path` |
| `src/gui/dosgui_explorer_test_stub.c` | so the explorer test can compile and run without the full WM/Kernel. |
| `src/gui/dosgui_explorer_tree.c` | (extension, case-insensitive compare, sort, file compare) used across the |
| `src/gui/dosgui_explorer_zip.c` | Self-contained ZIP central directory parsing + libzip dlopen wrapper |
| `src/gui/dosgui_service_mgr.c` | See dosgui_service_mgr.h for the contract. |
| `src/gui/dosgui_service_mgr_test.c` | wubu_archd wired as the Desktop's autostart/service manager. |
| `src/gui/dosgui_startmenu.c` | Win98-style: Start -> Programs -> {Accessories, WuBuOS, System} |
| `src/gui/dosgui_startmenu_db.c` | static main-menu item list and the category submenus (Programs -> |
| `src/gui/dosgui_startmenu_power.c` | the power-options render. Uses g_open (extern in dosgui_startmenu_internal.h) |
| `src/gui/dosgui_startmenu_search.c` | shared g_search / g_recent / g_program_db state (declared extern in |
| `src/gui/dosgui_startmenu_test.c` | static int g_pass = 0, g_fail = 0, g_total = 0; |
| `src/gui/dosgui_startmenu_test_stub.c` | int dosgui_taskbar_height(void) { |
| `src/gui/dosgui_startmenu_tree.c` | shared g_tree_* state (extern in dosgui_startmenu_internal.h). Minimal includes. |
| `src/gui/dosgui_term.c` | - PTY backend for shell sessions (bash, zsh, etc.) |
| `src/gui/dosgui_term_ansi.c` | parsers (term_process_pty_output ~240 lines, term_process_container_output |
| `src/gui/dosgui_term_pty.c` | container PTY, input handling, and I/O operations. |
| `src/gui/dosgui_term_render.c` | content area, and PTY/holyc/container session views. Uses the shared g_term |
| `src/gui/dosgui_term_tabs.c` | default-shell resolver. Uses TermState/TermTab (dosgui_term.h) and g_term via |
| `src/gui/dosgui_term_test.c` | HolyC REPL, keyboard shortcuts, and resize handling. |
| `src/gui/dosgui_term_test_stub.c` | so the terminal test can compile and run without the full WM/Kernel. |
| `src/gui/dosgui_window_chrome.c` | - Window frame/border (3D raised/sunken) |
| `src/gui/dosgui_wm.c` | Ports ZealOS/WuBuDos bare-metal window management into WuBuOS. |
| `src/gui/dosgui_wm_clock.c` | clock update + string formatting. Depends only on the shared WM state |
| `src/gui/dosgui_wm_ctxmenu.c` | to the desktop + icon domains. Owns the default context actions (Open/Play/ |
| `src/gui/dosgui_wm_ctxmenu_engine.c` | context-menu stack, lifecycle (create/add/show/hide), mouse dispatch, |
| `src/gui/dosgui_wm_desktop.c` | snapping math, auto-arrange / sort / refresh of desktop icons, and real |
| `src/gui/dosgui_wm_holyc_term.c` | Extracted from dosgui_wm.c for modularity. |
| `src/gui/dosgui_wm_icon_glyphs.c` | recognizable shape. That is the "mess": a desktop full of identical |
| `src/gui/dosgui_wm_icons.c` | URL-shortcut creation, hit-testing, and icon lookup. Uses g_dwm (extern in |
| `src/gui/dosgui_wm_input.c` | - dosgui_wm_handle_key(): Alt+Tab cycling, Win-key hotkeys, theme |
| `src/gui/dosgui_wm_layout.c` | int title_bar_height(void) { return theme()->Luna_start_button ? 24 : DOSGUI_TITLE_H; } |
| `src/gui/dosgui_wm_render.c` | background, icons, every live window (chrome + on_draw content), the |
| `src/gui/dosgui_wm_systray.c` | SYSTEM TRAY / NOTIFICATION AREA |
| `src/gui/dosgui_wm_taskbar.c` | systray) using the shared g_dwm state (extern in dosgui_wm_internal.h) and |
| `src/gui/dosgui_wm_test.c` | Tests window creation, z-order, drag, focus, taskbar, icons. |
| `src/gui/dosgui_wm_test_stub.c` | the WM and context-menu engine call. These let unit/integration tests link |
| `src/gui/dosgui_wm_window.c` | - Window lifecycle: spawn / close / raise / destroy / focus / lookup |
| `src/gui/dosgui_wm_window_state.c` | state transitions (resize / move / maximize / minimize / restore) and the |
| `src/gui/gui_dbuf.c` | Minimal 8x8 font: digits 0-9, A-Z, space, colon, dash, period, slash */ |
| `src/gui/gui_dbuf_test.c` | Win98 borders, dirty rect tracking, and flip. |
| `src/gui/standalone_hosted_shim.c` | hosted binary (src/hosted/hosted.o, which carries main() and is therefore |
| `src/gui/test_dosgui_cp_sound.c` | extern wubu_sound_t g_sound_engine; |
| `src/gui/test_synth.c` | printf("=== test_synth (WT-A oscillators, WT-B ladder) ===
"); |
| `src/gui/test_wubu_sound.c` | printf("=== test_wubu_sound ===
"); |
| `src/gui/wubu_clipboard.c` | Phase 2: Wayland data device + primary selection |
| `src/gui/wubu_clipboard_mime.c` | clear/get MIME entries. Take the mime array by pointer (no shared global). |
| `src/gui/wubu_clipboard_test.c` | Tests internal clipboard logic without Wayland integration |
| `src/gui/wubu_clipboard_wl.c` | listeners and drag-and-drop handlers, ~350 LOC).  This is the protocol/ |
| `src/gui/wubu_compositor.c` | Implements the API defined in wubu_compositor.h. |
| `src/gui/wubu_compositor_standalone.c` | This is the hosted version that runs on an existing Wayland compositor. |
| `src/gui/wubu_compositor_test.c` | printf("=== WuBuOS Compositor Test ===
"); |
| `src/gui/wubu_deploy.c` | Uses system tools: mkinitcpio, tar, docker/buildah, xcodebuild |
| `src/gui/wubu_deploy_config.c` | Extracted from wubu_deploy.c (separable leaf). Self-contained: pure config |
| `src/gui/wubu_deploy_gen.c` | macOS entitlements + Info.plist writers. Uses the shared wubu_deploy API |
| `src/gui/wubu_deploy_test.c` | fprintf(stderr, "FAIL: %s (%s:%d)
", msg, __FILE__, __LINE__); \ |
| `src/gui/wubu_deploy_util.c` | write_file, mkdir_p, copy_file. Uses the public wubu_deploy API types via |
| `src/gui/wubu_gamelib.c` | GameLibraryState g_gamelib = {0}; |
| `src/gui/wubu_gamelib_config.c` | Extracted from wubu_gamelib.c (separable leaf). Self-contained: serializes the |
| `src/gui/wubu_gamelib_playtime.c` | Extracted from wubu_gamelib.c (separable leaf). Self-contained: records/queries |
| `src/gui/wubu_gamelib_scan.c` | custom-dir scanners + the shared id/size/sort-name helpers. Uses the public |
| `src/gui/wubu_gamelib_startmenu.c` | Extracted from wubu_gamelib.c (separable leaf). Self-contained: builds/clears |
| `src/gui/wubu_gamelib_test.c` | printf("Testing Game Library...
"); |
| `src/gui/wubu_json.c` | arena + state declared in wubu_json.h. Minimal includes. |
| `src/gui/wubu_ladder.c` | Four one-pole stages + resonance feedback + tanh saturation, in the |
| `src/gui/wubu_mime.c` | static MimeSystem g_mime = {0}; |
| `src/gui/wubu_mime_desktop.c` | (str_lower/trim/dup_trim/endswith, get_file_extension) + parse_desktop_file. |
| `src/gui/wubu_mime_test.c` | printf("Testing MIME system...
"); |
| `src/gui/wubu_notify.c` | Phase 2: libnotify-compatible notification server |
| `src/gui/wubu_pkgmgr.c` | wubu_pkgmgr_pkg.c     - package create/verify/extract/read-manifest |
| `src/gui/wubu_pkgmgr_db.c` | repo/installed list load callbacks. State via g_pkgmgr (extern in |
| `src/gui/wubu_pkgmgr_install.c` | Repo management, search and index queries live in the facade; this module |
| `src/gui/wubu_pkgmgr_manifest.c` | Extracted from wubu_pkgmgr.c (separable leaf). Self-contained: stdlib only. |
| `src/gui/wubu_pkgmgr_pkg.c` | (struct definition lives in wubu_pkgmgr_internal.h) |
| `src/gui/wubu_pkgmgr_remote.c` | oci_http_client.c), parses it into the local repo_packages table, and |
| `src/gui/wubu_pkgmgr_resolve.c` | Extracted from wubu_pkgmgr.c (separable leaf). Self-contained: builds a sorted |
| `src/gui/wubu_pkgmgr_test.c` | fprintf(stderr, "FAIL: %s (%s:%d)
", msg, __FILE__, __LINE__); \ |
| `src/gui/wubu_pkgmgr_txn.c` | bool wubu_pkgmgr_txn_begin(wubu_pkg_transaction_t* txn, bool dry_run) { |
| `src/gui/wubu_pkgmgr_verify.c` | Extracted from wubu_pkgmgr.c (separable leaf). Self-contained: queries the |
| `src/gui/wubu_proton.c` | strncpy((dst), (src), (dst_size) - 1); \ |
| `src/gui/wubu_proton_config.c` | Uses the shared g_proton state + Proton API via wubu_proton_internal.h. |
| `src/gui/wubu_proton_dxvk.c` | This file owns ONLY the GUI-specific concerns: |
| `src/gui/wubu_proton_exec.c` | winecmd/regedit/winetricks, game launch, GE-proton install + version. |
| `src/gui/wubu_proton_test.c` | printf("Testing Proton subsystem...
"); |
| `src/gui/wubu_proton_util.c` | helpers, Steam path detection, VDF/appmanifest parsing. Uses the shared |
| `src/gui/wubu_screenshot.c` | static char g_screenshot_dir[PATH_MAX]; |
| `src/gui/wubu_screenshot_clipboard_test.c` | Regression test for wubu_screenshot_to_clipboard (was a no-op returning |
| `src/gui/wubu_screenshot_draw.c` | draw_arrow. Operate on a uint32_t framebuffer. Declared in |
| `src/gui/wubu_screenshot_png.c` | + the PNG writer (CRC, chunking, RGBA encoder). Uses the public |
| `src/gui/wubu_screenshot_test.c` | Parse big-endian uint32 from a PNG buffer offset. */ |
| `src/gui/wubu_session.c` | Phase 2: Session management, auto-start, shutdown dialog |
| `src/gui/wubu_session_autostart.c` | autostart files, saves/loads/restores them. The json_read_* helpers are shared |
| `src/gui/wubu_settings.c` | default factories live in wubu_settings_defaults.c; JSON (de)serialization |
| `src/gui/wubu_settings_defaults.c` | state (g_settings / g_settings_dirty), the config-path resolution, and the |
| `src/gui/wubu_settings_io.c` | helpers, the per-section savers, and the file load/save entry points. |
| `src/gui/wubu_sound.c` | Pure PCM synthesis -- no assets, no third-party. |
| `src/gui/wubu_theme.c` | Win98 Classic, XP Luna Blue, XP Media Center Orange/Black. |
| `src/gui/wubu_trash.c` | static TrashState g_trash = {0}; |
| `src/gui/wubu_trash_test.c` | printf("Testing Trash system...
"); |
| `src/gui/wubu_ui.c` | the identical entry points a real human's input device feeds. This keeps the |
| `src/gui/wubu_ui_test.c` | desktop through the SAME input path a human uses -- move windows, close |
| `src/gui/wubu_wallpaper.c` | into an XRGB8888 buffer compatible with the VBE framebuffer. Implements the |
| `src/gui/wubu_wallpaper_test.c` | and asserts the pixels land correctly (XRGB8888). Also checks the five |
| `src/gui/wubu_waveosc.c` | The tables are precomputed by additive harmonic sums; the read path |
| `src/gui/wubu_wayland_stub.c` | `g_wl` (wayland_state_t), which is fully populated only by the hosted |
| `src/gui/wubu_welcome.c` | shortcuts and a "Don't show again" button. Creates a marker file at |
| `src/gui/wubu_wm.c` | Sub-modules: wubu_wm_desktop.c, wubu_wm_input.c, wubu_wm_render.c |
| `src/gui/wubu_wm_desktop.c` | Supports 1-9 desktops with switch/next/prev/move operations. |
| `src/gui/wubu_wm_input.c` | Routes events to windows, handles title bar buttons, |
| `src/gui/wubu_wm_render.c` | taskbar, GAAD snap previews, and virtual desktop indicators. |
| `src/gui/wubu_wm_test.c` | static void test_theme_init(void) { |
| `src/gui/xdg-shell-client-protocol.c` | Generated by wayland-scanner 1.22.0 */ |
| `src/compiler/holyc_codegen.c` | Modular codegen: emit, expr, stmt, api submodules. |
| `src/compiler/holyc_codegen_api.c` | Top-level compile/eval functions and public interface. |
| `src/compiler/holyc_codegen_emit.c` | Low-level byte emission, instruction patterns, patching utilities. |
| `src/compiler/holyc_codegen_expr.c` | Generates x86-64 machine code for HolyC AST expressions. |
| `src/compiler/holyc_codegen_stmt.c` | Generates x86-64 machine code for HolyC AST statements. |
| `src/compiler/holyc_lexer.c` | Tokenizes HolyC source text into a stream of tokens. |
| `src/compiler/holyc_parse.c` | Ported from ZealOS/src/Compiler/ParseExp.ZC + ParseStatement.ZC |
| `src/compiler/holyc_parse_ast.c` | (holyc_parse.h). Minimal includes. |
| `src/compiler/holyc_ptx.c` | Targets NVIDIA Volta/Ampere/Hopper via MMA instructions. |
| `src/compiler/holyc_runtime.c` | era demo and the desktop HolyC terminal actually call: |
| `src/compiler/holyc_test.c` | static int g_run = 0, g_pass = 0; |
| `src/compiler/test_holyc_ptx.c` | static int g_pass = 0, g_fail = 0; |
| `src/shell/wubu_shell.c` | loop (line editing with history recall + tab completion). The heavy |
| `src/shell/wubu_shell_complete.c` | command table. Returns the longest common prefix (the canonical readline |
| `src/shell/wubu_shell_exec.c` | (BATTLESHIP gaps 230 shell_pipe / 231 shell_redirect) |
| `src/shell/wubu_shell_history.c` | exposed via shell_history_prev()/shell_history_next(); the REPL driver |
| `src/shell/wubu_shell_test.c` | 228-231): command history, tab completion, pipelines, and I/O redirection. |
| `src/framework/test_bonzi_comfy.c` | 1. Bonzi parses intent and routes to REAL OS plumbing: |
| `src/framework/wubufx.c` | and the EDR engine (wubu_edr.*) into a namespace-first app model. |
| `src/framework/wubufx_apps.c` | WuBuFX. Each app is a content-addressed, capability-scoped, EDR-disclosed |
| `src/framework/wubufx_apps_test.c` | wubufx_app_launch (no placeholder bodies survive). Also verifies the app |
| `src/framework/wubufx_test.c` | - mount resolves a content-addressed app namespace |
| `src/bear/bear_arena.c` | BearArena g_bear_rollout_arena; |
| `src/bear/bear_cudnn.c` | Compiled with: nvcc -c bear_cudnn.c -o bear_cudnn.o -lcublas -lcudnn |
| `src/bear/bear_cudnn_cublas.c` | void hc_builtin_cublas_destroy(BearCublasHandle handle) { |
| `src/bear/bear_cudnn_cuda.c` | void* hc_builtin_cuda_malloc(size_t bytes) { |
| `src/bear/bear_env.c` | Episode step/return scratch buffers (shared with subsystems via |
| `src/bear/bear_env_npole.c` | Self-contained physics: Recursive Lagrangian dynamics + RK4 integrator for |
| `src/bear/bear_nn_ckpt.c` | Checkpointing — binary serialization of policy network state |
| `src/bear/bear_nn_policy.c` | (MLP/minGRU create, forward, sample, param get/set, orthogonal init). |
| `src/bear/bear_nn_value.c` | (create, forward, orthogonal init, backward passes, zero-grad). |
| `src/bear/bear_opt.c` | Optimizer Creation / Registration |
| `src/bear/bear_opt_test.c` | do REAL work (previously they were no-op stubs): |
| `src/bear/bear_ppo_loss.c` | gradient application (extracted from the monolithic bear_ppo.c). |
| `src/bear/bear_ppo_trainer.c` | and checkpoint save/load (extracted from the monolithic bear_ppo.c). |
| `src/bear/bear_ppo_traj.c` | and minibatch sampler (extracted from the monolithic bear_ppo.c). |
| `src/bear/bear_train.c` | No Python, no PyTorch, no Gym. Pure metal. |
| `src/bear/bear_vulkan_soft.c` | All "tensor" ops run on CPU via bear_nn functions. |
| `src/firmware/fw_acpi.c` | generates them itself and hands them over through fw_cfg. We locate the |
| `src/firmware/fw_acpiload.c` | the ACPI tables in guest memory: |
| `src/firmware/fw_agi.c` | learning platform": a tiny trusted ring-0 shim that (1) publishes a live |
| `src/firmware/fw_ahci.c` | SATA disk is reached; the ATA PIO path in fw_ata.c only covers legacy IDE. |
| `src/firmware/fw_ata.c` | ports, which is the one storage path guaranteed present without PCI |
| `src/firmware/fw_block.c` | no legacy IDE (any q35 / real modern board) had no boot volume at all. All |
| `src/firmware/fw_bs_mem.c` | extern EFI_EVENT g_wait_for_key; |
| `src/firmware/fw_bs_proto.c` | EFI_STATUS fw_efi_uninstall(EFI_HANDLE h, EFI_GUID *guid); |
| `src/firmware/fw_con.c` | void fw_vga_set_attr(uint8_t a); |
| `src/firmware/fw_drivers.c` | TCG-required measurements that are not tied to a single stage (GPT into |
| `src/firmware/fw_e1000.c` | PCI bus), so a real driver makes the firmware network-capable rather than |
| `src/firmware/fw_fsproto.c` | pretending to succeed. A boot loader only needs Open/Read/SetPosition/ |
| `src/firmware/fw_fwcfg.c` | hands them over through fw_cfg together with a linker/loader script telling |
| `src/firmware/fw_gop.c` | console. QEMU's std/virtio VGA exposes a linear framebuffer through BAR0 |
| `src/firmware/fw_guid.c` | EFI_GUID gEfiLoadedImageProtocolGuid = |
| `src/firmware/fw_handle.c` | HandleProtocol/LocateHandle can validate them instead of dereferencing |
| `src/firmware/fw_lib.c` | Console output goes to both COM1 (16550) and the VGA text buffer at |
| `src/firmware/fw_main.c` | Brings up console, memory, timing, storage, EFI tables, then loads and |
| `src/firmware/fw_media.c` | ESP. This is a self-contained FAT reader (no reuse of the kernel's fat32 |
| `src/firmware/fw_mem.c` | allocator is map-first: a fixed table of typed regions is the source of |
| `src/firmware/fw_nvme.c` | the first namespace and do LBA reads/writes, which is what a firmware boot |
| `src/firmware/fw_pci.c` | BARs, so enumeration is the foundation of the driver layer. Uses legacy |
| `src/firmware/fw_pcires.c` | powers up with every BAR at zero and expects the firmware to program the |
| `src/firmware/fw_pe.c` | preferred base when free (otherwise anywhere), copies sections, zeroes the |
| `src/firmware/fw_rt.c` | -bios path), which is spec-legal as volatile storage: NON_VOLATILE |
| `src/firmware/fw_secureboot.c` | *authenticated* boot: the firmware refuses to execute an EFI image whose |
| `src/firmware/fw_sha256.c` | runtime. Used for PCR extends and for hashing loaded EFI images. |
| `src/firmware/fw_shell.c` | manager) pokes at devices, reads files, and launches images. Ours supports |
| `src/firmware/fw_table.c` | extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL g_conout; |
| `src/firmware/fw_time.c` | Stall() is real microseconds rather than a spin guess. |
| `src/firmware/fw_tpm.c` | trust: a real TPM, a firmware that measures each stage into PCRs before |
| `src/firmware/fw_tpmlog.c` | verifier replays the log, recomputes each extend, and checks the result |
| `src/firmware/fw_xhci.c` | context base array and the command ring, and report attached root-hub |
| `src/hosted/hosted.c` | Runs as a regular Linux program via Wayland window. |
| `src/hosted/hosted_pe.c` | with the launch layer. Following the SteamOS strategy (Windows runs in a |
| `src/hosted/hosted_render.c` | - hosted_render_desktop(): composite desktop + start menu + tick into |
| `src/hosted/hosted_run.c` | sequence, the blit shim, mode switching, and the behavioral-test accessors |
| `src/hosted/hosted_styxfs.c` | Shared filesystem state (declared extern in hosted_internal.h). */ |
| `src/hosted/hosted_test.c` | Styx server callbacks (attach, walk, open, read, stat), |
| `src/hosted/hosted_wayland.c` | Thin orchestration: owns the public hosted_wl_* entry points declared in |
| `src/hosted/hosted_wayland_input.c` | Per-device input state (owned by this module). */ |
| `src/hosted/hosted_wayland_shm.c` | SHM double-buffered pool state (owned by this module). */ |
| `src/hosted/hosted_wayland_surface.c` | Primary selection device manager global (owned by this module). */ |
| `src/hosted/primary-selection-private.c` | Generated by wayland-scanner 1.22.0 */ |
| `src/hosted/wubu_display.c` | Fallback: WSL2, nested X11, dev environments where DRM isn't available. |
| `src/hosted/wubu_display_test.c` | printf("=== wubu_drm_direct test ===
"); |
| `src/hosted/wubu_gbm.c` | Uses Linux DRM/KMS dumb buffers directly via ioctls. |
| `src/hosted/wubu_metal.c` | Kernel subsystems for bare-metal */ |
| `src/hosted/wubu_metal_audio.c` | variants), each with init/shutdown/submit/cpu_load. Dispatched by |
| `src/hosted/wubu_metal_drm.c` | helpers, connector/CRTC/plane property enumeration, dumb-fb creation, |
| `src/hosted/wubu_metal_evdev.c` | Mirror of the original wubu_metal.c include set (proven to compile) plus the |
| `src/hosted/wubu_metal_test.c` | Stub: wubu_shell_run is not testable in isolation (requires full GUI stack) */ |
| `src/hosted/wubu_metal_vulkan.c` | Self-contained: forward-declares Vulkan types locally (mirrors original |
| `src/hosted/wubu_metal_x11.c` | Mirror of the original wubu_metal.c include set (proven to compile) plus the |
| `src/hosted/wubu_vulkan_cmd.c` | (extracted from the monolithic wubu_vulkan.c). Self-contained; depends only |
| `src/hosted/wubu_vulkan_compute.c` | memory-type utilities (extracted from the monolithic wubu_vulkan.c). |
| `src/hosted/wubu_vulkan_loader.c` | physical-device selection, and logical-device creation/destroy. |
| `src/hosted/wubu_vulkan_swapchain.c` | from the monolithic wubu_vulkan.c). Self-contained; depends only on the |
| `src/hosted/wubu_vulkan_util.c` | memory-type lookup. Self-contained; depends only on the public wubu_vulkan.h |
| `src/apps/app_canvas.c` | to a DosGuiWindow. Provides a Win98/Photoshop-class UI: toolbar, color |
| `src/apps/app_explorer.c` | (dosgui_explorer.c + ex_render_*) to a DosGuiWindow. The engine already |
| `src/apps/canvas_standalone.c` | editor (the real, layered wubu_canvas engine — Photoshop-class, not the |
| `src/apps/control_test.c` | Cell 395: Win98-style settings panel. Verifies the Desktop tab |
| `src/apps/dosgui_apps.c` | and startmenu both consume this table; there is no second hardcoded list. |
| `src/apps/dosgui_apps_test.c` | - Task Manager (Windows 11 style) |
| `src/apps/dosgui_apps_test_stubs.c` | points (explorer, cmd, edr, dos-proc, notify, wallpaper, session, compat, |
| `src/apps/edr_dash.c` | The OS does not sandbox the AGI; it makes everything the OS AND the AGI do |
| `src/apps/notepad.c` | buffer with a cursor, typing, backspace, enter, and arrow navigation, plus a |
| `src/apps/repl.c` | Uses the HolyC compiler (hc_eval) for evaluation |
| `src/apps/wubu_apps2_test.c` | static void test_ed_create(void) { |
| `src/apps/wubu_canvas_blend.c` | overlay/etc) with opacity. Uses WubuBlendMode / BLEND_* (wubu_canvas.h). |
| `src/apps/wubu_canvas_draw.c` | WubuLayer types and the public wubu_cv_* drawing/selection API. Mutations |
| `src/apps/wubu_canvas_filter.c` | (blur, sharpen, edge, invert, threshold, grayscale). |
| `src/apps/wubu_canvas_io.c` | Self-contained: depends only on the public canvas API (wubu_canvas.h), |
| `src/apps/wubu_canvas_io_ppm.c` | uses wubu_cv_composite/resize (wubu_canvas.h). No external libs, no shared |
| `src/apps/wubu_canvas_layers.c` | types, wubu_blend for compositing, public wubu_cv_* layer API). Minimal |
| `src/apps/wubu_canvas_plugin.c` | WubuPlugin types and the public wubu_cv_plugin_* API. Plugins are stored |
| `src/apps/wubu_canvas_transform.c` | geometric layer transforms (resize, crop, flip H/V, rotate 90). |
| `src/apps/wubu_canvas_undo.c` | exposes wubu_cv__undo_push() -- the single internal seam the drawing and |
| `src/apps/wubu_codec.c` | Always works on Arch (ffmpeg in community repo). |
| `src/apps/wubu_editor.c` | WubuSyntax wubu_ed_detect_syntax(const char *filename) { |
| `src/apps/wubu_editor_bookmark.c` | wubu_editor.h) and the WubuEditor/WubuEdTab types. Minimal includes. |
| `src/apps/wubu_editor_find.c` | replace-next / replace-all over the active tab's lines, with wrap-around |
| `src/apps/wubu_editor_macro.c` | Extracted from wubu_editor.c (separable leaf). Self-contained: uses the |
| `src/apps/wubu_editor_selection.c` | selection deletion, and cut/copy/paste against the editor clipboard. |
| `src/apps/wubu_editor_undo.c` | Mirrors the sibling-module convention used by wubu_editor_bookmark.c and |
| `src/apps/wubu_image_codec.c` | PNG chunk emission, and PNG adaptive-unfilter. Extracted from the monolithic |

## Tests (make targets)

`test_acpi, test_agi_kernel, test_agi_metal, test_ahci, test_ahcifat, test_anticheat, test_apps, test_apps2, test_arch, test_archd, test_as, test_audio, test_bear_opt, test_blk, test_bonzi_comfy, test_bonzi_study, test_bottles, test_bridge, test_bridge_flip, test_bytropix_verifier, test_calc, test_cap, test_clipboard, test_cmd, test_colonel, test_compat, test_compositor, test_container_registry, test_control, test_critical_kernel, test_critical_runtime, test_daemon_panel, test_dbuf, test_deploy, test_dos_emu, test_dos_emu_smoke, test_dos_proc, test_dosgui_apps, test_dosgui_cp_sound, test_dosgui_dos_window` — run the full gate with `make test_all` (subset) / the
target's own `make <target>`.

## Research ledger

| Doc | Topic |
|---|---|


## Build

```bash
make <target>     # any target above; C11, GCC/Clang, optional nvcc
make test_all     # full test gate (see Makefile target lists)
```

*Repo hygiene: 540 C modules, 1 test tools, 0 research docs (auto-audited — run `tools/repodoc/repodoc.py .` to refresh).*
<!-- repodoc:END -->
