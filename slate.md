# WuBuOS Slate — Active Work Surface (v21)

## Current Focus: **SPRINT BOARD ~400 (form≠function) + 5 PARITY EPICS + MONOLITH SPLITS**
**Sprint board**: ~400 REAL_GAPs (Triple DA, form≠function filtered) — the actionable file-by-file board.
**Parity epics**: 5 (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS) — marathons, NOT in the 400.
**Mode**: Perpetual gap-closer loop — execute until 400 → 0 (sprint) + epics progress.
**Constraint**: "Rewriting from scratch in C" — no stubs, no scaffolding, no "for later".
**Devil's Advocate**: Every parity gap with SteamOS/Ubuntu/Arch/TempleOS/ZealOS/ReactOS = REAL_GAP (epic).
**Execution**: PARALLEL — sprint board + desktop fixup + monolith splits simultaneously.

---

## ✅ COMPLETED WORKSTREAMS (vaulted → vault/ACCOMPLISHMENTS_2026-07-07.md)
Foundation 1-5 (~650 gaps) + Campaign 1-25 (~650 gaps) — all closed, tests green.
Desktop wallpaper (2026-07-07): `wubu_wallpaper.{h,c}` real BMP decode + 5 ReactOS placement modes; 18/18 test.
Pre-existing test failures CLOSED (2026-07-07 session 2): `test_holyc` 84/84 (`jit_lock_exec` RX-data SIGSEGV + global Inc/Dec/`&x` RIP-relative), `test_styxfs` 11/11 (missing compiler backend in link line), `test_syscall` 5/5 (`-D_GNU_SOURCE` for `struct sigaction`). Full `make test` gate exits 0 ("All tests passed!"). Commit `6d9824d`. See skill `wubuos-holyc-compiler`.

---

## Active Work Items — THIS CYCLE (user focus)

**A. Desktop fixup (Streams 2-4)** — plan: `DESKTOP_FIXUP_PLAN.md`
- Stream 2: persistent icon layout (save/restore via wubu_settings)
- Stream 3: working context menu (sort-by-name real, create-shortcut writes .desktop, view toggles auto-arrange)
- Stream 4: Control Panel Desktop tab goes LIVE (was verified stub)

**B. Monolith splits** (opaque structs + C11 + no god headers + self-contained) — table in BATTLESHIP.md
- 25 files ≥800 lines. Rule: opaque struct in `foo_internal.h`, static-inline helpers, Makefile 4 edits
  (OBJS + host link + test direct + runtime). Verify `make clean && make runtime && make hosted`.

**C. Sprint board ~400** — top files: wubu_metal(70), vsl_syscall_net(67), styxfs(65), interrupt(54),
vsl_syscall_fileio(53), vsl_syscall_proc(47), wubu_network(46), styx(45), wubu_syscall(36), wubu_snapshot(34).

---

## 5 Parity Epics (marathons — each = "rewrite the subsystem in C")
1. **SteamOS** 0% — CEF UI, Steam Input, Networking, Proton DXVK/VKD3D, gamescope, Pressure Vessel, Shader cache, ProtonDB, Cloud
2. **Ubuntu/Arch** ~5% (archd/pkgmgr exist) — systemd, NetworkManager, Polkit, D-Bus, PipeWire, CUPS, AppArmor, GRUB
3. **TempleOS** ~30% — HolyC JIT AOT+JIT, Doc/DolDoc, Compiler-as-lib, RedSea FS, Ring-0
4. **ZealOS** ~67% (name parity 96/96) — identity-mapped mem, VGA/VESA direct, PC speaker, God word
5. **ReactOS NT** 0% impl (297 mapped) — threads, VAD, objects, I/O/IRP, Win32k, registry → VSL/Styx9

### Devil's-Advocate parity deep-dive (what "working" requires 1:1)
- **SteamOS working** = click game in store UI → runs under Proton w/ controller+overlay. Need `wubu_steamclient.c` + `wubu_gamescope.c`.
- **Ubuntu/Arch working** = `archd` (done) + real `init`/service-manager + D-Bus. Extend to `wubu_init.c` + `wubu_dbus.c`.
- **TempleOS DOS daemon working** = `wubu_holyd` REPL (done, 33/33) + DolDoc + RedSea. Add `wubu_doldoc.c` + `styxfs_redsea.c`.

---

## ReactOS Mission: NT → VSL/Styx9 (epic #5)
`reactos-study/reactos/` is the source. Pipeline: NT syscall → vsl_syscall.c → styxfs.c → ZealOS → TempleOS.
Key dirs: ntoskrnl/{ke,mm,io,ob,ps,rtl,config}, win32ss/{user,gdi}, dll/ntdll. 297 syscalls mapped (vsl_syscall_list.h); 0 transliterated.

---

## Blockers
- None — every gap is "rewrite in C"; no external deps blocking.
- All test infrastructure working (64 targets, 747+ assertions). `make test_high_gui` ✅ (incl. wallpaper).

---

## Notes
- 73 .c files, ~15K real LOC. Real work LOC ≈ 15K (was ~123K inflated).
- **~400 sprint REAL_GAPs** (honest, form≠function). Previous "~3000" (2026-07-05) double-counted parity-% + defensive guards → reclassed.
- 64 test targets, 747+ assertions green.

## Next Direction — PARALLEL
1. Desktop Streams 2→3→4 (user priority)
2. Monolith splits per BATTLESHIP table
3. Sprint board ~400 file-by-file
4. Parity epics as they unblock
5. **Documentation overhaul** 🆕 — screenshots/ directory (w/ README), triple DA phase table in STATE.md, screenshot link in README+index. See `.hermes/plans/2026-07-07_doc-media-roadmap-overhaul.md`.
Each gap = "rewriting from scratch in C". **NO PICK ONE — PARALLEL.**
