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
