# 00-philosophy — The Documentation Philosophy (AGI-OS era)

*Addendum entry. Human-written. Last updated 2026-08-02 (the docs DA).*

## The virtue

**The compendium is the AGI's institutional memory.** Not a developer
appendix, not a wiki to be maintained begrudgingly — the generated
reference + the prestige ledger + the addendum philosophy ARE the
recursive-learning substrate the whole project runs on. The Colonel reads
it, the generations write it, and `make docs` keeps it true.

## The five rules (updated)

1. **Generated docs must be RUN, not listed.** Every file in
   `01-reference/` is the output of a scanner that EXECUTES something:
   - `modules.md` — walks the entire source tree (all 16 dirs, 582 modules)
   - `tests.md` — RUNS the curated host tests and records PASS/FAIL
   - `state.md` — boots the metal kernel in QEMU and records the live
     vitals (tick, promoted_total, attestation, /theme writes)
   - `parity.md` — inspects the hosted OS legs + the portability matrix
   A scanner that only counts lines is a list; a scanner that boots the OS
   is evidence.
2. **The scanners catch regressions by construction.** `make docs` failing
   a test target is a real signal (this batch: the new `agi_theme_step`
   broke `test_agi_kernel` — the scanner surfaced it, the stub fixed it).
3. **The AGI-OS integration is the organizing principle.** The sections map
   to the four levels: 00-philosophy (the Colonel's values), 02-architecture
   (the levels), 03-learned (the prestige ledger — recursive learning),
   04-roadmap (the DA audits + the sub-module plan). The reference feeds
   the AGI's context; the ledger is what it learns from.
4. **The addendum half is human and must stay human.** Philosophy,
   decisions, and worked/didn't-work are judgment, not output. The
   TEMPLATE keeps them honest (evidence first, never delete, say whether
   "didn't work" is a hypothesis or settled).
5. **PARITY is a first-class doc citizen.** The hosted layer is the
   scaffold that must run on Linux/Windows/macOS with parity; `parity.md`
   tracks the portability matrix as generated evidence, and every hosted
   feature states which leg it runs on.

## The way forward (integrating with the AGI OS)

- The docs grow a machine-readable surface: the generated reference is
  already plain tables the Colonel can consume; the ledger entries carry
  dates + evidence for the recursive loop.
- The state scanner becomes a heartbeat: `make docs` records what the OS
  was doing at generation time, so every doc generation is also a system
  snapshot.
- The verifier doctrine extends to docs: a change that breaks `make docs`
  or a curated test is not promotable.
