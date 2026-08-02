# 04-roadmap — Sub-Module Plan (P0/P1)

From `docs/DA_DESIGN_AUDIT.md` (triple-DA of the AGI OS design).

## Fix-first / open items
- [ ] Preempt resume #GP (NT-bit iretq; add a real TSS + fix the resume
      alignment) — evidence in 03-learned/didnt-work.md
- [ ] #PF handler: fault-in instead of halt (feeds wubu_vmm)

## P0 — the foundation
- [ ] `wubu_vmm` — page allocator, demand paging, swap, real #PF fault-in
- [ ] `wubu_sync` — spinlocks + ISR-safe FIFO queues (preemption prerequisite)
- [ ] `wubu_verifier` — test-suite-as-verifier → activates the AGI promote
      loop (currently dormant: verifier=NULL on metal)
- [ ] `wubu_holyd_metal` — HolyC JIT REPL daemon on metal (needs libc
      strtod/strtol + exec memory)

## P1 — hangs off P0
- [ ] `wubu_segments` / `wubu_store` — single-level store (persistent
      segments + checkpoints, Phantom-style resume)
- [ ] `wubu_net` — e1000 port + IPv4/TCP/UDP + DNS + HTTP(S) on metal
- [ ] `wubu_anticheat` — core-integrity scan, runtime PCRs, overlay scopes,
      IOMMU policy
- [ ] `wubu_personality` — personality registry + tier router
      (absorb-everything)
- [ ] `wubu_capguard` — capability guards on the Colonel's core memory
- [ ] `wubu_editor` — live text editor (Genera feel) on fb + serial
- [ ] `wubu_human` — hedged human-command gateway (allowlist + audit)

## Discipline
Every module ships with: its header as the interface contract, a
compendium entry (generated reference + addendum notes), and console
assertions for the metal boot test.
