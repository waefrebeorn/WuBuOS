# WuBuOS Slate — Active Work Surface (v24)

## Current Focus: **SPRINT BOARD: ~363 REMAINING — ALL TODOs CLOSED**
**All 4 TODO/FIXME gaps in wubu_pkgmgr.c closed.** The entire `src/` tree has 0 remaining `/* TODO: ... */` comments.
**Mode**: Perpetual gap-closer loop — execute until ~363 → 0.
**Constraint**: "Rewriting from scratch in C" — no stubs, no scaffolding, no "for later".

---

## ✅ VAULTED (2026-07-08)
| Date | Work | Δ Real Gaps | Files |
|------|------|-------------|-------|
| 2026-07-08 | JIT encoder: 33 return-0 stubs → real byte counts | -33 | `wubu_x86.c`, `jit_test.c` |
| 2026-07-08 | Pkgmgr: resolve_deps, clean_cache, autoremove, verify_installed | -4 | `wubu_pkgmgr.c`, `wubu_pkgmgr_internal.h` |
| 2026-07-07 | UX Stream E: welcome dialog, bundled wallpaper, status bar tips | — | `wubu_welcome.*`, `wubu_wallpaper.*` |
| 2026-07-07 | Desktop Stream 3: context menu fully implemented | — | `dosgui_wm_ctxmenu.c` |
| 2026-07-05 | Monolith splits: ALL 14+ complete | — | vsl_syscall, holyc_codegen, wubu_holyd, wubu_audio, wubu_network, wubu_snapshot, wubu_pkgmgr, dosgui_wm, dosgui_explorer |
| Prior | Foundation 1-5 + Campaign 1-25 | ~650 | All subsystems |

---

## Active Work Items — NEXT CYCLE

### HIGHEST PRIORITY
**A. VSL syscall void-cast files** (~140 total, 3 files):
- `vsl_syscall_net.c` — 58 void casts (socket/ns/security)
- `vsl_syscall_fileio.c` — 46 void casts
- `vsl_syscall_proc.c` — 37 void casts
Each `(void)d; (void)e; (void)f;` represents a real syscall with unused register params.

**B. Monolith splits** — remaining ≥800-line files:
`wubu_metal`(1508), `hosted.c`(1320), `interrupt.c`(1153), `fat32.c`(1060),
`wubu_archd.c`(1055), `wubu_proton.c`(1053), `wubu_vulkan.c`(990), `wubu_canvas.c`(1325).

**C. Bear RL backend** — `bear_cudnn.c` edge-case fallback gaps.

---

## Notes
- **73 .c files**, ~15K real LOC.
- **~363 sprint REAL_GAPs** remaining (was ~400 before this session).
- **0 TODO/FIXME** comments remain in `src/`.
- All tests green. 64+ test targets, 747+ assertions. Full gate exits 0.
- `goal-paste.md` has the next-session copy-paste prompt.
- WuBuOS: ZealOS kernel + Win98 shell + Styx/9P namespace + Arch containers.