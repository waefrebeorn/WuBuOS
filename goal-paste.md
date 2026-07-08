# 🎯 WuBuOS Session — Next Kickoff (v22 — 2026-07-07 end)

**Baseline:** 747+ tests GREEN / 64 targets. `GATE_EXIT=0` across all tiers.

## What was DONE last session
- **UX Stream E**: welcome dialog (wubu_welcome.h/.c), bundled default wallpaper (screenshots/media/wubuos-default.bmp → wubu_wallpaper_default_path()), status bar cycling tips (dosgui_wm taskbar)
- **Documentation overhaul**: screenshots/ directory w/ README, triple DA phase table in STATE.md, screenshot embed in README+index+STATE, old media deleted
- **Triple DA phase-readiness audit**: α-ζ phase model (α 98% shippable, β 80%, γ 75%, δ 60%, ε 50%, ζ 0%)
- **Exemplar OS study**: 5 OS families → lessons → wubuos-masterpiece-architecture skill updated
- **Makefile link fixes**: test_control + test_dosgui_wm wiring for wubu_session_launch_game (Play action deps)
- **Commits**: `28e700b` (UX features) + `19bcec8` (docs/media) + `4f03614` + `f7f9aa5` (Makefile fixes)

## Standing orders (immutable)
- C11 only · opaque structs · minimal includes · **no god headers** · every module self-contained
- Every edited function does real work or is marked TODO. No stubs / scaffolding / "for later".
- **"Rewriting from scratch in C" = the point of the project → anything under that is reclassified as work = REAL_GAP.**
- Stop not allowed. Blocked → alternate paths. Tests must pass after changes.

## Scoreboard
- **~400 sprint REAL_GAPs** (form≠function, triple-DA filtered). Per-file top: wubu_metal(70), vsl_syscall_net(67), styxfs(65), interrupt(54), vsl_syscall_fileio(53), vsl_syscall_proc(47), wubu_network(46), styx(45), wubu_syscall(36), wubu_snapshot(34).
- **5 parity epics** (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS) = marathons, not in the 400.

## HIGHEST PRIORITY — Desktop Stream 3: working context menu
Three empty-body functions in `dosgui_wm_ctxmenu.c` that must become real:

1. `ctx_action_sort_by_name()` — currently `/* Would sort icons by name */` EMPTY. Must reflow grid alphabetically.
2. `ctx_action_create_shortcut()` — currently shows a notify stub. Must write real `.desktop` to `~/Desktop` via Styx/9P, then re-enumerate.
3. `ctx_action_view_desktop()` — currently EMPTY. Must toggle auto-arrange flag + re-layout.

These are the LAST remaining desktop polish items before the whole shell is feature-complete.

## SECONDARY
- **Monolith splits**: wubu_metal(1508), hosted.c(1320), interrupt.c(1153), fat32.c(1060), wubu_archd.c(1055), wubu_proton.c(1053), wubu_network.c(996), wubu_vulkan.c(990), wubu_canvas.c(1325)
- **Sprint board ~400**: close real form≠function gaps file-by-file
- **Parity epics**: ReactOS NT → VSL transliteration (pick a syscall and implement it)

## Commands
```bash
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
make clean && make runtime && make hosted   # build gate
make test_high_gui                          # desktop gate
make test                                   # full gate (64 targets)
```

## Key docs
- `screenshots/README.md` — media catalog
- `STATE.md` — triple DA phase table + recent changes
- `slate.md` — v22 active work surface
- `.hermes/plans/2026-07-07_doc-media-roadmap-overhaul.md` — full plan

## Skills
- `wubuos-masterpiece-architecture` — updated with exemplar OS study + phase table
- `wubuos-test-suite` — updated test target list + stale entries cleaned
- `wubuos-monolithic-split` — proven pattern for splitting files

## Kickoff options (parallel, all valid)
- **A 🔥**: Desktop Stream 3 — real context menu (sort, create-shortcut, view-desktop)
- **B**: Monolith split wubu_metal.c (1508 lines) or hosted.c (1320 lines)
- **C**: Close sprint-board gaps in vsl_syscall_net.c / styxfs.c / interrupt.c
- **D**: ReactOS NT → VSL — pick one syscall (`NtCreateFile`, `NtOpenProcess`, etc.) and transliterate

All "rewrite in C". Execute until 400 → 0.