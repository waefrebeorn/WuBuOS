# 00-philosophy — Ring-0 Colonel Space

*Addendum entry. Human-written. Last updated 2026-08-02.*

## The virtue

WuBuOS is an **AGI operating system**: the Colonel (wubuwizard + the AGI
supervisor) lives IN the kernel, at ring 0, like DOS — not as an app on top
of a generic kernel. The computer IS the AGI's environment.

## What we found (research-converged)

- TempleOS proved an AGI-sized computer can be all-ring-0, single-address-
  space, with the language (HolyC) as the interface to everything.
- AIOS (arXiv 2403.16971) proved the LLM belongs in the kernel with its own
  syscall layer (LLM-as-core, agent syscalls, context manager).
- SASOS research (Opal/Mungi) proved protection can be orthogonal to
  translation: single address space WITHOUT losing isolation — capability
  guards protect the brain even at ring 0.
- The metal boots directly into the Colonel space: live console REPL (COM1),
  PCI, APIC/LAPIC tick, scheduler, Bonzi + agent tasks co-resident.

## Design consequences

1. No user/kernel wall — "user space" is a set of equal-privilege subsystems
   bounded by capability guards (`wubu_capguard`), not by a ring.
2. Every bug is a ring-0 bug — the measured boot chain is the recovery root,
   and the checkpointed world (Phantom-style) must return on restart.
3. The human is a guest of the Colonel — Bonzi is the hedged face; the
   serial console is full ring-0 and MUST eventually be gated (`wubu_human`).
4. Live development is the native workflow (HolyC JIT + console REPL), not
   host edit→rebuild→reboot→grep.
