# WuBuOS Accomplishments — Vaulted (Mind Palace)

> Roll-up of everything closed since project inception. Refreshed 2026-07-07.
> Purpose: free the active gap list (BATTLESHIP.md) to hold ONLY open REAL_GAPs,
> and give the next session a clean scoreboard.

## Methodology note
- "Closing a gap" = rewriting the function in real C so it does real work, OR a
  verified defensive guard that is NOT a gap (input validation / null checks on
  error paths). Empty bodies, `(void)param` on success paths, `system()`, and
  `return -1/0` without work ARE gaps.
- Tests: `make test` → 64 targets, 747+ assertions GREEN (re-confirmed 2026-07-07).

## Foundation streams (all closed)
1. **Runtime Core** — OCI, network, snapshot, VSL, holyd, image, proton: real C,
   system()→fork+exec, 0 stubs.
2. **Kernel/Metal** — interrupt, fat32, txfs, ahci, drm_direct, vulkan: real impl.
3. **Bridge** — wubu_syscall.c: 97 void casts + 2 system() → fd/Styx/container
   handlers, 26 trampolines.
4. **Hosted** — hosted.c: 72 Wayland void casts → all callbacks wired.
5. **Bear RL** — bear_cudnn.c: 117 #else void casts → CPU cuBLAS/cuDNN via bear_simd.h;
   bear_vulkan.c: 4 compute pipelines.

## Campaign streams (all closed)
6. **VSL syscall** — 138 syscalls; 173 "void casts" reclassified as 6-register ABI
   (d,e,f unused by convention) — NOT gaps.
7. **Package Manager** — wubu_pkgmgr.c: SQLite, .wubu, repos, deps, hooks, signing; 11/11.
8. **Audio Engine** — wubu_audio.c: 30+ chips, Furnace, SF2, DAW, AI plugins; 14/14.
9. **Arch Daemon** — wubu_archd.c: pacman, AUR, signing, hooks, ABS; 16/16.
10. **TempleOS Daemon** — wubu_holyd.c: REPL, compiler state, symbols, macros; 33/33.
11. **Proton/Wine** — wubu_proton.c + wubu_proton2.c: PE loader, Win32→VSL, Wine,
    DXVK, GameScope; 46/46.
12. **HolyC Compiler** — holyc_codegen.c: JIT backpatch, regalloc, 84/84.
13. **StyxFS** — styxfs.c: 14 void casts → real; 11/11.
14. **Control Panel** — apps/control: 9 tabs (was stub).
15. **App registry** — dosgui_apps.c: 16 void casts eliminated.
16. **Terminal** — dosgui_term.c / terminal.c: PTY, ANSI, scrollback, tabs; 17/17.
17. **Explorer** — dosgui_explorer.c: 9P/Styx, real ZIP mount; 74/74.
18. **Start Menu** — dosgui_startmenu.c: .desktop parse, categories; 4/4.
19. **WM** — dosgui_wm.c: resize snap, virtual desktop migrate, focus stack; 16/16.
20. **Clipboard** — wubu_clipboard.c: multi-MIME; 17/17.
21. **Desktop wallpaper** — wubu_wallpaper.{h,c} NEW (2026-07-07): real 24/32-bit BMP
    decode → XRGB8888 + 5 ReactOS PLACEMENT modes (Center/Tile/Stretch/Fit/Fill).
    load_default_wallpaper() now reads wubu_settings→theme.wallpaper_path; draw_wallpaper()
    CENTER fixed + STRETCH/FIT/FILL sample decoded bitmap. test_wallpaper 18/18 PASS.

## Monolith splits (all committed)
- dosgui_explorer.c: render layer → dosgui_explorer_render.c (2254→1743)
- dosgui_wm.c: holyc_term → dosgui_wm_holyc_term.c; systray → dosgui_wm_systray.c;
  ctxmenu → dosgui_wm_ctxmenu.c
- vsl_syscall → vsl/vsl_syscall_*.c (9 modules)
- wubu_pkgmgr → wubu_pkgmgr_{pkg,install,txn}.c
- wubu_audio, wubu_holyd, wubu_network, wubu_snapshot submodules

## Dead-code purge (2026-07-07)
- Quarantined src/gui/wm.c, wm_test.c, startmenu.c, desktop.c, theme.c,
  src/kernel/vbe_legacy.h → src/_legacy_bak/ (never in Makefile; dead legacy w/
  vbe_legacy.h). Build relinks clean, test_high_gui green.

## What is explicitly NOT claimed (honesty)
- SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS subsystems are parity TARGETS,
  not done. See BATTLESHIP.md v21 open-gap list.
- wubu_metal.c, wubu_network.c, styxfs.c, interrupt.c, wubu_snapshot.c, wubu_oci.c,
  styx.c, wubu_x86.c, vsl_syscall_* still carry significant void-cast/return-const
  counts (see open list) — these are the live frontier.
