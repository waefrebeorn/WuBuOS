# WuBuOS Desktop Vision — Study + Execution Plan

**Date:** 2026-07-12 (session 2 of monolith-split → desktop-vision pivot)
**Author:** gap-closer (Hermes)
**Context:** 7 monolith splits committed this session (bear_ppo, wubu_vulkan, wubu_drm_direct dedup, styxfs, bear_nn, wubu_archd, wubu_metal backends). Full gate green. User directed: *"revisit the desktop and its issues completely; study the wayland and steamos desktop and reactos desktop; fully realize our project vision of templeOS dos kernel and then a NT kernel that is steamOS essentially on ArchOS and our special styx9."*

---

## THE VISION (what "fully realize" means — 6 layers)

```
TempleOS HolyC DOS soul   wubu_holyd REPL/JIT/compiler-as-library   ✅ DONE (E4)
        │
ZealOS microkernel        ring-0, single-address-space, VBE         ✅ DONE
        │
Styx9 (our special 9P)    file-backed IPC namespace (the "soul")    🟠 PARTIAL: styxfs_server 11/11, NT bridge 20/297
        │
NT personality (SteamOS)  ReactOS-transliterated syscall surface    ✅ spine exists; E1 20/297
        │
Win98/XP shell = Desktop  dosgui_wm/desktop/startmenu               🟠 MOSTLY DONE — gaps below
        │
SteamOS-on-Arch           Arch container + Proton + gamescope surf. 🔴 E2 0%: compositor/input/cloud
```

**The user's three named "desktops" map to three layers:**
- **TempleOS DOS kernel** → `wubu_holyd` HolyC runtime (done) + a DOS `.exe` loader personality (E1 NT surface boots PE/EXE via ReactOS-transliterated syscalls).
- **NT kernel that is SteamOS on ArchOS** → ReactOS NT syscall transliteration (E1) driving the Win32k/Win98 shell, with real apps launched as Arch containers running Proton (the `wubu_session_launch_game` → `wubu_launch_windows` path, already real).
- **SteamOS desktop** → the hosted binary renders the Win98/XP shell into a Wayland surface that a host gamescope/compositor presents; the SteamOS *experience* (game mode, controller, overlay, cloud) is EPIC E2.

---

## STUDY FINDINGS (what's actually there vs. the vision)

### A. Win98/XP Desktop — the layer the user *sees* (Layer 5)
Studied `dosgui_wm.c`, `dosgui_desktop.c`, `dosgui_wm_desktop.c`, `dosgui_wm_ctxmenu.c`, `dosgui_explorer_ops.c`, ReactOS `explorer/desktop.cpp`, `dll/cpl/desk/desktop.c`, `win32ss/user/ntuser/desktop.c`.

**Already correct (do not touch):**
- Real BMP wallpaper decode + 5 ReactOS PLACEMENT modes (Center/Tile/Stretch/Fit/Fill).
- Persisted icon layout (`IconLayout[]` in `wubu_settings`, round-trips to Styx).
- Live Control Panel Desktop tab (`control_desktop_apply` → `dosgui_wm_reload_wallpaper`).
- `ctx_action_sort_by_name`, `ctx_action_view_desktop` (auto-arrange), `ctx_action_refresh`, `ctx_action_create_shortcut` — all REAL.
- `dosgui_wm_refresh_desktop()` already scans `~/Desktop` for `.desktop` files (ReactOS `explorer/desktop.cpp` lesson).
- `dosgui_explorer_new_folder()` / `dosgui_explorer_new_file()` already create real fs objects (in explorer's `current_path`).

**Real gaps (verified, ReactOS analogs):**
1. 🔴 Desktop "New → Folder" and "New → Text Document" are `NULL` menu actions (dead). ReactOS `shell32/CDesktopFolder` creates real fs objects on the namespace. The desktop just never calls the existing explorer create functions against `~/Desktop`.
2. 🔴 "Sort By → Size / Type / Date Modified" are `NULL` (only Name is real). ReactOS `desktop.cpp` "Arrange Icons By" supports all four.
3. 🟠 **Static namespace on boot.** `dosgui_desktop_init()` registers a fixed app-array; `~/Desktop/*.desktop` files only appear after a manual Refresh. ReactOS enumerates the live namespace at init.

### B. Wayland + SteamOS display (Layer 2 / 6)
Studied `hosted.c` (Wayland client: `wl_compositor`/`xdg_wm_base`/`wl_shm`, SHM double-buffer), `wubu_metal_vulkan.c` (Vk surface), ReactOS `ntuser/desktop.c` (kernel desktop object).
- `hosted.c` is a **correct Wayland client** — it renders the desktop frame and presents it to whatever host compositor exists (weston/mutter/gamescope). This is the Inferno `emu` pattern; WuBuOS should NOT re-implement the compositor. The nuked `wubu_compositor.c` (wlroots) was redundant — keep in legacy_bak.
- 🔴 **gamescope/SteamOS features (E2) = 0%**: no VRR/HDR/FSR, no Steam Input (uinput/gyro), no nested/embed mode, no Steam Cloud/shader-precache. The *display path* works; the *SteamOS experience* is not built.

### C. NT kernel personality (Layer 4) — the "SteamOS kernel"
Studied `vsl_nt_bridge.h`, `vsl_syscall_nt.c`, `vsl_syscall_table.c`, `reactos/ntoskrnl/sysfuncs.lst`.
- Full 297-syscall NT→VSL→Styx9 pipeline *designed* (bridge header complete: status codes, object types, handle table NT-handle→VSL-fd→Styx-fid).
- **Only 20/297 transliterated** (E1 batch 1+2). 277 remain. Each = a "rewrite from scratch in C" marathon per the standing rule.

---

## EXECUTION PLAN (this session: Layer 1 desktop gaps; roadmap for Layers 2-6)

### STREAM A — Desktop live namespace + missing actions  [EXECUTE NOW]
Closes ReactOS gaps #1, #2, #3 above. All testable, no stubs.

- **A1** `dosgui_wm_desktop.c`: add `dosgui_wm_new_folder()` (mkdir `~/Desktop/New Folder` with EEXIST counter) + `dosgui_wm_new_text_doc()` (open `~/Desktop/New Text Document.txt` O_EXCL). Both then `dosgui_wm_refresh_desktop()` + `reflow_all_icons_column()`. Reuses the existing explorer create logic but targets `~/Desktop`.
- **A2** `dosgui_wm_desktop.c`: add `dosgui_wm_sort_icons(DosGuiSortMode)` with Name/Size/Type/Date. Size/Type/Date resolve via `stat(icon->target)` (folder→directory type). Mirrors ReactOS "Arrange Icons By".
- **A3** `dosgui_wm_ctxmenu.c`: wire `ctx_action_new_folder`→`dosgui_wm_new_folder`, `ctx_action_new_text_doc`→`dosgui_wm_new_text_doc`, and `ctx_action_sort_by_size/type/date`→`dosgui_wm_sort_icons(...)`. Replace the 3 `NULL` Sort-By entries and 2 `NULL` New entries.
- **A4** `dosgui_desktop.c`: call `dosgui_wm_refresh_desktop()` inside `dosgui_desktop_init()` so `~/Desktop` is a live namespace on boot (ReactOS init behavior).
- **A5** Regression test: extend `dosgui_wm_test.c` — create a temp `XDG_DESKTOP_DIR`, init desktop, assert `~/.Desktop/*.desktop` surfaces as an icon; assert New Folder creates a dir + new icon; assert sort-by-size reorders by stat size. Green in `test_high_gui`.

### STREAM B — NT kernel spine (E1, Layer 4)  [NEXT SESSION]
Continue ReactOS transliteration: batch 3 (10 more syscalls: NtCreateProcess/NtCreateThread/NtOpenFile/NtReadFile/NtWriteFile/NtQueryInformationFile/NtSetInformationFile/NtCreateKey/NtOpenKey/NtQueryValueKey). Real VSL handlers + wire `vsl_syscall_table[]` + extend `test_vsl_nt` (must run standalone, not in gate). Drives the "NT kernel = SteamOS" spine.

### STREAM C — SteamOS display experience (E2, Layer 2/6)  [ROADMAP]
- gamescope-style nested Wayland surface + VRR/HDR/FSR hooks in `hosted.c` render path.
- Steam Input: uinput gyro/rumble mapping in `wubu_metal_evdev.c`.
- Steam Cloud / shader-precache via `wubu_snapshot.c` + `wubu_compat_db.c`.
- Each = EPIC E2 marathon (not micro-counted).

### STREAM D — Styx9 soul (Layer 3)  [ROADMAP]
Extend `styxfs_server.c` (11/11) with the NT registry-as-namespace (`vsl_nt_bridge_ctx_t.registry` → Styx9), completing the "our special Styx9" glue between ZealOS and the NT personality.

---

## VERIFICATION GATE (after each stream)
```
make runtime && make hosted && make test          # must be green
make test_vsl_nt                                   # E1 only: exit 0, no hang (49→59 checks)
find_real_gaps.py src                              # EMPTY=0 / CONST-ONLY=0
```
Update `BATTLESHIP.md` (Part 1 + E1) + `slate.md` + `STATE.md` after Stream A.

## COMMIT STRATEGY
- Stream A: 1 commit (`desktop: live ~/Desktop namespace + New Folder/Text Doc + Size/Type/Date sort (ReactOS explorer/desktop.cpp lesson)`).
- Streams B/C/D: separate sessions, each own commit.
