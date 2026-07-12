# WuBuOS Mind Palace — Current State (v22 — 2026-07-08)

```
╔════════════════════════════════════════════════════════════════════════╗
║     🌱  W U B U O S                                                       ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers    ║
║     268 C files · ~15K real LOC · 747+ tests green                       ║
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

