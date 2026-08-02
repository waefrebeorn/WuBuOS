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

## P1 — graphic set system + translation layers (Kevin-Bacon pass 3)
- [ ] `/theme` namespace — the graphic set as Styx9 nodes (colors, chrome,
      fonts, sprites, layout); the compositor reads it every frame
- [ ] Damage-rect compositor (WuBuFX Phase G) — dirty-region rendering so
      live re-skins are cheap
- [ ] AGI-writable graphic set — the Colonel writes /theme through the
      capability gate (EDR-disclosed); the desktop re-skins live
- [ ] GameInput-style unified input — one event model for all devices
      (keyboard/mouse/gamepad/controller), common time base
- [ ] GDI/DirectX → /theme translation shim — the DXVK lesson: one
      translation target per domain
- [ ] VSL speed doctrine — in-process dispatch (Wine 11 lesson), batching,
      zero-copy where possible

## Discipline
Every module ships with: its header as the interface contract, a
compendium entry (generated reference + addendum notes), and console
assertions for the metal boot test.
