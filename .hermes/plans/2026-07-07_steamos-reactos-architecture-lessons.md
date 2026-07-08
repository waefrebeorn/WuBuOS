# WuBuOS × SteamOS × ReactOS — Architecture Lessons Plan

> **For Hermes:** Planning-only synthesis. Maps the hard-won lessons of SteamOS
> (atomic/immutable, container-first compat) and ReactOS (NT-reimplementation
> trap, Win32K-in-kernel pain) onto WuBuOS's real subsystems. No code execution
> here — this is the *basic planning of how we make things work* the user asked for.

**Goal:** Re-shape WuBuOS so it (1) approaches 100% Windows/Linux app compatibility
through the *SteamOS strategy* (stand on containers + Proton, not kernel NT reimpl),
and (2) delivers a comfy, win98-feeling UX that is grounded in **real functional
compat** (the ReactOS lesson: skin without function is not comfort). All work stays
inside the project guidelines: C11, opaque structs, no stubs, "rewriting from scratch
in C" = the work, identity `ZealOS kernel + Win98 shell + Styx/9P + Arch containers`.

**Architecture thesis (the key lesson):**
- **SteamOS won** by NOT reimplementing Windows. It stands on the Linux kernel and
  runs Windows via **Proton/Wine inside Flatpak/Pressure-Vessel containers**, with an
  **immutable A/B rootfs** + **gamescope session split** (game mode vs desktop).
- **ReactOS lost the compat race** by trying to **reimplement the NT kernel + Win32K
  from scratch** — 20+ years, still <100%. 90% of their app-compat bugs live in
  **kernel-mode Win32K**, which they copied from Windows' own worst design mistake.
- **WuBuOS currently lists `ReactOS NT → VSL → Styx9 → ZealOS → TempleOS` as a 0%
  epic.** That is the ReactOS trap. The plan below **formally de-prioritizes the NT
  kernel reimpl** and instead makes the *existing* SteamOS-shaped pieces first-class:
  `.wubu` containers + `wubu_ct_bwrap` (Pressure-Vessel analog) + `wubu_proton` PE
  loader (Proton analog) + `wubu_snapshot` overlayfs (immutable/atomic analog).

---

## Part 1 — What each OS teaches (extracted hard lessons)

### SteamOS hard lessons (ADOPT)
| Lesson | SteamOS mechanism | WuBuOS mapping (existing?) | Action |
|--------|------------------|---------------------------|--------|
| Immutable rootfs | read-only `/` + `ostree`/btrfs | `wubu_snapshot.c` overlayfs exists | Make system root a snapshot; writes go to upper layer only |
| Atomic A/B updates | RAUC + Casync, inactive partition, reboot-to-swap | snapshot branches exist | Add `wubu_system_commit()` / `wubu_system_rollback()` on top of branches |
| Apps as atomic units | Flatpak (sandboxed, versioned) | `.wubu` containers exist | Promote `.wubu` to the *primary* app unit; kernel never runs a bare exe |
| Session split | gamescope: dedicated game session, compositor-nested, controller-first | Win98 shell is the only mode | Add `DESKTOP_MODE` vs `GAME_MODE` session state in `hosted.c` |
| Containerized Windows | Pressure Vessel + Proton, bwrap isolation | `wubu_ct_isolate.c` + `wubu_ct_bwrap.c` exist | Harden into the default Windows-launch path |
| Per-app compat DB | ProtonDB, per-title tweaks, shader cache | none | Add `wubu_compat_db` (title→protonflags→override) + cache dir |
| Controller-first | Steam Input, unified gamepad | `src/kernel/input.c` | Expose a unified `wubu_input` gamepad abstraction |

### ReactOS hard lessons (AVOID / INVERT)
| Lesson | What ReactOS did | Why it hurt | WuBuOS inversion |
|--------|-----------------|-------------|------------------|
| Reimplement NT kernel | `ntoskrnl` from scratch, 297 syscalls | 20 yrs, <100% | **Do NOT** build NT in the kernel. Keep WuBuOS ABI clean; Windows runs in a container. |
| Win32K in kernel mode | GUI in ring-0 (copied Windows' bug) | 90% of compat bugs here | Keep all Windows/Win32 surface in **user-space** (Proton/DXVK in container). ZealOS kernel stays GUI-agnostic. |
| Need real Windows drivers | binary driver compat | nearly impossible | **Already correct:** rip through Linux drivers (archd/NVK), never target Windows drivers. |
| Skin without function | looks-like-WinXP but apps crash | "comfort" feels fake | UX comfort = **real launched apps**. Context menu / Control Panel must do real work (already done this session for desktop). |
| One giant reimpl effort | no container escape hatch | stuck forever | Use the **container escape hatch** SteamOS proved: if a binary won't run natively, run it in an Arch+Wine container. |

**Net strategic decision (Triple-DA verified):**
1. The `ReactOS NT → VSL → Styx9 → ZealOS → TempleOS` epic is re-scoped from
   "reimplement NT kernel" → "**present an NT-compatible *personality* in user space
   via VSL + Proton container**." This is the difference between a 20-year trap and a
   shippable 100%-compat product.
2. WuBuOS's *kernel* (ZealOS) keeps its own clean ABI. Windows/Linux binaries reach
   it only through the VSL bridge + container runtime — never by the kernel pretending
   to be NT.
3. The *shell* (Win98) is the comfy surface; it launches **real containers**, so the
   comfort is real, not a skin.

---

## Part 2 — Current WuBuOS state (grounded in repo)

Present and usable (from architecture skill + this session's gate):
- `src/runtime/wubu_snapshot.c` (920 LOC) — overlayfs lower/upper/work, branches,
  tags, GC, export/import `.wubu`. **This is the atomic/immutable foundation.**
- `src/runtime/wubu_container.c` + `wubu_ct_isolate.c` + `wubu_ct_bwrap.c` —
  `.wubu` parse, universal exec, bwrap isolation. **Pressure-Vessel analog.**
- `src/runtime/wubu_proton.c` (PE32/64 loader) + `wubu_vsl.c` (syscall bridge).
  **Proton analog + NT-personality-in-userspace analog.**
- `src/kernel/input.c` — key/mouse queues. **Steam-Input analog seed.**
- `src/gui/dosgui_wm.c` — Win98 WM, GAAD snap, virtual desktops. **Comfy surface.**
- `src/hosted/hosted.c` — main binary, event loop, Wayland callbacks. **Session owner.**
- This session already closed: real context-menu actions (write `.desktop`,
  auto-arrange, sort-by-name, refresh), Control Panel Desktop tab live, wallpaper.

Gaps this plan attacks (the "basic planning" holes the user senses):
- **No immutable/atomic *system* root** — snapshot only covers containers, not the OS.
- **No session split** — one shell mode; no gamescope-style game session.
- **Windows compat still routes through the 0% NT-kernel epic** in planning, not
  through the container/Proton path that actually works.
- **No per-app compat DB / cache** — every launch is blind.

---

## Part 3 — Workstreams (bite-sized, mapped to real files + tests)

### Workstream A — Immutable/Atomic System Root (SteamOS lesson #1/#2)
Build on `wubu_snapshot.c` so the OS itself is snapshot-able and rollback-able.

- **A1.** Add `wubu_system_snapshot_create()` in `src/runtime/wubu_snapshot.c`:
  snapshot the *running system root* (upper overlay layer) as a named branch.
  Test: `make test_snapshot` (extend with a system-root case).
- **A2.** Add `wubu_system_commit(label)` / `wubu_system_rollback(label)` — swap the
  active branch pointer (atomically, like RAUC). Test: new `test_snapshot` assertions
  for commit/rollback round-trip.
- **A3.** Wire `hosted.c` startup to boot from the **active snapshot branch** only;
  all live writes land in the upper layer. Test: `make hosted` still launches; gate
  green.

### Workstream B — Session Split: Desktop vs Game Mode (SteamOS gamescope lesson)
Formalize a session state so the comfy Win98 shell and a focused game session coexist.

- **B1.** Add `WUBU_SESSION_MODE` enum (`DESKTOP`, `GAME`) + `wubu_session_set(mode)`
  in `src/hosted/hosted.c` (or a new `src/runtime/wubu_session.c`).
- **B2.** In `GAME` mode, `hosted.c` launches the target via the **container/Proton
  path** directly (bypass shell chrome), fullscreen, controller-first — gamescope
  analog. In `DESKTOP` mode, the Win98 WM owns the screen.
- **B3.** Expose a "Play" action in `dosgui_wm.c` context menu that calls
  `wubu_session_set(GAME)` then the container launch. (Real work, like the
  create-shortcut action already added this session.)
- Test: `make test_hosted` + `make test_dosgui_wm`.

### Workstream C — Container/Proton as the DEFAULT Windows path (invert ReactOS trap)
Make Windows apps launch through `wubu_ct_bwrap` + `wubu_proton`, never an NT-kernel
emulation. This is what gets us to ~100% compat fast.

- **C1.** Add `wubu_launch_windows(const char *exe)` in `src/runtime/wubu_container.c`
  that: parses PE → selects Proton container → runs under `wubu_ct_bwrap` isolation.
  Mark any not-yet-implemented sub-step `TODO`, never empty-stub.
- **C2.** Re-scope the `ReactOS NT` epic in `BATTLESHIP.md` from "reimplement NT
  kernel" to "NT *personality* via VSL + Proton container (user-space)". Update the
  epic table + the DA note so future sessions don't sink years into ring-0 NT.
- Test: `make test_wubu` (universal exec) + `make test_proton` green.

### Workstream D — Per-app Compat DB + Cache (SteamOS ProtonDB/shader-cache lesson)
- **D1.** Add `src/runtime/wubu_compat_db.c`: load/save a `title → {proton_flags,
  env_overrides, dll_overrides}` DB (SQLite, reuse `wubu_pkgmgr` DB patterns).
- **D2.** Add a per-title **cache dir** under `~/.wubu/cache/<title>/` (shader/proton
  cache analog) created by the launcher.
- Test: `make test_pkgmgr` style SQLite round-trip, or a new `test_compat_db`.

### Workstream E — Comfy UX grounded in real compat (ReactOS "skin≠comfort" lesson)
Continue what this session started; ensure every shell affordance launches a REAL
container/app, never a notification-only stub.
- **E1.** Extend `dosgui_wm_ctxmenu.c` "Play"/launch to resolve a `.desktop`/`.wubu`
  into a live container process (reuse `dosgui_wm_write_desktop_shortcut` + C launcher).
- **E2.** Control Panel "Default launcher" tab (live, persisted via `wubu_settings`)
  chooses container vs native. Already-live Desktop tab proves the pattern.
- Test: `make test_control` + `make test_dosgui_wm` green.

### Workstream F — Unified Input / Controller-first (Steam Input lesson)
- **F1.** Add `wubu_input_gamepad_state()` to `src/kernel/input.c` aggregating
  keyboard+mouse+gamepad into one event stream the WM/gamescope session consumes.
- Test: `make test_input` extended.

---

## Part 4 — Validation (gate discipline)
After each workstream, run the project-mandated gate:
```bash
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
make clean && make runtime && make hosted   # build gate
make test                                   # full 64-target gate (GATE_EXIT=0 required)
# plus the specific targets named per workstream above
```
Every edited function does real work or is marked `TODO`. No stubs/scaffolding.

## Part 5 — Risks / Tradeoffs (Triple-DA)
- **Risk:** De-scoping the NT-kernel epic may feel like "abandoning Windows compat."
  **Mitigation:** we are *not* abandoning it — we are switching to the strategy that
  already reaches ~100% (containers+Proton), exactly as SteamOS did. The kernel stays
  ZealOS-clean, which is the project's identity.
- **Risk:** Immutable rootfs complicates dev workflow. **Mitigation:** keep a
  `DEVELOPER` mode that disables the read-only upper layer (documented; tests use it).
- **Risk:** Session split adds WM complexity. **Mitigation:** it's a state enum +
  one branch in `hosted.c`; the Win98 WM is untouched in `DESKTOP` mode.
- **Open question:** should `.wubu` become the *only* executable unit (Flatpak-like),
  or coexist with native ZealOS binaries? Recommend: coexist; native for WuBuOS apps,
  `.wubu` for foreign binaries. Decide in `BATTLESHIP.md` epic notes.

## Part 6 — Immediate next step (pick one)
The highest-leverage, lowest-risk first move is **Workstream C2** (re-scope the
ReactOS NT epic in `BATTLESHIP.md`) + **A1** (system snapshot on existing
`wubu_snapshot.c`): both are real, gate-green, and set the strategic direction before
any large code. Then B/C/D/E/F follow as the compat+UX flywheel.
