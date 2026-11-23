# WuBuOS / WuBuKernel / WEMU usability gap map (2026-11-23)

This is the contract for the next work cycles. It replaces "boot is green" as
the success criterion. Gates that are green today prove boot, driver registry
consistency, and WEMU guest block I/O; they do **not** prove a usable OS, a
portable kernel ABI, or a usable emulator.

## Verdicts

1. **WuBuOS is a facade for a human user.** `metal_main.c:963` ends in
   `wubu_agi_kernel_run()`; `wubu_shell_run()` (`metal_main.c:976-981`) is an
   empty yield loop. The only kernel console (`wubu_console.c:47-155`) is a
   ring-0 diagnostic dispatcher over COM1. The real shell is hosted-only
   (`src/shell/wubu_shell.c`), the interactive file shell is firmware-only
   (`src/firmware/fw_shell.c`), and the rich GUI needs a Linux Wayland host.
   `mk/targets.mk:188-193` links only `KERNEL_OBJS` into `kernel.elf`; GUI,
   runtime, shell and audio objects are not in it.
2. **WuBuKernel is a contract/reference repo, not the running kernel.**
   L21 is explicit (`include/wubu_driver_contract.h:8-10`, L21 record in
   `os-gaps-2026-11-19.md:220-232`): only `wubu_driver_contract.o` is
   consumed by WuBuOS. The repo has four small modules, raw pointers in
   guest-visible structs, a single compile-time version macro, no capability
   model, no IRQ/DMA/per-CPU contract, and a driver registry that accepts
   hook-less drivers and always succeeds on release. Being "best kernel for
   all OS" means the best **portable ABI/contract**, not hoarding every
   kernel object into WuBuOS.
3. **WEMU is clean-room, not a QEMU wrapper — but it is a facade as a
   developer tool.** There is no CLI, monitor/QMP/HMP, gdbstub, snapshot or
   save/load. The machine is CPU + COM1 + one PCI function + one virtio-blk
   BAR. LAPIC/IOAPIC/framebuffer/PIT/PS2/DMA/SMP are absent or constant
   stubs; ISR is hardwired to 0 and completion is polled. The honest
   fail-closed ISA and 67.35M-step guest block gate are real progress, but
   "a QEMU facade" is a fair description of the **user-facing** surface.
4. **The AGI kernel claim is doctrine ahead of execution.** No brain object
   is linked into `KERNEL_OBJS`; the Live Colonel is an integer expression VM
   (`wubu_console_colonel.c:19-98`); the ring-0 agent is a deterministic cog
   loop (`wubu_agi_kernel.c:104-140`). KV-as-FS has a metal mount/route port
   but no in-kernel 9P server. Swap is 4 MiB first-fit, VMM is 4 KiB only,
   IOMMU is DMAR probe only, GPU/perf are hosted-only, checkpoints store
   counters in 512-byte RAM slots. The `THEORY/07-kernel-layer-model.md`
   byte-budget spec is a good target, not a shipped kernel.

## External standards (primary sources)

- Steam Deck/Machine compatibility contract: frame-time targets, minimum
  glyph size, default controller reachability, controller-only text entry,
  four-badge verdict with per-check results. Copy the **acceptance contract**,
  not proprietary parts. `https://partner.steamgames.com/doc/steamhardware/compat`
- SteamOS recovery UX: Previous B rollback preserving user data, factory
  reset, repair, re-image, recovery tools as explicit tiers.
  `https://help.steampowered.com/en/faqs/view/1B71-EDF2-EB6D-2BB3`
- Omarchy: one supported update entry point, staged channels, snapshots with
  an explicitly documented root-vs-home caveat.
  `https://omarchy.org/manual/updates/` and `/manual/system-snapshots/`
- **CachyOS** (the user's "CatchyOS"; no authoritative CatchyOS product was
  found) is the better backup reference for hardware profiles: priority-ranked
  profile table with fallback (`https://wiki.cachyos.org/features/chwd/chwd/`)
  and a repeatable benchmarker (`github.com/CachyOS/cachyos-benchmarker`).
- Reject: proprietary Proton/Steam client dependency, Linux BPF scheduler
  import, Secure-Boot-off install preconditions. Keep the ideas, not the
  mechanisms.

## Prioritized waves

### Wave 0 — make the metal image a usable computer (highest leverage)

| Leg | Smallest real increment | Behavior gate | Evidence |
|---|---|---|---|
| FS console | `ls/cat/write/rm` in `wubu_console_exec()` over `fat32_boot_volume()` | write HELLO.TXT, reboot same image, `cat` returns exact bytes | `wubu_console.c:47-155`, `fat32_file.c:8-95` |
| Keyboard console | translate `input_key_poll()` scancodes into the COM1 line editor | QMP `sendkey mem+Enter` with no serial input; heap line on COM1 | `input.c:61`, `wubu_console.c:163-268`, `wubu_bonzi.c:289-304` |
| Framebuffer terminal | render the line editor through `vbe_draw_text()` into the GOP surface | typed string appears as readable pixels and on COM1 | `vbe.c:266-476`, `metal_main.c:332-393` |
| Run local app | `apps/` FAT32 scan; extend `run` to load ELF/PE without host 9P | `run APP.BIN` with no COM2 server prints `APP-RAN:` | `wubu_n9.c:158-245`, `wubu_console_recovery.c:118-184` |
| Network commands | `ip`/`ping`, DHCP-lite beyond hard-coded `10.0.2.15` | console-set address + host-observed ICMP echo | `wubu_netstack.c:7,186-434` |
| Crash persistence | crash dump + recovery ledger on virtio-blk, not RAM sim disk | forced crash, reboot, same seq reported | `wubu_crash.c:29-70`, `wubu_recovery.c:30-95` |

### Wave 1 — make WuBuKernel the best portable contract

Keep L21. Do not adopt objects; strengthen the ABI that consumers bind to.

1. Versioned descriptor with opaque `u64` handles, no raw guest-visible
   pointers; reject unknown/truncated versions in both repos.
2. Capability tokens for `DEV_OPEN / MMIO_MAP / IRQ_ACQUIRE / DMA_DOMAIN`,
   fail closed (`-EPERM`) with positive/negative/revoke gates.
3. Fail-closed registry lifecycle: reject hook-less drivers, unbind on
   unregister, real release errors, trylock instead of self-deadlock, PMM
   region-order/overlap validation.
4. BAR decode v2: physical base **and size**, reserved type rejection, named
   MMIO flags; v1 wrapper retained.
5. IRQ routing descriptor (hwirq→virq, MSI/MSI-X, affinity, lifetime) and
   DMA/IOMMU domain descriptors keyed by capability.
6. Structured versioned events, bounded non-truncating writes, panic hook
   capturing fault context.

### Wave 2 — make WEMU a real development emulator

1. PIT tick + external IRQ delivery; LAPIC/IOAPIC bounded MMIO windows.
2. Virtio-blk completion interrupt, ISR bit, descriptor bounds and `S_IOERR`
   negative control; then PCI multi-function topology with IRQ routing.
3. Text framebuffer + PS/2 scancode queue; guest writes and host scrapes
   exact characters.
4. Two-vCPU SMP with per-CPU LAPIC ID and bounded IPI/INIT-SIPI.
5. `wemu-cli` frontend, minimal GDB RSP subset, save/load state. No QEMU
   source; keep the clean-room slerm.
6. Honest docs: label polled/synchronous device paths as bounded, never
   "virtio parity".

### Wave 3 — make the AGI kernel real

1. Metal KVFS init + one routed read/write with reboot persistence.
2. Opaque arena + F16 GEMV + one forward step linked into `KERNEL_OBJS`,
   compared to the hosted CPU baseline within 1e-3.
3. 2 MB huge-page KV slab through a PS=1 PDE path; 4 KB path unchanged.
4. Deterministic decode task + fix the tick-12/33 ring race before adding
   more ring-0 producers; 10 boots must emit identical span IDs.
5. Guarded `kv_read/kv_write` syscalls with capability bit and syscall counts.
6. File-backed AGI checkpoint (counters + KV slab + CRC32), restored after
   reboot; replace the no-op recovery log.

## Workflow rules for every leg

1. Name the outcome before the probe: boot, driver, human session, guest
   execution, persistence, or AGI state. A gate that changes none of these
   is parked.
2. One worktree per implementation leg; fresh submodules; no two agents
   editing one tree. Integrator re-runs every child gate in the canonical
   tree before merge; child self-reports are hypotheses.
3. Behavior gate or it did not happen: write→reboot→read, byte-exact host
   image, host-observed ICMP, pixel scrape. Non-crash is not a gate.
4. Fresh `make kernel-strict`, `tandem_check`, full `make test`, and product
   gate at each **batch boundary**, not after every micro-edit. Keep the
   fast targeted gate inside the batch.
5. Research at each step: primary specs/docs first, derive and reimplement
   in C11, cite the source in the audit doc. No Python runs the loop.
6. Devil's advocate after each batch: does the gate measure the product or a
   model of it? Is the object linked into the shipped image? Does the old
   implementation fail the new gate? Are hosted-only wins being reported as
   metal wins?
7. Never say "done" for WuBuOS/WuBuKernel/WEMU. Report HEAD, gate line, and
   the next open leg.

## Current gate scope (for honesty)

- `test_wemu_kernel`: real kernel image reaches `kernel_main`, no fault.
- `test_wemu_kernel_virtio`: guest reports live virtio-blk at 67.35M steps.
- `ci_boot_assert`: boot trace, prompt, driver-registry consistency.
- `test_linux_product`: hosted Wayland client frames, not metal GUI.
- None of these proves a human can create a file, type on the keyboard, run
  an app from the boot disk, or recover state after a crash.

## Landed increment — Wave 0 FS console

- `wubu_console_cmds.c` now implements `fs ls|cat|write|rm` with direct
  `ls/cat/write/rm` aliases over `fat32_boot_volume()`. The real shell remains
  hosted-only; this is a deliberate small in-kernel file surface, not a shell.
- WEMU `92d9b7d` fixed descriptor classification: T_OUT data descriptors are
  device-readable and were previously dropped while status still said OK.
  `tests/test_virtio_mmio.c` now does T_OUT sector 1 -> T_IN sector 1;
  pre-fix source RED-controls with `T_OUT data was dropped`.
- `make test_wemu_fs` boots the real kernel twice through WEMU against the
  same 1 MiB FAT32 image: `write HELLO.TXT world`, then a fresh process
  `cat HELLO.TXT` must read `world`. Verified PASS. This is the first gate in
  this workflow where a human-created file survives a reboot on metal.

## Landed increment — Wave 0 keyboard console

- Refactored the ring-0 console line editor into `console_feed_char()` so
  COM1 bytes and PS/2 key events run through identical history/tab/backspace
  handling. `ps2_poll_keyboard()` consumes the 8042 output buffer from the
  console loop; IRQ1 remains the real-hardware path.
- WEMU `ebe62a8` adds a bounded PS/2 scancode queue (0x64 OBF / 0x60 data)
  and `wemu_machine_queue_keyboard()` for deterministic post-prompt injection.
- `make test_wemu_kbd` types `mem` from PS/2 with no COM1 input and asserts
  `heap used=` in the post-injection window. Negative control with the
  injection window but no key event FAILs at 200M steps; positive PASSes in
  50k steps. Fixed a latent `input_init` declaration mismatch in `libc.h`.

## Landed increment — Wave 0 framebuffer terminal

- `wubu_fbterm.c` echoes the live ring-0 line editor into the GOP
  framebuffer using the existing VBE primitives; typed characters and
  submitted commands are rendered on the metal display, not only COM1.
- The WEMU gate locates `wubu_fbterm_hash`, reads it from guest RAM before
  and after injected PS/2 input, and requires the hash of rendered pixels
  to change. Positive: `0 -> 94e6cbba546ee2ff` with `mem` executed.
  Negative control with no key event FAILs; both observed in this batch.

## Landed increment — Wave 0 local app script

- `cmd_run` no longer overmounts the boot volume with an AHCI RAM sim disk.
  It uses the volume already attached by metal_main (virtio-blk on metal),
  so `run APP.TXT` executes from the guest's own FAT32 disk with no host 9P
  or COM2 server.
- `make test_wemu_app` writes APP.TXT containing `mem`, reboots in a fresh
  WEMU process, types `run APP.TXT`, and requires both `heap used=` output
  and `run: APP.TXT: 1 commands executed`. Pre-change source RED-control
  FAILs with `run: no FAT32 volume`; restored source PASSes.
- Honest scope: this is a guest script app, not an ELF program loader. The
  native ELF app path is still open.

## Landed increment — Wave 0 crash persistence

- `wubu_crash_dump` now writes `CRASH.DMP` to the FAT32 boot volume with a
  persistent seq counter; `wubu_crash_pickup` reads it from that volume.
  Moved pickup after the boot volume is attached (it used to run before
  mount, so a file-backed record could never be seen).
- `make test_wemu_crash` forces `crash`, reboots in a fresh WEMU process
  against the same image, and requires `crash record found (seq=1 ...)`.
  Pre-change source RED-control FAILs at boot 1 with no `wrote CRASH.DMP`.
- Added `WEMU_KERNEL_SHOW_SERIAL` to the gate so a script can assert a
  boot-time marker without forcing a failure; fixed a gate/script mismatch
  where the marker was present in serial but suppressed on PASS.

## Landed increment — Wave 1 ABI descriptor

- WuBuKernel `c7c8527` adds `wubu_abi.h/.c`: a 40-byte v2 wire descriptor
  with only u64 handles (`device_handle`, `driver_handle`, `mmio_token`),
  no guest-visible pointers, and fail-closed validation of magic, exact
  struct_size, version and reserved flags. `make test` and freestanding
  compile pass; negative cases reject truncation, unknown version, bad
  magic/size/flags.
- WuBuOS adds `test_kernel_abi` to the critical hosted tier, consuming the
  canonical WuBuKernel checkout. L21 stays intact: no ABI object is linked
  into kernel.elf. Bridge PASS.
- This is the ABI descriptor only; capabilities, BARv2, IRQ and DMA
  contracts remain open.

## Landed increment — Wave 1 capability tokens

- WuBuKernel `5167f6b` adds `wubu_cap.h/.c`: opaque u64 tokens for
  DEV_OPEN, MMIO_MAP, IRQ_ACQUIRE and DMA_DOMAIN. Root-only v1 grants,
  exact class+object checks, immediate revoke, generation-stamped tokens
  that cannot be replayed after slot reuse, and bounded 32-slot storage.
- `make test` covers uncapped denial, wrong class/object, non-root grant,
  double revoke, reset and cross-token isolation. Freestanding compile
  passed. WuBuOS `test_kernel_cap` bridges the same contract in hosted
  mode; L21 is preserved (no cap object in kernel.elf).
- Still open: wiring these caps into actual device open / MMIO map /
  IRQ / DMA paths, and the BARv2 descriptor.

## Landed increment — Wave 1 BAR v2

- WuBuKernel `a96c956` adds `wubu_contract_decode_bar_v2`: physical base,
  aperture size from the all-ones probe, BAR type, and named MMIO flags
  (present/writable/cache-inhibited). I/O BARs, legacy 1MB and reserved
  type encodings, non-power-of-two sizes and null output fail closed.
  v1 decode is retained. All WuBuKernel tests and freestanding compile pass.
- WuBuOS `test_kernel_bar_v2` bridges the decoder in hosted mode, asserting
  `0x0F000000 / 1 MiB / 32-bit / flags=7` plus reserved-type and I/O BAR
  rejection. L21 holds: no contract object is linked into kernel.elf.
- One initial test fixture was wrong (it treated a valid 16-byte all-ones
  probe as unimplemented); it was removed after the decoder output showed
  the probe was legitimate, not after weakening the decoder.
- Still open: switching actual PCI/MMIO consumers from v1 to v2.

## Landed increment — Wave 1 binding path uses BAR v2

- `wubu_drv_map_bar_range` now decodes through `wubu_contract_decode_bar_v2`
  using the scanned aperture size, rejects missing/non-power-of-two or
  undersized windows, and maps with named PRESENT|WRITABLE flags instead of
  the integer 3. The kernel contract object is built from the canonical
  WuBuKernel checkout because the in-tree gitlink is not initialized.
- Legacy `test_drv` fixtures now carry probed `bar0_size` values; a new
  negative case asserts a reserved BAR type is rejected by the actual
  binding path. Pre-change source RED-control FAILs with
  `reserved BAR type was mapped`; restored source PASSes.
- Both `include/wubu_driver_contract.h` mirrors carry the v2 declarations.
- Still open: wiring capability tokens into this binding path.

## Landed increment — Wave 1 caps on the binding path

- `wubu_drv_dev_t` carries an `mmio_cap` token granted from ROOT at device
  registration. `wubu_drv_map_bar_range` checks `WUBU_CAP_MMIO_MAP` before
  geometry, so a device with no live token cannot map a BAR.
- L21 is deliberately extended: `wubu_cap.o` joins the single shared
  contract object in `kernel.elf` (two small WuBuKernel contract modules,
  not the driver core). This is the first authority object consumed by the
  running kernel; its behavior is bridged in `test_kernel_cap`.
- `test_drv` asserts denied-without-cap, allowed-with-cap, denied-after-
  revoke. Pre-change binding source RED-controls with
  `map_mmio without capability token`; restored source PASSes.
- Still open: DEV_OPEN and IRQ/DMA enforcement, and capability handoff to
  untrusted callers rather than device-registration time.

## Landed increment — Wave 1 DEV_OPEN at bind

- `wubu_drv_dev_t` carries a `dev_open_cap` token, granted with MMIO_MAP at
  normal registration. `wubu_drv_probe()` refuses to call a driver's probe
  callback without a live DEV_OPEN capability.
- `wubu_drv_add_device_noauth()` is an explicitly unprivileged registration
  path for sandbox/negative gates; it mints no tokens. Registry-level
  revoke/grant controls let a gate flip the same authority bit.
- `test_drv` now proves: noauth device stays unbound, revoked device stays
  unbound, regranted device binds. MMIO_MAP denied/allowed/revoked gate
  remains green. All driver-test link recipes include the cap module.
- Still open: IRQ_ACQUIRE and DMA_DOMAIN enforcement, and capability
  handoff beyond registration time.

## Landed increment — CI enforcement + portable contract path

- `boot-verify.yml` now runs the WuBuKernel contract gates
  (`test_kernel_abi`, `test_kernel_cap`, `test_kernel_bar_v2`) and the four
  Wave 0 metal usability gates (`test_wemu_kbd`, `fs`, `app`, `crash`) with
  a bounded `::error::` annotation. All eight pass locally.
- Removed hardcoded `/home/wubu/WuBuKernel` from the shipped Makefiles.
  `WUBU_KERNEL_CONTRACT` prefers the pinned submodule (CI) and falls back
  to the canonical local checkout only when the submodule is uninitialized.
  The forwarding header `include/wubu_cap.h` keeps one contract definition.
- This closes the local-only gap for all Wave 0/1 gates proven earlier.

## Landed increment — Wave 1 IRQ_ACQUIRE

- `interrupt_register_ex()` is the authority-aware registration path: it
  returns -1 unless a live IRQ_ACQUIRE token is held. `interrupt_register()`
  is the void wrapper; `interrupt_init()` grants the token at boot.
- `test_kernel_irq_cap` uses purpose-built link stubs for the assembly ISR
  vector table and proves granted install, revoked refusal, and regranted
  install. The gate is in the critical tier and boot-verify contract step.
- `wubu_irq_cap_stubs.c` is test-only and defines no product behavior.
- Still open: DMA_DOMAIN and capability handoff beyond registration time.

## Landed increment — Wave 1 DMA_DOMAIN

- `wubu_vq_setup_ring()` now requires a live DMA_DOMAIN token before
  allocating or publishing a virtqueue ring. Lazy init grants once; revoke
  stays revoked until an explicit regrant, so a failed authorization cannot
  silently re-arm itself.
- `test_virtio_blk` proves granted setup, revoked refusal, and regranted
  setup on the real ring path. `test_virtio_net` also passes with the cap
  linked into its direct-inclusion build. Both are in boot-verify's
  contract gate step.
- All four capability classes (DEV_OPEN, MMIO_MAP, IRQ_ACQUIRE, DMA_DOMAIN)
  are now defined AND enforced on real WuBuOS paths. Capability handoff
  beyond registration/init-time remains open.

## Landed increment — Wave 3 ring-0 KV namespace

- `metal_main` now initializes the KVFS singleton and mounts `/kv/world`
  over a 4 MiB float tensor. The console `kv write/read` commands route
  through the real KV namespace and persist a compact 16-byte state file
  on the FAT32 boot volume.
- `make test_wemu_kv` boots the real kernel twice against the same image:
  `kv write 11 22 33 44`, then a fresh process reads `kv state 11 22 33 44`
  back through the namespace. PASS, with the gate added to boot-verify.
- Debugging found a real build-specific trap: a float cast inside a
  varargs `klog_printf` printed 0 even when the float bits were correct.
  The console path now converts via IEEE-754 bits instead of trusting
  that cast in the formatter argument.
- Honest scope: this is a ring-0 path-routed KV state slice, not a full
  9P server or a model forward pass. Those remain open.

## Landed increment — Wave 3 ring-0 F16 GEMV

- `wubu_metal_model.c/h` adds a 64-byte-aligned opaque arena and a fixed
  8x16 F16 GEMV slice linked into `kernel.elf`. `test_metal_model` compares
  every output row to a hosted f32 baseline within 1e-3 and checks arena
  alignment/size accounting. Console `model` executes the same path in the
  real guest; gate sees `y0m=263 y1m=643` with no fault.
- The guest run exposed a WEMU ISA gap: gcc14 emitted `pinsrw` with REX.B
  (`66 41 0F C4 C0 imm`) in the GEMV loop. WEMU `7d3352f` implements it,
  with an 8-immediate host-differential gate and pre-fix RED control.
- Honest scope: this is one tiny deterministic GEMV, not a loaded model or
  a full forward pass. Weight loading, 2 MB huge-page KV slab, and 9P KV
  serving remain open.
