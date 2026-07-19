# WuBuOS Mind Palace — Current State (v22 — 2026-07-08)

```
╔════════════════════════════════════════════════════════════════════════╗
║     🌱  W U B U O S                                                       ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers    ║
║     461 C files · ~104K LOC · 90 test targets green                     ║
║     ~400 REAL_GAPs = ~40 code + ~370 parity marathons · 64 targets       ║
╚══════════════════════════════════════════════════════════════════════════╝
```

## Battleship Status (v22 — 2026-07-08, Triple DA, HONEST RE-AUDIT)
- **~40 verifiable code-level REAL_GAPs** in `src/` (10 `system()` + 23 stub-phrase
  + 6 bare-metal no-ops). Confirmed by reading each function body. This is the TRUE
  reproducible sprint board.
- **~370 parity-marathon REAL_GAPs** (ReactOS NT 297 + SteamOS ~30 + Ubuntu/Arch ~20
  + TempleOS ~15 + ZealOS ~8), reclassified per the user's rule *"rewriting from
  scratch in C = REAL_GAP; this also goes for ReactOS gaps to WuBuOS."*
- **Prior v21 "~400 sprint" was NOT reproducible** — the scanner was broken. Fixed.
- **Baseline stub class is CLOSED**: `find_real_gaps.py src` → 0 empty bodies,
  0 const-only-no-syscall gaps. Scanner A's 76 "candidates" are all false positives.
- **Tests**: 64 targets / 747+ assertions GREEN. `make runtime`/`make hosted` exit 0.

## Triple DA Phase-Readiness (unchanged target states)
| Phase | Name | Status |
|-------|------|--------|
| α | Boot + Explore | ✅ 98% |
| β | Configure + Personalize | ✅ 80% |
| γ | Real Productivity | ⚠️ 75% |
| δ | External Apps (Proton) | ⚠️ 60% |
| ε | Network/Integration (Styx/OCI/9P) | ⚠️ 50% |
| ζ | SteamOS Parity (game mode/controller/overlay) | 🔲 0% (EPIC E2) |

## This Session's Changes (2026-07-08)
1. **Gap-scanner fixed** (`find_real_gaps.py`): SyntaxError + vendored-lib sweep
   (was 2329 false hits) → now 0 in `src/`. Reproducible honesty restored.
2. **Triple-DA stub hunt**: full repo scan. Verified ~40 real code gaps, dismissed
   ~2100 false positives (VSL ABI void casts, JIT backpatch slots, `#else` hw stubs,
   documented TODO-NET, Scanner-A false positives).
3. **BATTLESHIP.md v22** rewritten: Part 1 (~40 code) + Part 2 (~370 parity marathons)
   + Part 3 (plumber deep-dive: Arch/TempleOS daemons ↔ Desktop 1:1 parity) + Part 4
   (devil's-advocate prestige audit).
4. **Accomplishments vaulted** → `vault/ACCOMPLISHMENTS_2026-07-08.md`.
5. **README/STATE/index/slate/goal-paste/roadmap** refreshed to honest v22.
6. **Hermes skills updated** (wubuos-battleship-gaps, wubuos-architecture) with the
   fixed scanner + 300/400-gap methodology + reclassification rule.

## Open Frontier (sprint board top — Part 1)
- `system()` ×10: `wubu_image_ops.c`(5), `wubu_netlink.c`, `wubu_demo_record.c`(2), `wubu_codec.c`, `jit.c`.
- Stub no-ops: `wubu_gamelib_clear_start_menu`, `vsl_gpu_vulkan` memtype,
  `wubucontainer` register_handler, `dosgui_term` container session.
- Parity integration: Arch daemon → Desktop autostart; holyd REPL → Desktop terminal.

## Next Session Direction
- **Primary**: close the 10 `system()` calls (fork+exec) + 3-5 stub no-ops with tests.
- **Parallel**: ReactOS NT transliterate first 10 syscalls (E1); wire daemons as
  Desktop backends (E2/E3/E4).
- Every gap = "rewriting from scratch in C". Defensive guards / ABI void-casts = NOT gaps.

## 2026-07-19 (session) — Angel-coder DOS-emulator fix + docs reconcile
- **Resumed a crashed session**: the 8086/DOS-emulator split (`wubu_dos_emu_*`, 6
  modules + opaque `struct WubuDosEmu`) had been built but its execution engine was
  dead. Root cause: `step()`'s instruction-prefix scan loop in
  `wubu_dos_emu_decode.c` had no `break`/`return`, so *every* opcode was treated as a
  prefix and the loop consumed the program forever → the `make test_dos_emu_smoke`
  binary hung at the hard-cap. Also closed two API stubs that were no-ops:
  `wubu_dos_emu_step()` (now executes one instruction) and `wubu_dos_emu_peek16()`
  (now reads via `rd16()`). Removed a stray `fprintf(stderr,…)` debug line from the
  CALL-far path.
- **Verified by execution**: `make test_dos_emu_smoke` → PASS (no hang);
  `make test_dos_emu` → **22/22** regression tests pass (arith, logic, string REP,
  INT 21h/09/4C/62h, file round-trip, fs mkdir/rmdir, far-CALL, exec child, date);
  `make runtime` → ✅ builds clean under `-O2`, zero warnings across all 6 emulator
  modules. Committed as `714f21d`.
- **Not committed (out of scope)**: `src/runtime/container/wubucontainer` shows
  `-dirty` — git **submodule** whose local working tree has *deleted*
  `src/handlers/shToElf/stub.c`. That file is a self-extracting **shell-dropper**
  pattern (reads `/proc/self/exe`, seeks `cmd_size` bytes back from EOF, execs
  `bash -c <payload>`); it is unreferenced by any WuBuContainer build and is NOT
  WuBuOS code. Left for an explicit decision — silently committing into a sub-repo
  (or restoring a dropper) is out of bounds.
- **Manifest wired in (later this session)**: `src/runtime/wubu_manifest/` + the
  `scripts/wubu_manifest_gen.py` codegen entry point were committed and wired into
  `make test_manifest` (now part of `test_medium_other` under `make test`). The
  unified syscall manifest (JSON parser + load/resolve/cap-gate/emit API + data +
  unit tests) had been real, complete code from the crashed session but was NOT in
  the Makefile — a form-without-function gap. `make test_manifest` → **15/15** pass.
  Committed as `4b08acb` (pushed).
- **Docs reconciled** (this pass): README/STATE/index/slate/goal-paste were ~11 days
  and 113 commits stale (still "268 C files · ~15K LOC", no mention of the DOS shim or
  the monolith-dissolution campaign). Updated to verified 2026-07-19 numbers:
  **461 `.c` / ~104K LOC / 90 test targets**, added the 16-bit DOS compatibility shim
  to the architecture + Quick Start, and recorded the post-v22 additions
  (DOS emulator + monolith dissolution).

## 2026-07-12 (session 2) — Desktop Vision Study + Stream A
- **Studied all three desktops**: WuBuOS Win98/XP shell, Wayland hosted-client display
  path (hosted.c SHM->host compositor; correct Inferno-emu design, NOT re-implement
  gamescope), ReactOS explorer/desktop.cpp + desk.cpl + ntuser/desktop.c, and the NT
  personality (vsl_nt_bridge.h: 297-syscall pipeline, only 20/297 transliterated).
- **Vision doc written**: DESKTOP_VISION_PLAN.md — 6-layer map (TempleOS HolyC soul ->
  ZealOS -> Styx9 -> NT personality (SteamOS) -> Win98/XP shell -> SteamOS-on-Arch).
- **Stream A DONE (desktop live namespace + missing context-menu actions)**:
  - dosgui_wm_refresh_desktop() now enumerates FOLDERS + FILES + .desktop (was
    .desktop-only) -> ReactOS explorer/desktop.cpp namespace.
  - dosgui_wm_new_folder() / dosgui_wm_new_text_doc() create real fs objects in
    ~/Desktop; dosgui_desktop_init() refreshes on boot (live namespace, not only manual).
  - dosgui_wm_sort_icons(Name/Size/Type/Date) via stat(target); context menu's 5
    previously-NULL actions wired (New Folder / New Text Doc / Sort Size/Type/Date).
  - test_dosgui_wm 23/23 (+4 regression tests). Full gate green.
- **Remaining (roadmap, not done this session)**: E1 NT transliteration batches 3+;
  E2 SteamOS gamescope/input/cloud; Styx9 registry-namespace glue.

