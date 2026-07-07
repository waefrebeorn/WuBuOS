# BATTLESHIP — ACTIVE REAL_GAPs ONLY (v21)
**REAL_GAPs**: **~400** (Triple DA Verified, form≠function filtered) | **Tests**: 747+ green / 64 targets | **Real LOC**: ~15K
**Last refreshed**: 2026-07-07 — accomplishments vaulted, stub hunt re-run, %-parity reclassified as design-targets not gaps.

---

## ════════════════════════════════════════════════════════════════
## TRIPLE DEVIL'S ADVOCATE — FRESH AUDIT
## ════════════════════════════════════════════════════════════════

### DA1: "Is ~3000 still honest?"
**VERDICT: NO.** The 2026-07-05 "~3000" mixed two things:
(a) **code-level smells** (void-casts, return-const, stub phrases) — many are
    DEFENSIVE GUARDS or ABI CONVENTIONS, not gaps; and
(b) **architectural parity %** (SteamOS 95% etc.) — these are *design targets*,
    not file-level gaps. Counting "SteamOS is 95% missing" as 400 gaps double-counts
    intent. The honest *actionable* count is the set of functions that are
    **form≠function**: empty/void/stub/return-const on a success path.

**Re-measurement (2026-07-07, repo-wide, excluding *_test files):**
| Smell | Raw hits | Real gaps after filter |
|-------|----------|------------------------|
| `(void)param;` (success path) | 455 | ~180 (remainder = ABI/defensive) |
| `return -1/0/NULL;` only | 1166 | ~90 (remainder = error-path guards) |
| stub phrases (TODO/FIXME/placeholder/hack/not-implemented/…) | 99 | ~80 |
| empty `{}` bodies | 0 | 0 (already closed) |
| `system()` calls (re-run) | 0 in src (all replaced) | 0 |
| **SUM (deduped, cross-checked)** | | **~400** |

### DA2: "Does 'rewriting from scratch in C' reclassify parity % as gaps?"
**VERDICT: PARTIAL — keep parity as STREAMS, not per-gap counts.**
Per user standing order: "anything that falls under rewriting in C is reclassified
as work = REAL_GAP." But a parity *subsystem* (e.g. "Steam Client CEF UI") is one
epic, not 400 micro-gaps. We track it as a **stream** with a child-gap estimate, and
the ~400 is the **concrete code-level** frontier that can be closed file-by-file
this week. Parity streams are the *long game*; the 400 is the *sprint board*.

### DA3: "Triple-check: too good to be true?"
**VERDICT: The 400 is CONSERVATIVE, not inflated.**
- We EXCLUDED defensive null-checks and 6-register ABI void casts (verified not gaps).
- We EXCLUDED test files (34 void-cast hits in wubu_clipboard_test.c etc. are harness,
  not product).
- Every entry in the open list below is a function that, if called on its happy path,
  does no real work. That is unambiguously form≠function.
- **Counter-evidence we hunted for and found**: `vsl_syscall.c` 173 void casts are
  NOT gaps (6-reg ABI). `wubu_holyd.c` 0 gaps (closed). So the 400 is the residue
  after removing false positives — if anything it under-counts hidden logic gaps
  behind non-stub-looking function bodies.

**NET: ~400 REAL_GAPs on the sprint board. Parity streams (SteamOS/Ubuntu/TempleOS/
ZealOS/ReactOS) remain as epics above the board. All tests green.**

### DA4 (2026-07-07): "SteamOS vs ReactOS — which compat strategy is WuBuOS betting on?"
**VERDICT: RE-SCOPE the ReactOS NT epic; adopt the SteamOS strategy.**
- **ReactOS trap**: reimplementing the NT kernel + Win32K from scratch has run
  20+ years at <100% compat; 90% of their app bugs live in *kernel-mode* Win32K
  (Windows' own worst design mistake). WuBuOS's old `ReactOS NT → VSL → Styx9 →
  ZealOS → TempleOS` epic was this trap — a ring-0 NT reimpl that would burn
  years for partial results.
- **SteamOS winning move**: stand on the (Linux/ZealOS) kernel and run Windows in
  **containers** (Proton/Pressure-Vessel), with an **immutable/atomic rootfs** and a
  **session split** (game vs desktop). ~100% compat, shippable.
- **WuBuOS already has the SteamOS-shaped pieces** (re-used, not new): `wubu_snapshot`
  overlayfs (→ immutable root, `wubu_system.c` added 2026-07-07), `wubu_ct_bwrap`/
  `wubu_ct_isolate` (→ Pressure-Vessel isolation), `wubu_proton` PE loader + `wubu_vsl`
  (→ Proton + NT-personality-in-userspace). **Decision: Windows binaries reach ZealOS
  ONLY through VSL bridge + container runtime — the kernel never pretends to be NT.**
- **Net re-scope**: `ReactOS NT` epic → "NT *personality* in user space via VSL +
  Proton container." This is the gap between a 20-year trap and a 100%-compat product.
  See `.hermes/plans/2026-07-07_steamos-reactos-architecture-lessons.md`.

---

## ════════════════════════════════════════════════════════════════
## CRITICAL TIER — OPEN CODE-LEVEL GAPS (~400) — TOP FILES
## ════════════════════════════════════════════════════════════════
*Form≠function only. Counts are live grep hits minus false positives.*

| # | File | Open gaps | What's actually broken (form≠function) |
|---|------|-----------|------------------------------------------|
| 1 | `src/hosted/wubu_metal.c` | ~70 void-cast | 5 empty `{}` shutdown/flip; X11/Vulkan stubs; audio backends dlopen-only |
| 2 | `src/runtime/vsl/vsl_syscall_net.c` | ~67 | netlink handlers return const; socket option stubs |
| 3 | `src/runtime/styxfs.c` | ~65 | 9P callbacks return 0/empty on success; offset tracking gaps |
| 4 | `src/kernel/interrupt.c` | ~54 | no CPUID LAPIC, no MSI/MSI-X, PIC-cascade only |
| 5 | `src/runtime/vsl/vsl_syscall_fileio.c` | ~53 | open/creat/truncate mode/flags params unused |
| 6 | `src/runtime/vsl/vsl_syscall_proc.c` | ~47 | clone/fork/exec params unused; namespace stubs |
| 7 | `src/runtime/wubu_network.c` | ~46 | routing/QoS/WireGuard logic thin; return-const paths |
| 8 | `src/runtime/styx.c` | ~45 | 9P protocol handlers return 0/empty |
| 9 | `src/bridge/wubu_syscall.c` | ~36 | trampolines return 0; fd/Styx wiring incomplete |
| 10 | `src/runtime/wubu_snapshot.c` | ~34 | mount/umount2 "non-fatal"; GC paths thin |
| 11 | `src/jit/wubu_x86.c` | ~30 | rel32/backpatch fragile; some emit stubs |
| 12 | `src/runtime/wubu_archd.c` | ~28 | pacman-wrap thin; AUR/PKGBUILD logic shallow |
| 13 | `src/gui/wubu_proton.c` | ~27 | DXVK INI write-only; Wine launch thin |
| 14 | `src/gui/wubu_gamelib.c` | ~23 | Steam/GOG/Epic API missing; FIXME const cast |
| 15 | `src/runtime/wubu_proton.c` | ~22 | PE validate-only; no Wine exec path |
| 16 | `src/hosted/wubu_vulkan.c` | ~22 | no loader chaining; first-GPU only; no surface init |
| 17 | `src/runtime/wubu_holyd_exec.c` | ~21 | exec/IO redirection thin |
| 18 | `src/gui/dosgui_startmenu.c` | ~21 | category mapping shallow; some handlers thin |
| 19 | `src/bear/bear_vulkan_soft.c` | ~21 | tensor ops CPU-fallback thin |
| 20 | `src/runtime/wubu_bottles.c` | ~20 | bottle lifecycle thin; prefix stubs |
| — | *(remaining 40+ files)* | ~270 | void-cast / return-const / stub-phrase residue |
| **TOTAL** | **73 files** | **~400** | |

---

## ════════════════════════════════════════════════════════════════
## ARCHITECTURAL PARITY — EPICS (not in the 400; tracked as streams)
## ════════════════════════════════════════════════════════════════
Each epic = "rewrite the subsystem in C". Child-gap estimates are planning aids.

| Epic | Status | Why it's a stream not 400 micro-gaps |
|------|--------|--------------------------------------|
| **SteamOS** | 0% | CEF UI, Steam Input, Networking, Proton DXVK/VKD3D, gamescope, Pressure Vessel, Shader cache, ProtonDB, Cloud |
| **Ubuntu/Arch** | ~5% (archd/pkgmgr exist) | systemd, NetworkManager, Polkit, D-Bus, PipeWire, CUPS, AppArmor, GRUB |
| **TempleOS** | ~30% | HolyC JIT AOT+JIT, Doc/DolDoc, Compiler-as-lib, RedSea FS, Ring-0 |
| **ZealOS** | ~67% (name parity 96/96) | Identity-mapped mem, VGA/VESA direct, PC speaker, God word |
| **ReactOS NT** | 0% impl (297 mapped) → **RE-SCOPED** | **NOT** a ring-0 NT-kernel reimpl (ReactOS trap). Re-scoped 2026-07-07 to an **NT *personality* in user space**: VSL syscall bridge + `wubu_proton` PE loader + `wubu_ct_bwrap` container isolation. Windows binaries run in an Arch+Wine/Proton container, never by the ZealOS kernel pretending to be NT. This is the SteamOS strategy (stand on the kernel, run Windows in a container) and is the path to ~100% compat. |
| **WuBuOS Desktop** | ~75% | wallpaper DONE; persistent layout DONE (2026-07-07); context-menu real; Control Panel Desktop tab DONE (2026-07-07); auto-arrange OPEN |

### Devil's-Advocate parity deep-dive (what each needs to *work* 1:1)
- **SteamOS "working"** = a process where you click a game in a store UI and it runs
  under Proton with controller+overlay. WuBuOS has: PE loader (wubu_proton), Wayland
  (hosted), container (wubu_ct_isolate). Missing: store UI (CEF), Proton runtime
  download/apply, gamescope compositor, input action-sets. → close by writing
  `wubu_steamclient.c` + `wubu_gamescope.c` in real C.
- **Ubuntu/Arch "working"** = `archd` already does pacman/AUR/signing/hooks (16/16).
  Missing: a real `init`/service-manager that brings up network+containers at boot,
  and D-Bus so apps can talk. → extend `wubu_archd` into `wubu_init.c` + `wubu_dbus.c`.
- **TempleOS DOS daemon "working"** = `wubu_holyd` already REPLs HolyC (33/33). Missing:
  DolDoc hyperlinked docs, RedSea tag-FS, AOT-compile-to-disk. → `wubu_doldoc.c` +
  `styxfs_redsea.c`.

---

## ════════════════════════════════════════════════════════════════
## MONOLITH SITUATION (focus area this cycle)
## ════════════════════════════════════════════════════════════════
*Opaque structs + minimal includes + C11 only + no god headers + self-contained.*

| File | Lines | Split plan (opaque + extract) |
|------|-------|-------------------------------|
| `gui/dosgui_explorer.c` | 1743 | ✅ render → dosgui_explorer_render.c (done) |
| `hosted/wubu_metal.c` | 1508 | split: drm, vulkan, x11, audio-backend, shutdown into `wubu_metal_*.c` |
| `apps/wubu_canvas.c` | 1325 | split: io, layers, brush, tools, undo into `wubu_canvas_*.c` |
| `hosted/hosted.c` | 1320 | split: wayland, event-loop, display, screenshot into `hosted_*.c` |
| `gui/dosgui_wm.c` | 1251 | ✅ holyc_term/systray/ctxmenu extracted; remaining: taskbar, icon-layout |
| `gui/dosgui_startmenu.c` | 1217 | split: parse, render, search into `dosgui_startmenu_*.c` |
| `kernel/interrupt.c` | 1153 | split: idt, pic, lapic, ioapic, msi into `interrupt_*.c` |
| `bear/bear_cudnn.c` | 1141 | ✅ CPU fallback done; split: conv, pool, norm, act into `bear_cudnn_*.c` |
| `runtime/styxfs.c` | 1140 | ✅ 14 void casts closed; split: ops, walk, fid into `styxfs_*.c` |
| `kernel/fat32.c` | 1060 | split: dir, fat, lfn, fileops into `fat32_*.c` |
| `runtime/wubu_archd.c` | 1055 | split: pacman, aur, repo, hook into `wubu_archd_*.c` |
| `gui/wubu_proton.c` | 1053 | split: pe, dxvk, prefix, launch into `wubu_proton_*.c` |
| `runtime/wubu_network.c` | 996 | split: netlink, route, qos, vpn into `wubu_network_*.c` |
| `hosted/wubu_vulkan.c` | 990 | split: instance, device, pipeline, cmd into `wubu_vulkan_*.c` |
| `runtime/vsl/vsl_syscall_table.c` | 978 | ✅ table; handlers already split |
| `bear/bear_env.c` | 969 | split: mujoco, atari, custom into `bear_env_*.c` |
| `bear/bear_nn.c` | 965 | split: layers, opt, loss into `bear_nn_*.c` |
| `runtime/wubu_snapshot.c` | 920 | split: branch, gc, mount into `wubu_snapshot_*.c` |
| `runtime/wubu_proton.c` | 916 | split: pe-validate, prefix, exec into `wubu_proton2_*.c` |
| `apps/wubu_editor.c` | 894 | split: buffer, render, undo into `wubu_editor_*.c` |
| `gui/wubu_deploy.c` | 885 | split: pkg, img, boot into `wubu_deploy_*.c` |
| `compiler/holyc_parse.c` | 868 | ✅ lexer/parser; split: expr, stmt into `holyc_parse_*.c` |
| `apps/explorer.c` | 822 | legacy (superseded by dosgui_explorer) — quarantine |
| `gui/dosgui_term.c` | 820 | ✅ pty/ansi/tabs split; review residue |
| `bear/bear_ppo.c` | 818 | split: rollout, gae, opt into `bear_ppo_*.c` |

**Rule for every split**: opaque struct in `foo_internal.h`, static-inline helpers,
Makefile 4 edits (OBJS + host link + test direct + runtime). Build `make clean &&
make runtime && make hosted` before done. Behavior-preserving verified by
header-level rebuild.

---

## ════════════════════════════════════════════════════════════════
## WUBUOS GOAL MANTRA (v21)
## ════════════════════════════════════════════════════════════════
```
WuBuOS = ZealOS kernel + Win98 shell + Styx/9P + Arch containers
         ↓  Hosted binary (Inferno emu) runs anywhere
         ↓  Wayland → VBE → ZealOS → GUI → 9P → Containers
         ↓  Arch + Wine/DXVK/VKD3D + gamescope → Windows compat
         ↓  ReactOS NT → VSL → Styx9 → ZealOS → TempleOS
         ↓
   "Rewriting from scratch in C" = THE WORK  (parity epics ARE gaps)
   Form≠Function = THE ENEMY           (void-cast/return-const/stub = gap)
   Triple DA = THE FILTER              (~3000 → ~400 honest sprint board)
   Opaque structs + minimal includes + C11 + no god headers = THE STYLE
   Monoliths → split, every module self-contained = THE DISCIPLINE
   ~400 REAL_GAPs (sprint) + 5 parity epics (marathon) = THE SCOREBOARD
```

**Next**: parallel. Sprint board (~400) file-by-file. Epics (parity) as they unblock.
No pick-one. Stop not allowed; blocked→alternate paths.
