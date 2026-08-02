# 00-philosophy — Decision Log

*Append-only. Format: date | decision | alternatives considered | reason.*

## 2026-08-02 — Interrupt source: LAPIC timer, not IOAPIC-routed PIT
- **Decision:** the 100 Hz system tick comes from the LAPIC timer (vector 32,
  periodic mode `0x1<<17`), not the PIT via the IOAPIC.
- **Alternatives:** PIT → IOAPIC INTIN2 (tried — produced a spurious
  vector-0 #DE during the boot bisect); LAPIC timer (chosen).
- **Reason:** direct delivery, no external routing; verified stable on metal
  (`tick` advances, uptime/sleep/AGI cycle work).

## 2026-08-02 — Preemption: disabled until the resume path is fixed
- **Decision:** cooperative round-robin + 100 Hz tick (stable base).
- **Alternatives:** timer preemption (tried — the resumed iretq #GPs with
  the NT flag set; no TSS in the GDT).
- **Reason:** the live system (console, bonzi, agent, uptime, sleep) must
  stay stable; the preempt fix is a tracked open item (03-learned).

## 2026-08-02 — InterruptFrame: rax IS saved (15 registers)
- **Decision:** the common ISR handler saves/restores rax (15 regs total),
  matching the C ABI's caller-saved set that isr_dispatch's C code clobbers.
- **Alternatives:** skip rax (the old 14-reg save — the ISR clobbered rax in
  interrupted loops → "memcpy writes to 0xff000000").
- **Reason:** an interrupt is a function call; caller-saved registers must
  be preserved or the interrupted code corrupts (verified: the #PF is gone).

## 2026-08-02 — Docs: compendium with generated + addendum halves
- **Decision:** Wikipedia-style compendium, `make docs` regenerates the
  reference sections; philosophy/learned/roadmap are human addenda.
- **Alternatives:** a single monolithic doc (rejected — unmaintainable);
  fully manual docs (rejected — drift).
- **Reason:** the project needs durable, honest institutional memory — the
  prestige/recursive-learning system the work depends on.
