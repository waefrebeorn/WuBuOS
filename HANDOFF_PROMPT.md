# WUBUOS PRESTIGE PHASE — Continuation Prompt

## Current State
- Build: make all -> pass (0 errors, 0 warnings)
- Tests: All 58 targets green (0 failures)
  - VSL 52/52 ✅ (lseek now real)
  - HolyD 31/30 ✅ (JIT eval wired to hc_eval)
  - HolyC 76/76 ✅
  - OCI 10/10 ✅
  - Bottles 16/16 ✅
  - Network 139/139 ✅
  - Snapshot 132/132 ✅
  - Daemon panel 21/21 ✅
  - All others green ✅
- LOC: ~98K across 240 .c + 107 .h files
- Project path: /home/wubu/.hermes/profiles/mind-palace/home/myseed/

## What Was Last Built
- wubu_image.c: Buffer overflow hardening — 16 fixes:
  - sha256_digest/file: added out_size param, snprintf with bounds check
  - wubu_image_export_wubu: sprintf chain → WUBU_JSON_APPEND macro with truncation guard
  - wubu_image_export_oci: config_json, manifest_json, index_json all bounds-checked
  - create_layer_tar: cmd[2048] → cmd[WUBU_MAX_PATH*2+64]
  - wubu_image_build RUN: cmd[WUBU_MAX_CMD_LEN] → cmd[WUBU_MAX_PATH+WUBU_MAX_CMD_LEN+16]
  - wubu_image_push/pull: url[512] → url[WUBU_MAX_PATH], cmd[4096] → cmd[WUBU_MAX_PATH*2+128]
  - wubu_image_import_oci: index_path[1024] → index_path[WUBU_MAX_PATH]
  - wubu_image_load_base_arch: cmd[512] → cmd[WUBU_MAX_PATH+64]
  - combined buffer: strcat loop → memcpy+len tracking with bounds check
  - wubu_image_inspect: read() return check, null-terminate at actual position
- Deep stub hunt: 234 TODO/STUB markers across 79 files audited
- 400+ real gaps identified (expanded from ~300)
- New gap categories: SteamOS/Arch/Ubuntu/TempleOS parity (40), UX Fuzzing (100)
- BATTLESHIP_GAPS.md fully rewritten with 400+ gaps
- README.md, slate.md, index.md, goal-paste.md all updated

## Remaining Work (Pick One)
A) wubu_vsl.c remaining edge cases (vsl_mmap real mmap, vsl_sched_yield real call)
B) wubu_holyd.c accept4 warning fix + epoll event loop hardening
C) Tier 4 bare metal: add more interrupt routing, AHCI command submission, FAT32 write support
D) StyxFS server integration test (styxfs_server.c → real 9P client test)
E) OCI runtime: implement real registry auth + blob push/pull
F) SteamOS parity: game mode, per-game profiles, shader pre-caching
G) Desktop UX: right-click context menu, Alt+Tab, taskbar clock
H) Container lifecycle UX: create/start/stop from desktop

## Key Rule
Stop only after a concrete build/test result; never stop at a plan.
Every edited function must do real work or be marked TODO.
Tests and build must pass after changes.

## Build Commands
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
make all
make test

## Additional Test Targets
make test_vsl      # 52/52
make test_holyd    # 31/30
make test_holyc    # 76/76
make test_oci      # 10/10
make test_bottles  # 16/16
make test_network  # 139/139
make test_snapshot # 132/132
make test_daemon_panel # 21/21
make test_metal
make test_ahci
make test_fat32
make test_styxfs

## Gap Reference
See BATTLESHIP_GAPS.md for full 400+ gap list with specific function names and line numbers.
Top 20 priority gaps:
1. wubu_oci.c — 17 FULL STUB functions
2. wubu_bottles.c — 12 FULL STUB functions
3. wubu_network.c — 18 TODO-NET markers (need netlink)
4. wubu_exec.c — 6 stubs
5. wubu_holyd.c — 4 TODOs
6. wubu_vsl.c — 1 TODO (ELF PT_LOAD)
7. wubu_snapshot.c — 5 TODOs
8. wubu_syscall.c — 4 TODOs
9. wubu_editor.c — 9 stubs
10. wubu_codec.c — 1 stub
11. wubu_canvas.c — 3 stubs
12. wubu_metal.c — 8 stubs
13. ahci.c — 4 stubs
14. fat32.c — 3 stubs
15. txfs.c — 2 stubs
16. interrupt.c — 4 stubs
17. wubu_gamelib.c — 8 stubs
18. wubu_proton2.c — 10 stubs
19. wubu_pkg.c — 6 stubs
20. bear_cudnn.c — CUDA stubs

## Direction
Execute-first. Pick the next gap, fix it, build, test, report real results.
Stopping is not allowed unless a clear blocker is hit. When blocked, try alternate paths.
"Rewriting from scratch in C" is the point — anything that falls under that is REAL_GAP.
