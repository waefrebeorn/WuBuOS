# WuBuOS True-OS Triple Devil's-Advocate — OS Baseline & Framework Setup Runthrough

Date: 2026-11-06 · Host: Linux Mint 22.3 (this fresh box) · wubuos HEAD `e4339dc`
Honesty discipline (standing): every claim below is grounded in source read + command output
this session, not memory. Nothing is "declared an OS"; the ladder is measured.

---

## 0. The honest baseline — "is WuBuOS an OS?"

The user's charge: *"wubuos is not an OS, its a uefi booted c applet, there is no os."*

Measured state (fresh commands, 2026-11-06):

- Boot ladder: **L1 bare-metal BIOS bootsector exists** (`boot.S`); **L2 C11 UEFI firmware exists**
  (`src/firmware/`: fw_acpi/fw_ata/fw_ahci/fw_agi + `loader/` + `run-agi.sh`, docs: qemu-boot-verify).
  So it boots on metal. It is NOT merely "a UEFI booted applet" — it out-boots that.
- Scale: BATTLESHIP.md v23 — 2463 `.c` / 1006 `.h` / 472,955 LOC / 414 `make test_*` targets,
  `make all` / `make hosted` exit 0. This is a mature tree, not a toy.
- **But**: the shipped product is the **hosted binary** (`src/hosted/wubu`) — the kernel runs
  **in-process**, ring-0, **one flat address space**, every "user" call crossing no privilege
  boundary. No protected processes, no per-process page tables, no kernel↔userspace split at
  runtime.

**Verdict (honest):** WuBuOS is a working **monolithic ring-0 kernel-program** that boots on
hardware (L1+L2) and is deep and wide — but by the strongest definition (an OS = a trusted
kernel that owns isolated processes / address spaces / objects) it is **not yet an OS**. The
"C applet" attack is *directionally correct at the isolation layer* and *false at the scale
layer*. The build-out is not "add an OS" — it is "make the existing kernel a real isolator,
and replace the immediate-mode WM with a real compositor of record."

---

## 1. Triple Devil's-Advocate

### DA #1 — the isolation/ownership test ("what must be true for it to BE an OS")

A real OS's defining property: **the kernel owns the hardware and every unprivileged actor's
memory; nothing user-trusted touches ring-0**. Pass the current WuBuOS through this:

| Requirement | Current WuBuOS | Status |
|---|---|---|
| Protected processes (per-proc address space) | Hosted: one flat address space, in-process | ✗ MISSING |
| Kernel/user privilege split at call time | `vsl_syscall_dispatch` routes in-process; no `iret` boundary | ✗ MISSING |
| Object ownership + capability revocation across processes | `wubu_cap` exists (NT-adjacent capability core) but no cross-process objects | ◑ PARTIAL |
| Memory isolation / fault containment | One `malloc` heaps the kernel and "apps" share | ✗ MISSING |
| Scheduler owns cores, preemption p99 | EEVDF `tasking.c` exists, but tests are hosted-unit, not multi-proc | ◑ PARTIAL |

DA#1 conclusion: the single highest-leverage gap is **process/address-space isolation**. Every
"make it Windows / make it an OS" item sits on top of this. Without it, WuBuOS is a fancy
applet no matter how deep the syscall table is. **Go heavy first = the memory/process/isolator
layer (or the honest L3 self-host boundary), not more GUI chrome.**

### DA #2 — the "Windows-system" test (ReactOS backbone + Win11 WM)

The user's two asks: steal the **ReactOS backbones to make Windows systems**, and treat the
**Windows 11 window manager as the target WM**.

What actually exists (measured):
- NT syscall layer: **20 `vsl_nt_*.c` modules** — alpc, atoms, enclave, io, ioring, job, ktm,
  misc, **misc_w11**, partition, process, registry, section, sync, thread, timer, token, vmem,
  wnf, worker + ordinal translation + `vsl_syscall_nt_test.c`. So the W11-era *syscall*
  backbone (ALPC/WNF/IoRing/Enclave/Partition/KTM) is largely mapped. `make test_vsl_nt` is a
  real gate.
- NT registry exported as Styx9P at `/n/nt/registry` (`src/runtime/styxfs_server.c`).
- **Missing**: the *window* backbone. Windows' actual WM lives in **win32k.sys** — a
  **kernel-mode driver holding USER (the window manager: window station, desktops, window
  classes, message queues, hooks) and GDI (the graphics engine)**, with **csrss.exe** as the
  user-mode subsystem server (NT 4 split kernel/user for speed). VSL dispatches *syscalls*; it
  does **not** provide win32k's USER/GDI object model. So "make Windows systems" needs that
  backbone, not more syscall numbers.

Current WM, measured from source:
- Real draw path = `wm_render(fb,w,h)`: background, icons, every window (chrome +
  `on_draw`), taskbar, notification + cursor drawn **directly to one framebuffer, every
  frame**. The dirty-region/retained system was **already removed** ("dead dirty-region
  tracking the hosted loop redraws every frame").
- `wubu_compositor.c` (a Wayland compositor on libwayland-server + xdg-shell + Vulkan) exists
  but **`wubu_compositor` is referenced nowhere in `wm*`/`hosted`** — it is orphaned
  veneer (form≠function).
- "snap" today = desktop-icon grid snap (`snap_icon_to_grid`), NOT Windows-11 Snap Layouts.

DA#2 conclusion: the "Windows system" build-out = **(1) a win32k-style kernel-mode
window-manager object model (USER: station/desktop/window-class/message-queue + GDI surface
abstracts) on top of the existing VSL NT syscalls**, and **(2) a DWM-style retained-mode
compositor** replacing the immediate-mode single-framebuffer redraw. That is the second
highest-leverage item, and it is also where the Win11 steal is.

### DA #3 — the steal-workflow / bootstrap test (what to take from each of the four sources)

The `os-source-steal-workflow` contract: clone target into `vendor/` (or `os-studies/`),
mine **citable facts**, map each stealable to the **Styx/9P `/n` control plane**, write the
study, emit a **P1–P5 build spec**. Current state: `os-studies/` has `arch`, `gnome`,
`steamos`, `templeos` studies — **no `reactos`, no `windows11`, no `cinnamon`/`mint` study
yet**; `vendor/` has the SteamOS/Linux precedent.

DA#3 conclusion: the *research/framework* layer is genuinely missing four studies + four /
control-plane maps. That is the "wubuos needs a lot of frameworks setup" truth. Bootstrap the
fresh machine (install qemu — **qemu is NOT installed on this host**, so the guest boot path
cannot run yet), clone the four sources, write the four studies, and only then pick the first
build spec.

---

## 2. The four-source research (citable, mapped to /n)

### 2.1 OMARCHY installer — the Arch install/boot stack
- **archinstall** *is a library* ("archinstall is a library which can be used to install Arch
  Linux ... comes packaged with pre-configured installers") with a guided flow; options:
  `bootloader` (grub/limine; BIOS needs grub/limine) + `uki` (unified kernel images, UEFI +
  systemd-boot/limine) + `removable` (`/EFI/BOOT/`). — man.archlinux.org/man/archinstall.1.en
- **UKI** (ArchWiki "Unified kernel image"): a single UEFI PE merging the `systemd-stub` boot
  stub + kernel + initramfs + cmdline (+ dv-verity root hash); assembled by `kernel-install`
  (`/etc/kernel/install.conf` layout=uki) + `mkinitcpio`/`dracut` (initramfs) + `ukify`
  (`systemd-ukify`). — wiki.archlinux.org
- **mkinitcpio**: hook ordering (`udev/block/keymap` before fs), preset files, module-dep
  closure → emits initramfs or folds into the UKI.
- WuBu OMARCHY goals (registry, 14 items) ALREADY name the target modules:
  `wubu_keyring.c` (pacman PGP sig), `wubu_aur.c`, `wubu_unit_parse.c` (systemd),
  `wubu_btrfs_send.c`, `wubu_hypr_conf.c`, `wubu_pkg_txn.c`, `wubu_initramfs.c` (mkinitcpio-
  style), UKI (unified kernel image assembly), `.pacnew` merge, journald, systemd timers,
  Landlock, layer-shell surfaces, reproducible build manifest.
- **Stealable → /n**: `/n/pac/{verify,txn,aur}`, `/n/boot/uki`, `/n/initramfs`, `/n/units`,
  `/n/journal`, `/n/timer`. The installer-as-workflow (what archinstall's guided flow does)
  is the orthogonal piece: partition → bootloader → UKI → root; that maps to `/n/install` and
  is what "the omarchy installer" most usefully means.
- **Steal FIRST (match WuBu ethos, self-contained C11):** mkinitcpio-style initramfs +
  UKI assembly (a bootable, verifiable chain off the existing WuBuFW), then pacman sig keyring
  + transaction planner.

### 2.2 Linux Mint desktop environment — Cinnamon (running live on this host)
- **Muffin** (github.com/linuxmint/muffin) = the WM + compositor, **a fork of Mutter**
  (Wayland display server / X11 WM+compositor, on Cogl/Clutter forks). Cinnamon's **panel and
  window-list run in-process via the CJS JS layer**; it never used libWnck (that's MATE).
- **Hybrid split** (deepwiki.com/linuxmint/cinnamon): a **C core** (`CinnamonGlobal`
  singleton exposing window mgmt/theming/display to JS; `St` CSS-themed widget toolkit over
  Clutter; `CinnamonPlugin` = the Muffin plugin bridging UI↔WM) + a **JS UI layer**
  (`main.js`, `LayoutManager`, `PanelSystem`, a JS `WindowManager` interface to Muffin).
- **Extensibility**: applets (panel widgets), desklets (desktop widgets), extensions.
- **Stealable → WuBu /n design** (C only, no JS): the *partition* matters — a thin C WM core
  (`wm`) + a **scripted/themeable UI layer on top** (WuBu's C11 equivalent of the
  `CinnamonPlugin` bridge), a CSS-ish theming language over `vbe`/surface primitives, and a
  first-class **applet/desklet registry** (WuBu already has `g_app_defs` + a control-panel
  manager — extend that into a real applet/desklet/extensions triple).
- **Steal SECOND (it is the present-day, on-display UX target the user is literally sitting
  in):** the panel/applet model + the plugin-bridge architecture. Map to `/n/shell/{panel,
  applet,menu,theme}`.

### 2.3 ReactOS backbones — "make Windows systems"
- **win32k.sys** = the kernel-mode driver holding **USER (the {actual} window manager:
  window station, desktops, message queues, window classes, hooks) and GDI (the graphics
  engine)**. **CSRSS** (user-mode subsystem server) handles consoles, process bookkeeping,
  shutdown; since **NT4 the windowing was split user/kernel for speed**. (vectree/reactos,
  reactos.org/wiki/Remove_CSRSS)
- **NT kernel** (reactos.org/architecture): Object manager, IO manager, Executive, NT thread
  scheduler; Win32 API = kernel32/gdi32/user32; targets NT 5.2. Window manager sources live
  at `win32ss/user/ntuser/`.
- **Stealable → /n**: the USER object model (station/desktop/window-class/msg-queue) as
  `/n/win32/{stations,desktops,wclasses,queues,hooks}` exposed on the SAME backing win32k
  that the NT syscall handlers use — exactly the pattern already proven with
  `/n/nt/registry`. That turns VSL's syscalls into a *system*.
- **Steal THIRD** (it is the "make Windows systems" backbone the user named): win32k GDI/USER,
  wired to the existing 20-module VSL NT layer.

### 2.4 Windows 11 window manager — the WM target
- **DWM** (en.wikipedia Desktop Window Manager, Microsoft docs): a **compositing WM**. Each
  window writes to its own buffer; DWM composites off-screen surfaces **in video memory via
  DirectX on the GPU** (CPU freed from surface→display). It uses **MIL** (Media Integration
  Layer, shared with WPF): windows are **composition nodes in a retained-mode composition
  tree** — cached, depth-ordered, repaints handled by DWM/MIL not the app.
- **Shell/UX stealables** (Microsoft learning-center): **Snap layouts** (Win+Z; preset grids,
  halves→quarters→thirds), **Snap assist** (thumbnail pick of next window), **Snap groups**
  (the snapped set persists; taskbar hover restores; remembered across monitor reconnect),
  **Task View virtual desktops** (Win+Tab; per-desktop layouts). Multitasking Settings toggles.
- **Stealable → WuBu**: the **retained-mode composition tree** (each `dosgui` window is an
  offscreen surface; the compositor owns ordering + repaint) — this directly replaces the
  removed dirty-region/full-redraw path; **Snap layouts + Snap groups + virtual-desktop
  layouts** (virtual-desktop switching already exists in `wm_desktop.c` — promote it
  to  per-desktop window sets + snap groups) — note: virtual-desktop switching lives in
  `src/gui/wubu_wm_desktop.c` (switch/next/prev/move); `wm_desktop.c` is the desktop-icon
  grid subsystem.
- **Steal FOURTH** — it is the *target WM*, but it sits ON the win32k backbone (2.3). Order
  matters: backbone before chrome.

---

## 3. /n control-plane map (one namespace, steal-by-steal)

| Concern | Source | /n surface |
|---|---|---|
| Arch install/boot | OMARCHY/archinstall/UKI | `/n/install`, `/n/initramfs`, `/n/boot/uki`, `/n/pac/{verify,txn,aur}`, `/n/units`, `/n/journal`, `/n/timer` |
| Desktop shell | Cinnamon/Muffin | `/n/shell/{panel,applet,desklet,menu,theme}` |
| Windows system backbone | ReactOS win32k (USER/GDI) | `/n/win32/{stations,desktops,wclasses,queues,hooks}` (+ existing `/n/nt/registry`) |
| Window manager target | Win11 DWM/MIL/Snap | `/n/wm/{compositor,snap,groups,desktops}` |

---

## 4. Framework setup roadmap — make the fresh machine + repo ready

### 4.1 Bootstrap the host (fresh-machine setup) — do in order
1. **Install qemu** (`qemu-system-x86_64`) — currently ABSENT; the sanctioned guest-boot path
   cannot run until it exists. `apt install qemu-system-x86` (needs your sudo; else user-level
   `~opt` build).
2. Confirm `make all` / `make hosted` on this box (hosted = host OS binary); the guest
   (`make qemu`, run-agi.sh) needs qemu.
3. Ladder proof available here: `bash src/firmware/qemu-boot-verify.sh` (L2) once qemu is in.
4. Edge cases: no `nvcc` on this host — the `-DWUBU_ENABLE_CUDA`/`.cu` paths are compile-out;
   keep CPU/`WUBU_HOSTED` builds the verification path here.

### 4.2 Repo framework wave (P-level build specs)
- **P1** — win32k-lite: wrap the existing VSL NT syscalls with a kernel-mode window-manager
  object model (window-station/desktop/window-class/message-queue) + export as `/n/win32/*`.
  Gate: `make test_vsl_nt` still green + a `/n/win32` walk test.
- **P1** — initramfs + UKI assembly (`wubu_initramfs.c`, UKI); gate: packed cpio lists +
  assembled UKI verifies under the existing WuBuFW.
- **P2** — retained-mode composition tree in `wm`: per-window offscreen surfaces +
  compositor-owned repaint (the DWM/MIL pattern); gate: a render test asserting only the
  changed window's surface re-composites.
- **P2** — pacman signature keyring + transaction planner (`wubu_keyring.c`, `wubu_pkg_txn.c`);
  gate: `make test_keyring`, `make test_pkg_txn`; the real gates are `make test_cap` / `make test_txn` and the newly-wired `make test_kvfs` / `make test_vulkan`.
- **P3** — Snap layouts + Snap groups + per-desktop window sets (promote `wubu_wm_desktop`
  to per-desktop window sets + snap groups); gate: WM snap/groups tests.
- **P3** — Cinnamon-style shell split: thin C WM core + applet/desklet/theme registry +
  scriptable UI layer (C11; the `CinnamonGlobal`/`St`/`CinnamonPlugin` pattern adapted to
  vbe surfaces).
- **P4** — win32k GDI (surface/geometry objects) + real `user32/gdi32` personalities.
- **P5** — end-to-end isolation: protected processes + per-proc address spaces + cap
  revocation across processes (DA#1), making the O/S label true.

Order discipline: **P1 backbone → P1 boot chain → P2 compositor → P2 package → P3 shell/WM →
P4 GDI → P5 isolation.** That sequence converts "deep applet" into "real OS" while each step
ships a verifiable gate.

### 4.3 Studies to write (research framework)
- `os-studies/reactos_study.md` (win32k USER/GDI + csrss; ~300 syscall ordinal refs already in
  the tree)
- `os-studies/windows11_wm_study.md` (DWM/MIL retained composition + Snap/TaskView)
- `os-studies/cinnamon_study.md` (Muffin + CJS split + applet/desklet/extensions) — the host's
  live DE, study from the running binaries too
- `os-studies/omarchy_install_study.md` (archinstall guided flow + UKI/kernel-install chain)

---

## 5. Deliverables this session
- wubuos cloned to `/home/wubu/wubuos` (HEAD `e4339dc`); wubuwizard at `/home/wubu/opt/wubuwizard`.
- Four-source research captured (this doc, sources citable) + Triple-DA against the real tree.
- Four verified source studies written to `os-studies/` (each `sources.py verify` = citations OK,
  shared ledger `.citations-ledger.json`): `omarchy_install_study.md`, `cinnamon_study.md`,
  `reactos_study.md`, `windows11_wm_study.md`. These are the build-spec inputs for the roadmap.
- This doc → `docs/TRIPLE_DA_OS_BASELINE_2026-11-06.md`.
- Repo worktree left uncommitted (main/master ff-only discipline; review before push).

Next concrete step (per standing "go heavy first"): install qemu, then start **P1 win32k-lite**
(wrap VSL NT → window-manager object model → `/n/win32`), because every "Windows system" and
"Win11 WM" goal stands on it.
