# 🎯 WuBuOS Session — Next Kickoff (copy from BATTLESHIP v21)

**Baseline (re-confirmed 2026-07-07):** 747+ tests GREEN / 64 targets. `make test_high_gui` ✅ (incl. new `test_wallpaper` 18/18).

## Standing orders (immutable)
- C11 only · opaque structs · minimal includes · **no god headers** · every module self-contained
- Every edited function does real work or is marked TODO. No stubs / scaffolding / "for later".
- **"Rewriting from scratch in C" = the point of the project → anything under that is reclassified as work = REAL_GAP.**
- Stop not allowed. Blocked → alternate paths. Tests must pass after changes.

## Scoreboard (v21)
- **~400 sprint REAL_GAPs** (form≠function, triple-DA filtered). Per-file top: wubu_metal(70), vsl_syscall_net(67), styxfs(65), interrupt(54), vsl_syscall_fileio(53), vsl_syscall_proc(47), wubu_network(46), styx(45), wubu_syscall(36), wubu_snapshot(34).
- **5 parity epics** (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS) = marathons, not in the 400.

## This cycle's FOCUS (user directive)
1. **Desktop fixup** — Streams 2-4: persistent icon layout (save/restore via wubu_settings), working context menu (sort-by-name real, create-shortcut writes real .desktop, view toggles auto-arrange), Control Panel Desktop tab goes LIVE (was verified stub). Plan: `DESKTOP_FIXUP_PLAN.md`.
2. **Monolith situation** — split ≥800-line files with opaque structs + C11 + no god headers. Table in BATTLESHIP.md. Rule: opaque struct in `foo_internal.h`, static-inline helpers, Makefile 4 edits (OBJS + host link + test direct + runtime). Verify `make clean && make runtime && make hosted` before done.

## Commands
```bash
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
make clean && make runtime && make hosted   # build gate
make test_high_gui                          # desktop gate (incl. wallpaper)
make test                                   # full gate (64 targets)
```

## Kickoff options (parallel, all valid)
- **A**: Desktop Stream 2 (persistent layout) → Stream 3 (context menu) → Stream 4 (Control Panel tab)
- **B**: Monolith split `wubu_metal.c` (1508) → `wubu_metal_{drm,vulkan,x11,audio,shutdown}.c`
- **C**: Close sprint-board gaps in `vsl_syscall_net.c` / `styxfs.c` / `interrupt.c`
- **D**: Parity epic — ReactOS NT → VSL transliteration (`ntoskrnl/ke/thrdschd.c` → `tasking.c`)

All "rewrite in C". Pick one or more. Execute until 400 → 0.
