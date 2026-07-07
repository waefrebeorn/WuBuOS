# WuBuOS Mind Palace — Current State (v21)

```
╔════════════════════════════════════════════════════════════════╗
║     🌱  W U B U O S                                       ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace   ║
║     73 C files · ~15K real LOC · 747+ tests green    ║
║     ~400 sprint REAL_GAPs · 5 parity epics · 64 targets  ║
╚════════════════════════════════════════════════════════════════╝
```

## Battleship Status (v21 — 2026-07-07)
- **~400 sprint REAL_GAPs** (Triple DA, form≠function filtered). Previous "~3000"
  (2026-07-05) double-counted parity-% + defensive guards → reclassed.
- **5 parity epics** (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS) tracked
  as marathons above the sprint board, NOT in the 400.
- **Tests**: 747+ assertions / **64 targets** GREEN (re-confirmed this session).
- **Accomplishments vaulted** → `vault/ACCOMPLISHMENTS_2026-07-07.md`.

## This Session's Changes (2026-07-07, session 2 — bug-closer)
1. **3 pre-existing test failures CLOSED** (full `make test` gate now exits 0):
   - `test_holyc` (84/84): two HolyC JIT SIGSEGV bugs — (a) `jit_lock_exec` made
     the data section RX but globals store into it at runtime → crash; fix only
     locks when `data_size==0`. (b) `++/--`/`&x` on module-level globals only
     emitted stack `[rbp-off]`; added `emit_global_load_rax`/`emit_global_store_rax`
     RIP-relative helpers + `&x` lea, recording `global_patches`. `hc_eval` now
     applies the patch loop. See skill `wubuos-holyc-compiler`.
   - `test_styxfs` (11/11): Makefile link line omitted `holyc_codegen_api/emit/
     expr/stmt` → undefined `hc_eval`; added full compiler backend.
   - `test_syscall` (5/5): `-DMYSEED_METAL` w/o `_GNU_SOURCE` hid `struct sigaction`;
     added `-D_GNU_SOURCE`.
2. **holyd persistent-var bug fixed** (prior session's carryover): `VAR_DECL`
   module-level discriminator now `!in_function` (new `bool in_function` in
   `HCGen`), since `emit_prologue` runs before `gen_stmt` made `has_prologue`
   always-true → top-level vars misrouted to stack → garbage across evals.
3. **Committed**: `6d9824d` (compiler+Makefile fixes only; GUI/wallpaper/legacy
   refactor left uncommitted as separate WIP).

## This Session's Changes (2026-07-07)
1. **Desktop wallpaper (REAL_GAP closed)**: new `src/gui/wubu_wallpaper.{h,c}` —
   real 24/32-bit BMP decode → XRGB8888 + 5 ReactOS PLACEMENT modes. Wired into
   `dosgui_wm.c` `load_default_wallpaper()` (now reads `wubu_settings→wallpaper_path`)
   + `draw_wallpaper()`. `test_wallpaper` 18/18 PASS, added to `test_high_gui`.
2. **Dead-code purge**: quarantined 5 legacy files + `vbe_legacy.h` → `src/_legacy_bak/`.
3. **Stub hunt re-run** (form≠function): 455 void-cast / 1166 return-const / 99 stub
   phrases in src (excluding tests). After filtering defensive+ABI → ~400 real.
4. **BATTLESHIP/STATE/README/banner/goal-paste/index/slate** refreshed to v21.
5. **Monolith table** rebuilt: 25 files ≥800 lines listed with split plans.

## Open Frontier (sprint board top)
`wubu_metal.c`(70) · `vsl_syscall_net.c`(67) · `styxfs.c`(65) · `interrupt.c`(54) ·
`vsl_syscall_fileio.c`(53) · `vsl_syscall_proc.c`(47) · `wubu_network.c`(46) ·
`styx.c`(45) · `wubu_syscall.c`(36) · `wubu_snapshot.c`(34) — full list in BATTLESHIP.md.

## Monolith focus (this cycle)
Opaque structs + minimal includes + C11 + no god headers. Remaining ≥800-line files:
`dosgui_explorer.c`(✅render split) · `wubu_metal.c`(1508) · `wubu_canvas.c`(1325) ·
`hosted.c`(1320) · `dosgui_wm.c`(✅3 splits) · `dosgui_startmenu.c`(1217) ·
`interrupt.c`(1153) · `styxfs.c`(✅voids closed) · `fat32.c`(1060) · `wubu_archd.c`(1055) ·
`wubu_proton.c`(1053) · `wubu_network.c`(996) · `wubu_vulkan.c`(990) · many more — see table.

## Next Session Direction
- **Primary**: close desktop gaps Streams 2-4 (persistent icon layout, real context
  menu, Control Panel Desktop tab live) — see `DESKTOP_FIXUP_PLAN.md`.
- **Parallel**: monolith splits per table; sprint-board ~400 file-by-file.
- Every gap = "rewriting from scratch in C". Defensive guards / ABI void-casts = NOT gaps.
