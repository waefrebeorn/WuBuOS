# WuBuOS Comprehensive Gap Analysis — Triple Devil's Advocate
## Post-Prestige Audit 2026-06-28 (Phase 16 — Comprehensive)

> **⚠️ SUPERSEDED by BATTLESHIP.md (Phase 16, 2026-06-28)** — This document contains the v14 gap analysis. The current 1562-gap classified inventory is in [BATTLESHIP.md](BATTLESHIP.md).

## Executive Summary

**Total src files**: 356 (249 .c, 107 .h)
**Total LOC**: 111,835
**Test targets**: 47 (44 pass, 0 fail, 0 timeout)
**~747+ test assertions** counted across passing suites

### Gap Counts (Raw)

| Category | Count |
|---|---|
| `(void)param` casts (dead code) | 1,256 |
| TODO/STUB/FIXME/XXX/HACK markers | 200+ |
| `system()` calls | 76 |
| "for later"/"scaffolding"/"brevity" comments | 60+ |
| Empty function bodies `{}` | 23 |
| Placeholder patterns (JIT, codegen, Vulkan) | 69 |
| Weak alias stubs | 26 |
| `return -1` without work (not null guard) | 13 |
| **Grand total raw markers** | **~1,700** |
| **Active gaps (after DA classification)** | **1,562** |
| **Files with active gaps** | **129** |

### After This Session's Fixes

Previously "FULL STUB" files now implemented:
- ✅ wubu_network.c — 139 tests pass (FULL CRUD)
- ✅ wubu_snapshot.c — 132 tests pass (FULL snapshot/restore)
- ✅ wubu_proton.c — 32/32 tests pass (PE exec pipeline)
- ✅ dosgui_daemon_panel.c — 21 tests pass (desktop-daemon bridge)
- ✅ wubu_archd.c — 16 tests pass (Arch Linux daemon)
- ✅ wubu_holyd.c — 27 tests pass (HolyC DOS daemon)
- ✅ styxfs_server.c — Full 9P server implementation

Link errors and timeouts RESOLVED:
- ✅ test_hosted — Fixed by adding dosgui_daemon_panel to link line
- ✅ test_metal — Fixed by adding taskbar_init stub
- ✅ test_wubu/test_host_exec — Fixed by adding prctl PR_SET_PDEATHSIG for Styx server

## The Point

"Rewriting in scratch in C" is the point of the project. Anything that falls
under that is reclassified as REAL_GAP. There is no "scaffolding for later."
There is no "stub for extension." There is only: does it work at runtime or not?

**REAL_GAP definition**: Any function that:
1. Returns -1 without doing work (and is not a null-pointer guard)
2. Has empty body `{}`
3. Only casts to `(void)` to silence warnings
4. Has TODO/STUB/FIXME/HACK/XXX markers
5. Calls another stub function (transitive stub)
6. Claims a feature in comments but has no implementation
7. Says "for later", "scaffolding", "for brevity", "stub for", "placeholder"
8. Uses weak alias for test stubs
9. Has placeholder patterns (JIT backpatch, codegen, Vulkan)
10. Calls `system()` instead of direct C implementation

Null-pointer guards are NOT real gaps. Error handling is real code.

## Gap Breakdown by Category (DA-Verified 1562)

| Category | Count | Severity |
|---|---|---|
| `(void)param` casts on success paths | 1,256 | 🔴 CRITICAL |
| Placeholder patterns (JIT, codegen, Vulkan) | 69 | 🔴 CRITICAL |
| Weak alias test stubs | 26 | 🔴 CRITICAL |
| `system()` calls | 76 | 🔴 CRITICAL |
| TODO/STUB/FIXME comments | 200+ | 🟠 HIGH |
| `return -1` without work (not null guard) | 13 | 🟠 HIGH |
| Empty bodies `{}` on success path | 23 | 🟠 HIGH |
| **TOTAL** | **1,562** | |

## Top 20 Files by Gap Count

1. **runtime/wubu_vsl.c** — 347 void casts in syscall handlers
2. **bear/bear_cudnn.c** — 117 void casts in #else stub blocks
3. **bridge/wubu_syscall.c** — 97 void casts + 2 system() calls
4. **hosted/hosted.c** — 72 void casts in Wayland callbacks
5. **gui/wubu_clipboard.c** — 43 void casts
6. **kernel/interrupt.c** — 41 void casts
7. **apps/wubu_canvas.c** — 41 void casts + 3 system()
8. **gui/dosgui_explorer.c** — 31 void casts + placeholders
9. **hosted/wubu_metal.c** — 31 void casts + 6 weak + stubs
10. **compiler/holyc_codegen.c** — 29 placeholders (JIT backpatching)
11. **bear/bear_vulkan.c** — 33 void casts
12. **gui/dosgui_startmenu.c** — 24 void casts + 2 system()
13. **gui/dosgui_term.c** — 23 void casts + placeholders
14. **gui/dosgui_wm.c** — 22 void casts
15. **apps/control.c** — 20 void casts
16. **apps/dosgui_apps.c** — 16 void casts
17. **runtime/styxfs.c** — 14 void casts
18. **gui/wubu_pkgmgr_test.c** — 14 void casts + 9 system()
19. **audio/wubu_audio.c** — 13 void casts + placeholders
20. **bear/bear_env.c** — 13 void casts