# TANDEM WORKFLOW — WuBuKernel + WuBuOS + WEMU, drivers first

Status: living contract. Owner directive (2026-11-21): *"wubukernel and
wubuos work reorientation, wemu as well — this is the tandem workflow to get
a bootable and usable development workflow. Drivers."*

This doc is the reorientation. It does **not** reverse the L21 decision
(`os-gaps-2026-11-19.md`, L21): WuBuKernel stays a hosted/freestanding
reference consumed through `test_kernel_bridge`, **not** the running kernel
core. "Reorientation" here means one contract across three repos, not a
forced merge of two different driver models. If a future leg adopts
WuBuKernel objects into `KERNEL_OBJS`, that is a **new, explicitly gated
decision** that supersedes L21 — never a silent side effect of a driver leg.

## The tandem

| Repo | Role in the tandem | What it must prove |
|---|---|---|
| `WuBuKernel` (`/home/wubu/WuBuKernel`, `main`) | the **contract**: device/driver registry, bounded heap, PMM, tick/console | strict-C11 hosted behaviour **and** `-ffreestanding -nostdinc` compile |
| `wubuos` (`/home/wubu/wubuos`, `main`) | the **body**: the only tree that boots. Owns the real PCI driver registry (`wubu_drv.c` + `wubu_bus.c`), VMM mapper, real storage/net, GUI | `make kernel` + boot gate + hosted full gate |
| `wemu` (pinned submodule `src/wemu`, `5cf1dee` on `leg/user-mode-vsl`, descended from `leg/nested-signals`@`2ce67c0`) | the **usable**: guest execution of foreign static ELF/tools through the VSL syscall table, so the booted system can run other OSes' tools | full WEMU `make test` in the local checkout + `test_wemu_busybox` through the bridge |

The gates build WEMU **out of tree** from `src/wemu` (`mk/wemu_user.mk`:
copy into `build/wemu-user`, apply the narrow VSL prot overlay), not from
any local `/home/wubu/wemu` branch. The local checkout was reconciled by a
fast-forward to `2ce67c0` (it was a strict ancestor, with no local-only
commits). The WEMU legs from that reconciled point are `660891e`
(memory bit-base BT/BTS/BTR/BTC addressing), `ac970c6` (AF borrow
semantics for subtraction; 8641-round host differential gate),
`660891e`..`5cf1dee` (BT memory base, AF borrow, SBB borrow CF,
zero-count SHLD/SHRD flags, redundant-REX decoding, `make test-clean`,
CI from clean, tracked busybox fixture, FCOMI flags, byte MUL/DIV/IDIV AH half, REP fault restart, gcc -fanalyzer static_check), the last now the pinned pointer and local branch tip. Do not
downgrade the pointer to the older `775bf40` ancestor.

The loop is one direction and one back-edge:

```
contract (WuBuKernel)  ->  boot (wubuos metal)  ->  use (WEMU guests)
        ^                       |                       |
        +------ gate evidence --+------- gate evidence --+
```

A change in any leg ships with the gate in **its own** repo AND the
cross-repo gate that proves the seam still works. There is no "it compiles
in my repo" completion.

## Why drivers are the reorientation's centre

The 2026-11-19 audit measured the honest boundary: the running kernel drives
APIC/PIT/HPET/RTC, Bochs-VBE, PS/2, COM1, PCI discovery, ACPI/SMBIOS and
virtio-blk/net; the ~150 `wubu_drv_*` modules are perception/routing tables
for a hosted Linux host or test-injected register models. The named
cross-cutting prerequisite at contract creation was:

> `dev->bar0 -> wubu_vmm_map_page -> probe` — the LFB mapping in
> `metal_main.c` proves the mechanism; `wubu_drv_dev_bar()` exposes the BAR;
> `wubu_drv_probe()` calls the driver directly. **Nothing connected them.**

That was true when this contract was written. It is **no longer open**: D1
made `wubu_drv_map_mmio[_range]()` the single BAR→VMM owner, DRV10b/DRV10c
added ranges and rollback, D2 made xHCI consume it, and D5/D5b carried BAR
space through the registry so legacy virtio I/O BARs are rejected by the
MMIO owner and decoded by the one I/O owner. The cited line numbers below
are historical evidence of the gap, not a present-day defect.

Verified in this session:
- `src/kernel/wubu_drv.h:71` declares `wubu_drv_dev_bar()`.
- `src/kernel/wubu_drv.c:149` implements it and returns the raw PCI BAR.
- `src/kernel/wubu_drv.c:160-177` `wubu_drv_probe()` binds and calls
  `probe(dev)` with **no** BAR mapping step.
- `src/kernel/wubu_vmm.c:177` is the one real mapper; `metal_main.c:370-377`
  and `:668-674` show the working LFB/GOP call pattern.
- `grep -rn 'bar.*vmm\|vmm.*bar\|map_bar' src/kernel` finds no owner.

Making that seam a single owner was drivers leg D1 below; it has shipped.
The paragraph is retained as the reasoning for the leg, not as pending work.

## Drivers-first legs (smallest real increment + behaviour gate)

**D1 — BAR→VMM single owner (DELIVERED 2026-11-21, `leg/d1-bar-vmm`).**
`wubu_drv_map_mmio()` in `wubu_drv.c` is the one owner: rejects no-BAR and
I/O-space BARs, masks BAR type bits (and assembles 64-bit BAR0+BAR1), calls
`wubu_vmm_map_page` through a weak seam, fail-soft on map failure.
`wubu_drv_map_all_mmio()` is the **late** idempotent pass — it must run
AFTER `wubu_vmm_init()` because the mapper allocates PT pages from the VMM
pool that init resets; `wubu_drv_probe()` runs before that point and must
never map. `metal_main.c` calls the late pass and logs the count.
Gate: `test_drv` extended with a strong recording mapper stub — asserts
phys/flags/VA, no-BAR and I-O-BAR rejection, 64-bit assembly, fail-soft
failure, and idempotent late pass. `make test_drv` exits 0. Metal boot log
confirms `WuBuOS: 1 device MMIO BARs mapped` (real behavior, not a no-op).

**D2 — one real consumer of D1 (DELIVERED 2026-11-21, `leg/d2-virtio-consumer`).**
`wubu_xhci_probe()` was dereferencing a raw physical BAR as a VA (broken
once paging is on), and its `bar0 & 1` test was inverted — bit 0 SET means
I/O space, not MMIO. It now finds its device through the one registry
(`wubu_drv_find_class(0x0C,0x03)`) and obtains a mapped VA via D1's
`wubu_drv_map_mmio()` at its own high-half slot. The duplicate PCI scan is
removed, not left as a second path. Gate: `test_xhci` stubs the registry
owner and asserts `probe()` makes exactly one map call, uses the returned
VA, and returns clean `absent` with zero map calls when no device exists.
- Note: D1 originally mapped one 4K page per device. DRV10b
  `wubu_drv_map_mmio_range()` now lifts that limit (up to 16 pages) through
  the SAME owner. A mid-range failure rolls back already-mapped pages via a
  weak-seamed `wubu_vmm_unmap` and returns -1 with no fabricated VA.
- xHCI consumes the range form: it requests a four-page (16 KB) aperture,
  covering the whole typical xHCI BAR0 — capability/operational, runtime,
  doorbells and the MSI-X region — through the same owner. `test_xhci`
  asserts pages == 4.

**D3 — contract alignment, not adoption (DELIVERED 2026-11-21).**
WuBuKernel now owns `include/wubu_driver_contract.h` + `src/wubu_driver_contract.c`:
BAR space encoding, 64-bit assembly, present+RW map flags, zero-VA sentinel,
and the discover→bind→map-after-vmm-init→use lifecycle, with C11
`_Static_assert`s so drift breaks the build. WuBuOS consumes it: `wubu_drv.c`
delegates BAR decoding to the contract (one decoder, no second copy), and
`test_kernel_bridge` proves both sides agree on version and decode results.
L21 is intact — this is a shared contract, **not** KERNEL_OBJS adoption of
the WuBuKernel driver model. Gates: WuBuKernel `make test` + `freestanding`
(4 TUs); wubuos `test_drv`, `test_xhci`, `test_kernel_bridge`, `test_docs`,
`kernel-strict`, `make kernel` + boot assert — all PASS.

**D4 — guest-facing driver surface (DELIVERED 2026-11-21, `leg/d4-guest-surface`).**
The VSL driver inventory is now readable by a real guest at
`/proc/wubu/drivers` — no new syscall, ordinary `cat`. One owner
(`vsl_driver_inventory`) renders the registered VSL drivers; one provider
(`vsl_open_inventory`) turns that snapshot into a real host fd, shared by
`vsl_open` and `vsl_openat` so the existing read/lseek/close paths work
unchanged. Gate: `test_wemu_busybox` runs static busybox `cat` inside WEMU
and asserts the **exact** inventory line, alongside the existing applet
The source is BOTH: the VSL driver registry and, when the host supplies it,
the OS driver registry summary via `wubu_drv_summary()` →
`vsl_driver_inventory_set_host()`. `test_wemu_busybox` asserts a real guest
sees `[drv] 144d:a80a ... -> nvme` alongside the VSL driver, and that
clearing the host section removes only that part (differential).

Each leg gets its own git worktree (`git worktree add .../wubuos-<leg> -b
leg/<name> main`), one agent per worktree, own branch, commits on the leg.
The integrator merges sequentially by evidence, never by self-report.

**D5 — BAR space correctness + virtio-blk on the one owner (DELIVERED 2026-11-21).**
The PCI scan always masked BAR type bits, so a legacy virtio I/O-port BAR
arrived at the registry as a plain address indistinguishable from MMIO —
`wubu_drv_map_all_mmio()` could map an I/O window as memory. The scan now
carries `bar0_is_io` from config space (and from sysfs resource flags in the
hosted path); the MMIO owner rejects that flag, and `wubu_drv_io_base()` is
the single owner that turns an I/O BAR into a 16-bit port base.
`wubu_virtio_blk_probe()` and `wubu_virtio_net_probe()` no longer run a
second `wubu_pci_scan()` or re-reads BAR0: each finds its device with
`wubu_drv_find_pci_id()` and gets the port base from the I/O owner.
Neither duplicate scan is left alongside the registry. Gate: `test_drv`
proves a scanned I/O BAR is rejected by the MMIO mapper, decoded by the I/O
owner, and that MMIO and out-of-range bases are refused; `test_virtio_blk`
and `test_virtio_net` prove probe succeeds only when the registry marks the
BAR as I/O space. `test_xhci`, kernel boot and `kernel-strict` stay green.

## Workflow rules (unchanged, still binding)

1. Worktree per leg; never two agents in one tree (`make -j` races `.o`).
2. **Behaviour gate or it didn't happen**: real output, round-trip or
   on-disk state. Non-crash is not a gate. Metal legs boot under QEMU and
   keep `ci_boot_assert` green; kernel touches keep `make kernel-strict`
   clean.
3. **No theater**: a success-path no-op is a bug. Banners must match bodies.
4. **One owner per concern**: search the tree before writing anything that
   smells like a copy. Reuse `wubu_vmm_map_page`, `wubu_drv_dev_bar`, the
   existing virtqueue core, the VSL syscall table.
5. Verify by running — `make <target>` then read the real output. Never
   cite a stale test count. Two historically load-sensitive targets had
   real root causes and were fixed: `test_svc_super` waited a fixed 100 ms
   instead of polling the restart condition; `test_host_exec` launched an
   instantly-exiting guest that the sync reaps before observation. If a
   gate still fails only under load, isolate it, then re-run the full gate;
   report a regression only if the isolated gate also fails.
6. Docs are testable: new paths named here must exist (`make test_docs`).
7. Submodules untouched unless the leg IS the submodule (WuBuKernel is
   public; WEMU legs are pushed from `/home/wubu/wemu` and the pointer
   bumped in `wubuos`).

- `make ci_status` (live public API, no `gh` auth) fails on any recent red
  run; `ci.yml` is `master`-only, so `boot-verify` is the `main` gate.

**Spec-driven driver correctness (round 175):** the boot drivers are
checked against their primary specs, not just self-tests. xHCI capability
offsets, byte-offset semantics and TRB encoding follow Intel xHCI 1.2b;
PCI BAR type/I-O decode follows PCI 3.0 §6.2.5.1; virtio fences, stable
config reads and FAILED handling follow OASIS VIRTIO 1.0. Unfixed spec
gaps (PCI subsystem IDs, prefetchable BAR
cache policy) are listed in the gap map — do not claim hardware support
for them from these gates.

## Gate quick-reference (the tandem's definition of green)

```sh
# live CI status without gh auth (public API, not the cached HTML page)
cd /home/wubu/wubuos && make ci_status
# all three repos, one command (contract -> boot -> guest -> emulator)
cd /home/wubu/wubuos && make tandem_check
# on another machine: make tandem_check TANDEM_ROOT=/path/to/repos
# contract
cd /home/wubu/WuBuKernel && make test && make freestanding
# body: bridge, kernel, boot, hosted
cd /home/wubu/wubuos && make test_kernel_bridge
cd /home/wubu/wubuos && make kernel-strict
cd /home/wubu/wubuos && python3 tools/ci_boot_assert.py src/kernel/kernel.elf --kernel
# real QEMU pci-bridge + disk behind it: registry must gain both devices
cd /home/wubu/wubuos && make pci-bridge-gate
# real QEMU modern-only virtio disk: caps + live FAT32 boot volume
cd /home/wubu/wubuos && make virtio-modern-gate
# the boot assertion now also requires >=1 registry-mapped MMIO BAR
cd /home/wubu/wubuos && make test_ci_boot_assert
cd /home/wubu/wubuos && make test_virtio_pci_modern
# OASIS 4.1.4 cap walk: QEMU-shaped chain, non-virtio caps skipped
cd /home/wubu/wubuos && make -k -j24 test
# usable: emulator, then the bridge acceptance
cd /home/wubu/wemu && make test
cd /home/wubu/wubuos && make test_wemu_busybox
```

## Baseline measured at contract creation (2026-11-21)

- WuBuKernel `make test`: **PASS** (`[wubukernel] all pass`, `=== PMM tests:
  all passed ===`); `make freestanding`: **PASS** (3 TUs compiled with
  `-std=c11 -ffreestanding -nostdinc`).
- wubuos `make test_kernel_bridge`: **PASS** (`[kernel_bridge] all pass`).
- wubuos `make kernel` + `ci_boot_assert`: **was RED at HEAD** —
  `wubu_fantml.c:102` called hosted `getenv()` unconditionally while the
  file is in `KERNEL_OBJS`, leaving `undefined reference to getenv` in the
  freestanding link (the "hosted-green is not kernel-green" class). Fixed
  in `98888998` with the tree's `#ifndef MYSEED_METAL` split. Re-verified:
  `make kernel` builds, `ci_boot_assert` **PASS** (3839 bytes, interactive
  `WuBuOS>` console), `make kernel-strict` builds, `test_hw_fantml`
  **11/11**, `test_docs` **599/0**. Class audit: `vbe.c` already guards
  `getenv` behind `#ifdef VBE_HOSTED`; `wubu_thermal_read.c` and
  `wubu_drv_install.c` are not in `KERNEL_OBJS` — no other leak in the
  freestanding link.
- WEMU `make test`: **PASS** (all gates plus `test_btmem`, the 8641-round
  `test_string_diff`, the 39329-case `test_flag_diff`, 48 named
  zero-count SHLD/SHRD flag cases, `test_btimm`, `test_fcomi_flags` and `test_divmul` and `test_rep_fault_restart` host-CPU gates). The
  local checkout and pinned pointer are both `5cf1dee`, descended from the old `2ce67c0` pin; `make test-clean`
  rebuilds everything before the gates and no local-only branch remains.
- Final integrated run at `95ac08cf`: `make -k -j24 test` exit 0,
  "✅ All tests passed (all tiers)" (`guest_host_full.log`);
  `env -u DISPLAY -u WAYLAND_DISPLAY make test_linux_product` PASS —
  1 headless boot + 5 distinct real-GUI frames (`guest_host_product.log`).
  Local evidence only for the final commit: `gh` is not authenticated here.
  CI status was read from the public web UI through a real browser:
  `boot-verify` #95–#112 all **completed successfully** (40m–45m each),
  covering D4, the game commits, DRV10b, the rollback, the xHCI range,
  the host driver inventory, the game removal, D5/D5b, the boot gate,
  the console/drv command, the gated drv command and the bounded
  formatters. Run #113 and later commits were still in progress or not
  yet reported at check time; do not claim CI green for those yet.
  The other workflow (`ci.yml`) triggers only on `master`, not `main`;
  its recent red runs are old `master` commits and are not evidence about
  this branch. `boot-verify` is the tandem's gate.

## Honest boundaries

- L21 boundary is intact: no WuBuKernel object in `KERNEL_OBJS`/`kernel.elf`.
- The BAR→VMM seam (D1) is **closed**, with DRV10b range mapping and DRV10c
  rollback; xHCI consumes a four-page 16 KB aperture. The guest inventory
  includes a host-supplied snapshot from `wubu_drv_summary()`, but there is
  no live metal→guest syscall channel. The compact one-line report is also
  visible to a guest at `/proc/wubu/registry`, using the same formatter.
- The 1000-line `driver-bank.md` is a coverage *ledger*, not proof of a
  bound driver in the running kernel; "wired" there means a perception or
  routing module, not metal MMIO. Do not cite it as hardware support.
