# 🎯 WuBuOS Session — Next Kickoff (v26 — 2026-07-08)

**Baseline:** 747+ tests GREEN / 64 targets. `make runtime` + `make hosted` exit 0 (`c475263`).

## The Goal Mantra (2026-07-08 — reclassification)
> **"Rewriting from scratch in C is the point of the project. Anything that falls
> under that is reclassified as work REAL_GAP. This also goes for ReactOS gaps to
> WuBuOS."**
>
> - No stubs. No scaffolding. No "for later". No "for brevity". No "stub for extension".
> - Every parity subsystem (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS NT)
>   is a REAL_GAP marathon = "rewrite that subsystem in C from scratch."
> - Form≠Function = gap. A function that looks done but does no real work is a gap.
> - Defensive guards + 6-register ABI void casts = NOT gaps (don't chase them).

## What was DONE v25→v26 (this session)
```
- Fixed gap-scanner find_real_gaps.py (was broken: SyntaxError + 2329 vendor false hits)
- Triple-DA stub hunt: ~40 verifiable code gaps + ~370 parity marathons (honest ~400)
- BATTLESHIP.md v22: Part1 (~40 code) + Part2 (~370 parity) + Part3 (plumber deep-dive) + Part4 (DA audit)
- Vaulted ACCOMPLISHMENTS_2026-07-08.md
- Refreshed README/STATE/index/slate/goal-paste/roadmap to honest v22
- Updated Hermes skills (wubuos-battleship-gaps, wubuos-architecture)
- Triple DA plumber deep-dive: Arch daemon + TempleOS DOS daemon ↔ WuBuOS Desktop 1:1 parity
```

## The Honest Board
| Bucket | Count | Source |
|--------|-------|--------|
| Code-level (verified) | ~40 | 10 `system()` + 23 stub-phrase + 6 bare-metal no-op |
| Parity marathons | ~370 | ReactOS NT 297 + SteamOS ~30 + Ubuntu/Arch ~20 + TempleOS ~15 + ZealOS ~8 |
| **Total** | **~410 ≈ 400** | reclassified per mantra |

## Next Cycle (execute, don't plan)
1. Close the **10 `system()` calls** → fork+exec (start `wubu_image_ops.c` ×5).
2. Close **3-5 stub no-ops** with regression tests (`wubu_gamelib_clear_start_menu`,
   `vsl_gpu_vulkan` memtype, `wubucontainer` register_handler, `dosgui_term` container session).
3. **Wire Arch daemon as Desktop autostart/service manager** (E3 integration).
4. **Embed holyd REPL into Desktop terminal** (E4; kills `dosgui_term.c:642` Not-Implemented).
5. **ReactOS NT: transliterate first 10 syscalls** (E1) — each = "rewrite in C."

## Reproducible verification (run before trusting any handoff)
```bash
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
python3 ~/.hermes/profiles/mind-palace/skills/software-development/wubuos-battleship-gaps/scripts/find_real_gaps.py src
# → EMPTY {} bodies: 0   CONST-ONLY no-syscall: 0
grep -rEn '\bsystem\s*\(' src --include='*.c' | grep -vE '_test\.c|test_|replaces system' | wc -l   # → 10
make runtime && make hosted && make test   # gate green
```

## Mantra reminders
- Stale handoff red-lists are hypotheses, NOT ground truth. Re-run the target first.
- Per-file void-cast counts are NOT a to-do list (155 in vsl_syscall_* are ABI).
- Every edited function does real work or is marked TODO. Tests + build must pass.
