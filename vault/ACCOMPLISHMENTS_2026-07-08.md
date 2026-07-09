# WuBuOS Accomplishments — Vaulted (Mind Palace) — v22 — 2026-07-08

> Roll-up of everything closed since project inception. Refresh cadence: every
> session-closure. Purpose: keep BATTLESHIP.md holding ONLY open REAL_GAPs with a
> clean scoreboard, and give the next session a verified "what is actually real"
> baseline.
>
> **Honesty note (2026-07-08 re-audit):** the prior v21 "~400 sprint gaps" number
> was **not reproducible** — the canonical scanner (`find_real_gaps.py`) was broken
> (SyntaxError + sweeping vendored `reference/` libs for 2329 false hits) and the
> new honest run finds **0 empty bodies and 0 const-only-no-syscall gaps in `src/`**.
> This roll-up therefore separates two different kinds of REAL_GAP (per the user's
> explicit reclassification rule "rewriting from scratch in C = REAL_GAP"):
>   1. **Code-level (~40 verifiable)** — live `system()` calls + stub-phrase funcs + bare-metal no-ops.
>   2. **Parity epics (~370, marathons)** — SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS NT
>      whose missing subsystems each = "rewrite that subsystem in C from scratch."

## Methodology (verified, not estimated)
- "Closing a gap" = rewriting the function in real C so it does real work, OR a
  verified defensive guard (input validation / null check on error path) that is
  NOT a gap. Empty bodies, `(void)param` on success paths, `system()`, and
  `return -1/0` without work ARE gaps.
- **Reproducible baseline scanner** (fixed 2026-07-08): `find_real_gaps.py src`
  → 0 empty `{}` bodies, 0 const-only-no-syscall gaps in `src/` (vendored
  `reference/` / `reactos-study/` / `wubucontainer/` excluded).
- **Marker grep** (`stub|not implemented|placeholder|for later` in `src/`) = the
  highest-signal real-gap finder; Scanner A (real-work-but-hardcoded-return) produces
  76 candidates but all sampled are false positives (real work + `return 0` success).
- Tests: `make runtime` exit 0, `make hosted` exit 0 (2026-07-08, `c475263`).
  Full `make test` gate = 64 targets / 747+ assertions GREEN.

## Foundation streams (all closed)
1. **Runtime Core** — OCI, network, snapshot, VSL, holyd, image, proton: real C,
   `system()`→fork+exec, 0 stubs.
2. **Kernel/Metal** — interrupt, fat32, txfs, ahci, drm_direct, vulkan: real impl.
3. **Bridge** — `wubu_syscall.c`: 97 void casts + 2 `system()` → fd/Styx/container handlers, 26 trampolines.
4. **Hosted** — `hosted.c`: 72 Wayland void casts → all callbacks wired (popup, DnD, touch, output, tablet).
5. **Bear RL** — `bear_cudnn.c`: 117 `#else` void casts → CPU cuBLAS/cuDNN via `bear_simd.h`; `bear_vulkan.c`: 4 compute pipelines; `bear_opt.c`: optimizer step/zero_grad real (Adam/SGD/Muon dispatch); `bear_nn.c`/`bear_ppo.c`: checkpoint/trainer save-load real.

## Campaign streams (all closed)
6. **VSL syscall** — 9 modular files; 173 "(void)d/e/f" reclassified as 6-register ABI (NOT gaps).
7. **Package Manager** — `wubu_pkgmgr.c`: SQLite, .wubu, repos, deps, hooks, signing; `resolve_deps`/`clean_cache`/`autoremove`/`verify_installed` real; 11/11.
8. **Audio Engine** — `wubu_audio.c`: 30+ chips, Furnace, SF2, DAW, AI plugins; 14/14.
9. **Arch Daemon** — `wubu_archd.c`: pacman, AUR, signing, hooks, ABS; 16/16.
10. **TempleOS Daemon** — `wubu_holyd.c`: REPL, compiler state, symbols, macros; 31/31.
11. **Proton/Wine** — `wubu_proton.c` + `wubu_proton2.c`: PE loader, Win32→VSL, Wine, DXVK, GameScope; 46/46.
12. **HolyC Compiler** — `holyc_codegen*.c`: JIT backpatch, regalloc, 84/84; `++/--`/`&x` globals fixed; lexer↔parser token-name mismatch fixed (was an infinite-loop hang).
13. **StyxFS** — `styxfs.c`: 14 void casts → real; dir-enum (opendir/closedir/readdir/readdir_r) real via mount→host; 11/11.
14. **Control Panel** — `apps/control`: 9 tabs (was stub).
15. **App registry** — `dosgui_apps.c`: 16 void casts eliminated.
16. **Terminal** — `dosgui_term.c` / `terminal.c`: PTY, ANSI, scrollback, tabs; 17/17.
17. **Explorer** — `dosgui_explorer.c`: 9P/Styx, real ZIP mount (libzip + raw parser); 74/74.
18. **Start Menu** — `dosgui_startmenu.c`: .desktop parse, categories; 4/4.
19. **WM** — `dosgui_wm.c`: resize snap, virtual desktop migrate, focus stack, ctxmenu, invalidate tracking; 16/16.
20. **Clipboard** — `wubu_clipboard.c`: multi-MIME + DnD; 17/17.
21. **Desktop wallpaper** — `wubu_wallpaper.{h,c}` (2026-07-07): real 24/32-bit BMP decode → XRGB8888 + 5 ReactOS PLACEMENT modes; 18/18.
22. **JIT encoder** — `wubu_x86.c`: 33 return-0 stubs → real byte counts; jit_test 82/82.
23. **Snapshot/FS** — `wubu_snapshot_fs.c`: LVM `lvm_snapshot_create` real lvcreate (was -ENOSYS); btrfs/zfs real ioctls.
24. **freestanding libc** — `vsprintf` real formatted output.
25. **Anticheat** — `wubu_anticheat_proton_config` mutable config table.
26. **Orphaned-header purge** — `wm.h`→compat shim; deleted `theme.h`/`startmenu.h`; 6 apps fb-signature fix; weak-symbol defaults.

## Monolith splits (all committed)
- `dosgui_explorer.c` render → `dosgui_explorer_render.c`
- `dosgui_wm.c` → `dosgui_wm_holyc_term.c` / `dosgui_wm_systray.c` / `dosgui_wm_ctxmenu.c`
- `wubu_vsl.c` → `vsl/vsl_syscall_*.c` (9 modules)
- `wubu_pkgmgr` → `wubu_pkgmgr_{pkg,install,txn}.c`
- `wubu_audio`, `wubu_holyd`, `wubu_network`, `wubu_snapshot`, `wubu_container` submodules

## Audit-tooling fixes (2026-07-08, part of this session)
- `find_real_gaps.py`: fixed `SyntaxError` (`"EPERM" not r` → `"EPERM" not in r`) + added
  `VENDOR_DIRS` exclusion so it no longer sweeps vendored `reference/`/`reactos-study/`
  (was 2329 false hits → now 0 in `src/`). Reproducible honesty restored.

## What is explicitly NOT claimed (honesty)
- The 5 parity epics (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS NT) are
  TARGETS, not done. Per the user's rule they are reclassified as ~370 REAL_GAP
  marathons in BATTLESHIP.md v22 — each missing subsystem = "rewrite in C from scratch."
- SteamOS: CEF UI, Steam Input, Steam Networking, Proton integration, gamescope,
  Pressure Vessel — NOT implemented.
- Ubuntu/Arch: systemd, apt/pacman full parity, NetworkManager, Polkit, D-Bus,
  PipeWire, CUPS, AppArmor — NOT implemented.
- TempleOS: HolyC JIT AOT+JIT, Doc/DolDoc, Compiler-as-lib, RedSea FS, Ring-0 — partial.
- ZealOS: identity-mapped memory, VGA/VESA direct, PC speaker — partial.
- ReactOS NT: 297 syscalls mapped → 0 transliterated to VSL/Styx9/ZealOS/TempleOS.
- Architectural daemons: Arch daemon + TempleOS DOS daemon exist (16/16 + 31/31 tests)
  but are NOT yet wired as the Desktop's boot/launcher backend in a 1:1 SteamOS/Ubuntu
  parity sense — see BATTLESHIP.md v22 plumber deep-dive.
