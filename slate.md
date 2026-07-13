# WuBuOS Slate — Active Work Surface (v26)

## Current Focus: **TRIPLE-DA AUDIT DONE — ~400 REAL_GAPs RECLASSIFIED (40 code + 370 parity)**
**Mode**: Perpetual gap-closer loop — execute until ~400 → 0.
**Constraint**: "Rewriting from scratch in C" = closing a gap. No stubs, no scaffolding, no "for later".

---

## ✅ VAULTED THIS SESSION (2026-07-08)
| Work | Δ | Files |
|------|---|-------|
| Gap-scanner `find_real_gaps.py` fixed (SyntaxError + vendor-sweep) | tool | skill script |
| Honest re-audit: baseline stub class CLOSED (0 empty / 0 const-only in `src/`) | — | BATTLESHIP v22 |
| BATTLESHIP v22: ~400 = ~40 code-level + ~370 parity marathons (ReactOS 297 + SteamOS/Ubuntu/TempleOS/ZealOS) | board | BATTLESHIP.md |
| Triple-DA plumber deep-dive: Arch daemon + TempleOS DOS daemon ↔ WuBuOS Desktop 1:1 parity | analysis | BATTLESHIP.md Part 3 |
| Accomplishments rolled to `vault/ACCOMPLISHMENTS_2026-07-08.md` | vault | vault/ |
| **CYCLE CLOSED (2026-07-08):** 10 `system()`→`wubu_run_program` fork+exec (`wubu_spawn.c`); 4 stub-phrases closed — `wubu_gamelib` start-menu registry, `vsl_gpu_vulkan` memtype scan, `wubucontainer` handler registry, `dosgui_term` container PTY render+input. All green; 3 new regression tests (`test_spawn`, `test_container_registry`, gamelib start-menu). | code | BATTLESHIP.md Part 1 A/B |
| **E3 CLOSED (2026-07-08):** `wubu_archd` (16/16) wired as Desktop service/autostart manager via `dosgui_service_mgr.c`; `dosgui_desktop_init/shutdown` call init+boot / shutdown. 19-check regression test (`make test_service_mgr`). Also fixed pre-existing broken `test_hosted` link (aligned with `hosted` binary). | code | BATTLESHIP.md Part 3.3/3.4 |
| **E4 CLOSED (2026-07-08):** HolyC terminal tab embeds `wubu_holyd --repl` as a real PTY-backed REPL. Added `--repl` TTY mode to `wubu_holyd_lifecycle.c` + `wubu_holyd_bin` target; `term_pty_spawn` gained argv support; HOLYC tab spawns REPL, key/render routed to its PTY. `test_dosgui_term` covers it (12 checks). | code | BATTLESHIP.md Part 3.4 |
| **E1 FIRST BATCH (2026-07-08):** 10 ReactOS NT syscalls transliterated to real VSL handlers (`vsl_syscall_nt.c`): NtAddAtom/NtFindAtom (atom table), NtClearEvent (eventfd reset), NtAllocateUuids (RFC4122 via getrandom), NtAllocateLocallyUniqueId, NtAlertThread (futex wake), NtCancelIoFile (shutdown), NtAssignProcessToJobObject (setpgid), NtAllocate/NtFreeUserPhysicalPages (mmap/munmap). Dispatched via `vsl_nt_syscall_dispatch` (was declared-only). 25-check `test_vsl_nt`. 277 of 297 remain after batch 2. | code | BATTLESHIP.md E1 |
| **E1 BATCH 2 (2026-07-09):** 10 MORE ReactOS NT syscalls transliterated to real VSL handlers (`vsl_syscall_nt.c` + wired into `vsl_syscall_table[]`): NtAlertResumeThread (14, futex wake + SIGCONT), NtAreMappedFilesTheSame (21, inode/dev compare), NtCreateJobObject (42) / NtOpenJobObject (125) / NtTerminateJobObject (266) / NtIsProcessInJob (99) — job objects backed by an ISOLATED sentinel-child process group so terminate does NOT kill the caller, NtDeleteAtom (63) / NtQueryInformationAtom (158), NtFlushWriteBuffer (86, real fsync), NtSetUuidSeed (256, deterministic UUID gen). Regression test extended to 49 checks (was 25). 277 remain. **HANG BUG FOUND + FIXED (2026-07-09):** `NtTerminateJobObject` used a blocking `waitpid(pgid,0)` that never returned under WSL2 (test hung every run). Replaced with a bounded `WNOHANG` poll reap; test now genuinely passes 49/0 (3× confirmed). The original "49 checks green" milestone claim was unverified — corrected. | code | BATTLESHIP.md E1 |
| **Code gaps closed (2026-07-09, session 2):** `wubu_canvas.c` PNG/GIF decode (BATTLESHIP #22/#23) — extracted all PNG/BMP/PPM/GIF save+load into new `src/apps/wubu_canvas_io.c`. Real zlib IDAT (compress2/inflate) + correct CRC32 chaining + adaptive PNG unfilter + real GIF LZW with Adam7 de-interlace. Pixel-exact round-trips, PIL-validated. `test_apps2` 19/19. Board ~22→~20 code-level gaps. | code | BATTLESHIP.md Part 1 #22/#23 |

---

## The Honest Sprint Board (Part 1 — ~25 verifiable code gaps, post-cycle)
Source of truth: `find_real_gaps.py src` (0 empty/0 const-only) + marker grep.
- **0 live `system()` calls** — CLOSED this cycle (10→0) via `src/runtime/wubu_spawn.c` `wubu_run_program()`; `grep -rEn '\bsystem\(' src` = 0.
- **19 stub-phrase funcs** (6 closed: `wubu_gamelib.c` ×2 start-menu, `vsl_gpu_vulkan.c` memtype, `wubucontainer.c` register_handler, `dosgui_term.c` container session, `wubu_canvas.c` ×2 PNG/GIF decode) — remaining: `tasking.c`(2), `wubu_anticheat.c`(2), `bear_cudnn.c`(3), `wubu_screenshot.c`, `wubu_pkgmgr.c`, `oci_http_client.c`, `holyc_ptx.c`, `wubu_compositor_standalone.c`(2), `wubu_compositor.c`, `wubu_bottles.c`.
- **6 bare-metal no-ops** — `tasking.c`(3 ctx-switch), `wubu_metal.c`(3 `#else` audio/X11 — correct).

## The Parity Marathons (Part 2 — ~350, reclassified per rule)
- **E1 ReactOS NT: 297** syscalls mapped, **20 transliterated** (batch 1 = 10, batch 2 = 10, 2026-07-09). 277 remain.
- **E1 BATCH 3+4 (2026-07-12):** 14 MORE ReactOS NT syscalls transliterated to real VSL handlers in `vsl_syscall_nt.c` (now the canonical NT dispatch table `g_nt_dispatch[]`; legacy `vsl_syscall_table.c` no longer exists). Batch 3 (10, file-I/O + events + delay spine): NtDelayExecution(62)/NtCreateEvent(38)/NtOpenEvent(121)/NtSetEvent(229)/NtResetEvent(209)/NtClose(28)/NtOpenFile(123)/NtReadFile(192)/NtWriteFile(285)/NtQueryInformationFile(159) — real nanosleep/eventfd/open/pread/pwrite/fstat work. Batch 4 (4, the 'NT=SteamOS' process launch spine): NtAllocateVirtualMemory(19)/NtFreeVirtualMemory(88)/NtCreateThread(56)/NtCreateProcess(50) — real mmap/pthread_create/fork work; handle table extended with a `data` payload slot (pid/tid/mmap-base). **ORIGINAL BATCH-3 ORDINALS WERE WRONG** (used off-by-one header macros, test passed only by internal consistency) — corrected against `reactos-study/.../sysfuncs.lst`, rebuilt + re-run: 74/0 green. Total 34 transliterated, 263 remain. `test_vsl_nt` verified by hand-built lite binary (Makefile's `-lvulkan -lcuda` link hangs WSL — known defect).
- **E2 SteamOS: ~30**, **E3 Ubuntu/Arch: ~20**, **E4 TempleOS: ~15**, **E5 ZealOS: ~8**.

## Triple-DA Plumber Verdict (Part 3)
Arch daemon (16/16) + TempleOS DOS daemon (31/31) EXIST but are NOT wired as the
Desktop's service/launcher/REPL backend. ~6 concrete integration REAL_GAPs.

---

## Next Cycle (highest priority)
1. ~~**Close the 10 `system()` calls** → fork+exec~~ **DONE** (10→0, `wubu_spawn.c` + `test_spawn`).
2. ~~**Close stub no-ops**: `wubu_gamelib_clear_start_menu`, `vsl_gpu_vulkan` memtype, `wubucontainer` register_handler, `dosgui_term` container session~~ **DONE** (4 closed, 3 regression tests).
3. ~~**Wire Arch daemon as Desktop autostart/service manager** (E3 integration)~~ **DONE** (`dosgui_service_mgr.c`; `dosgui_desktop_init` boots autostart services via `wubu_archd_svc_start`; 19-check `test_service_mgr`).
4. ~~**Embed holyd REPL into Desktop terminal** (E4)~~ **DONE** — HolyC terminal tab spawns `wubu_holyd --repl` as a real PTY-backed REPL (`term_pty_spawn` + `--repl` TTY mode in `wubu_holyd_lifecycle.c`); key/render routing wired; 12-check `test_dosgui_term` (incl. `test_holyc_embed`).
5. ~~**ReactOS NT: transliterate first 10 syscalls** (E1)~~ **DONE (batch 1 of 297)** — 10 real VSL handlers in `vsl_syscall_nt.c`; wired into `vsl_syscall_table[]` + `vsl_nt_syscall_dispatch`; regression `test_vsl_nt` (25 checks). 277 remain.
6. ~~**ReactOS NT: transliterate batch 2 (10 more)** (E1)~~ **DONE (2026-07-09)** — NtAlertResumeThread/NtAreMappedFilesTheSame/NtCreateJobObject/NtOpenJobObject/NtTerminateJobObject/NtIsProcessInJob/NtDeleteAtom/NtQueryInformationAtom/NtFlushWriteBuffer/NtSetUuidSeed. Job objects use isolated sentinel-child process group. `test_vsl_nt` now 49 checks. 277 remain.
7. ~~**Desktop Stream A (2026-07-12):** study Win98/XP shell + Wayland hosted-client display + ReactOS explorer/desktop.cpp; close live-namespace + missing ctx-menu gaps~~ **DONE** — `dosgui_wm_refresh_desktop()` enumerates folders+files+.desktop; `dosgui_wm_new_folder`/`new_text_doc` create real ~/Desktop objects; `dosgui_wm_sort_icons(Name/Size/Type/Date)` via stat(target); 5 previously-NULL ctx-menu actions wired (New Folder / New Text Doc / Sort Size/Type/Date). `test_dosgui_wm` 23/23. Vision doc: DESKTOP_VISION_PLAN.md.
 — NtAlertResumeThread/NtAreMappedFilesTheSame/NtCreateJobObject/NtOpenJobObject/NtTerminateJobObject/NtIsProcessInJob/NtDeleteAtom/NtQueryInformationAtom/NtFlushWriteBuffer/NtSetUuidSeed. Job objects use isolated sentinel-child process group. `test_vsl_nt` now 49 checks. 277 remain.

## Notes
- **268 .c / 164 .h** files, ~15K real LOC.
- **All tests green** (prior session: 64 targets / 747+ assertions). `make runtime`/`make hosted` exit 0 (this session, `c475263`).
- Slate v21/v25 "~349 sprint" was **NOT reproducible** — corrected to ~40 + ~370 this session.
- WuBuOS: ZealOS kernel + Win98 shell + Styx/9P namespace + Arch containers.
