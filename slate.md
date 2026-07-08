# WuBuOS Slate — Active Work Surface (v22)

## Current Focus: **UX POLISH (Stream E) + SPRINT BOARD + DOCUMENTATION OVERHAUL COMPLETE**
**Sprint board**: ~400 REAL_GAPs (Triple DA, form≠function filtered) — the actionable file-by-file board.
**Parity epics**: 5 (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS) — marathons, NOT in the 400.
**Mode**: Perpetual gap-closer loop — execute until 400 → 0 (sprint) + epics progress.
**Constraint**: "Rewriting from scratch in C" — no stubs, no scaffolding, no "for later".
**Execution**: PARALLEL — sprint board + Stream 3 context menu + monolith splits + parity epics.

---

## ✅ VAULTED (2026-07-07 end)
- Foundation 1-5 (~650 gaps) + Campaign 1-25 (~650 gaps) — all closed, tests green.
- Desktop wallpaper (2026-07-07): `wubu_wallpaper.{h,c}` real BMP decode + 5 ReactOS placement modes; 18/18 test.
- Pre-existing test failures CLOSED: `test_holyc` 84/84, `test_styxfs` 11/11, `test_syscall` 5/5.
- **Monolith splits ALL COMPLETE** — dosgui_explorer, dosgui_wm, holyc_codegen, vsl_syscall, wubu_audio, wubu_holyd, wubu_network, wubu_snapshot, wubu_pkgmgr.
- **Documentation overhaul** (2026-07-07): screenshots/ README, triple DA phase table in STATE.md, all docs linked.
- **UX Stream E** (2026-07-07): welcome dialog, bundled wallpaper, status bar tips — all wired, all green.

---

## Active Work Items — NEXT CYCLE

### HIGHEST PRIORITY
**A. Desktop Stream 3 — working context menu** (`dosgui_wm_ctxmenu.c`):
- `ctx_action_sort_by_name()` → empty body → real alphabetical grid reflow
- `ctx_action_create_shortcut()` → notify stub → writes real `.desktop` to `~/Desktop`
- `ctx_action_view_desktop()` → empty → toggle auto-arrange flag + re-layout

### SECONDARY
**B. Monolith splits** — remaining ≥800-line files: wubu_metal(1508), hosted.c(1320), interrupt.c(1153), fat32.c(1060), wubu_archd.c(1055), wubu_proton.c(1053), wubu_network.c(996), wubu_vulkan.c(990), wubu_canvas.c(1325).

**C. Sprint board ~400** — top files: wubu_metal(70), vsl_syscall_net(67), styxfs(65), interrupt(54), vsl_syscall_fileio(53), vsl_syscall_proc(47), wubu_network(46), styx(45), wubu_syscall(36), wubu_snapshot(34).

---

## Notes
- 73 .c files, ~15K real LOC. Real work LOC ≈ 15K (was ~123K inflated).
- **~400 sprint REAL_GAPs** (honest, form≠function). Previous "~3000" (2026-07-05) double-counted parity-% + defensive guards → reclassed.
- All tests green. 64 test targets, 747+ assertions. Full gate exits 0.
- `screenshots/README.md` catalogs all media with naming convention.
- `STATE.md` has triple DA phase-readiness table (α-ζ).
- `goal-paste.md` has the next-session copy-paste prompt.
