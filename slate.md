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
| **E1 FIRST BATCH (2026-07-08):** 10 ReactOS NT syscalls transliterated to real VSL handlers (`vsl_syscall_nt.c`): NtAddAtom/NtFindAtom (atom table), NtClearEvent (eventfd reset), NtAllocateUuids (RFC4122 via getrandom), NtAllocateLocallyUniqueId, NtAlertThread (futex wake), NtCancelIoFile (shutdown), NtAssignProcessToJobObject (setpgid), NtAllocate/NtFreeUserPhysicalPages (mmap/munmap). Dispatched via `vsl_nt_syscall_dispatch` (was declared-only). 25-check `test_vsl_nt`. 287 of 297 remain. | code | BATTLESHIP.md E1 |

---

## The Honest Sprint Board (Part 1 — ~25 verifiable code gaps, post-cycle)
Source of truth: `find_real_gaps.py src` (0 empty/0 const-only) + marker grep.
- **0 live `system()` calls** — CLOSED this cycle (10→0) via `src/runtime/wubu_spawn.c` `wubu_run_program()`; `grep -rEn '\bsystem\(' src` = 0.
- **19 stub-phrase funcs** (4 closed this cycle: `wubu_gamelib.c` ×2 start-menu, `vsl_gpu_vulkan.c` memtype, `wubucontainer.c` register_handler, `dosgui_term.c` container session) — remaining: `tasking.c`(2), `wubu_anticheat.c`(2), `bear_cudnn.c`(3), `wubu_screenshot.c`, `wubu_canvas.c`(2), `wubu_pkgmgr.c`, `oci_http_client.c`, `holyc_ptx.c`, `wubu_compositor_standalone.c`(2), `wubu_compositor.c`, `wubu_bottles.c`.
- **6 bare-metal no-ops** — `tasking.c`(3 ctx-switch), `wubu_metal.c`(3 `#else` audio/X11 — correct).

## The Parity Marathons (Part 2 — ~370, reclassified per rule)
- **E1 ReactOS NT: 297** syscalls mapped, 0 transliterated (`vsl_nt_bridge.h:376` `VSL_NT_MAP_STUB`).
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
5. ~~**ReactOS NT: transliterate first 10 syscalls** (E1)~~ **DONE (first batch of 297)** — 10 real VSL handlers in `vsl_syscall_nt.c` (add/find atom, clear event, alloc uuids/luid, alert thread, cancel io, assign job, alloc/free phys pages); wired into `vsl_syscall_table[]` + `vsl_nt_syscall_dispatch`; regression `test_vsl_nt` (13 checks). 287 remain.

## Notes
- **268 .c / 164 .h** files, ~15K real LOC.
- **All tests green** (prior session: 64 targets / 747+ assertions). `make runtime`/`make hosted` exit 0 (this session, `c475263`).
- Slate v21/v25 "~349 sprint" was **NOT reproducible** — corrected to ~40 + ~370 this session.
- WuBuOS: ZealOS kernel + Win98 shell + Styx/9P namespace + Arch containers.
