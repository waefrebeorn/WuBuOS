# Outcome loop (devils-advocate edition)

The tandem goal is **a bootable, usable development system**, not a maximum
count of correct instructions. Work is ordered by outcomes:

1. **Boot** — a real image reaches a usable shell with display and storage.
2. **Drivers** — one owner maps each BAR; devices bind from one registry;
   the guest can see and use them.
3. **WEMU guest** — the same kernel image executes in WEMU far enough to
   run its own code, not just host-ELF user programs.

## New cadence (supersedes one-bug-one-pin)

- A WEMU ISA fix is **not** a reason to stop and pin. It goes into a WEMU
  batch. Pin into WuBuOS only when the batch fixes a guest-visible blocker
  or reaches five commits, then run the full gate **once**.
- Before opening a probe, write the outcome it could change. If the answer
  is "nothing bootable changes", park it. Correctness for its own sake is
  a background activity, not the main loop.
- Timebox each probe to one focused question with an observable result.
  A spike earns a verdict, not an infinite follow-on.
- The full gate is a release gate, not a per-edit reflex. During a batch,
  run the targeted gate for the changed subsystem; batch boundary earns
  `make tandem_check` and one full/product run.

## Devil's advocate checks before every commit

- Does this change the image, a driver, or WEMU guest progress? If not,
  why is it in this cycle?
- Is the gate measuring the product, or a model of the product? A host
  differential for an opcode nobody executes is evidence of correctness,
  not evidence of boot.
- Did I just re-run the same slow gate to feel safe? Targeted gate first,
  release gate at the batch boundary.
- Am I repeating a pattern I learned from another repo (e.g. stash-pop
  workflows)? Confirm it belongs here before using it.

## CI artifact replay

When `boot-verify` finds a new ISA gap, download the run's `boot-diagnostics`
artifact and run `make test_wemu_kernel WEMU_KERNEL_ELF=<extracted kernel.elf>`.
This uses the same gate as CI against the exact failing binary, so several
gcc14-vectorization gaps can be found and fixed in one local batch instead
of one CI round-trip per opcode. A RED control must still be recorded for
each new WEMU gate.

## Guest execution milestone

The 120M-step WEMU spike now reaches `WuBuOS>` (live console prompt) with
`fault=0` after tasking, AGI realm tasks, FAT32 mount and VMM page mapping.
`make test_wemu_kernel` remains the fast `kernel_main` gate in tandem_check;
`make test_wemu_kernel_deep` is the opt-in 120M-step gate to the `WuBuOS>`
prompt (verified PASS at 113.9M steps, fault=0; the saved gcc14 CI kernel
artifact reaches it in 35.45M steps). Boot to a usable shell
with real devices is still unverified.

## Current strategic gap

Spikes 001–003 showed WEMU could not execute the higher-half kernel at all;
the boot-blocker batch fixed canonical addressing, CR moves, descriptor
loads, segment loads, CPUID LM and APIC MSRs. Spike 003 now runs the real
kernel through `kernel_main`, heap, interrupts, APIC and VBE init.

The next gap is **environment fidelity**, not the CPU: framebuffer sizing
and MMIO devices (LAPIC/I/O-APIC were redirected to spare RAM in the spike).
`make test_wemu_kernel` now gates guest execution into kernel_main; productize
the full loader/harness only after those are modelled honestly. Random opcode
audits stay paused.

## Usability-first replanning (2026-11-23)

The four-repo/tandem audit produced `docs/compendium/04-roadmap/usability-gap-map-2026-11-23.md`.
It is the contract for the next legs: boot, driver binding and WEMU guest
execution are proven; a human file session, keyboard/framebuffer console,
local app loading, cross-reboot recovery, portable kernel ABI and AGI state
restoration are not. Start with Wave 0 (FS console) and do not report a
usability win without a write->reboot->read or equivalent behavior gate.
