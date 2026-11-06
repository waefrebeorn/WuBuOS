# WuBuOS Baseline — consolidated from three reference OS studies (2026-11-06)

This doc ties three source-grounded studies into ONE baseline WuBuOS must match and ONE
build roadmap. The three legs answer "what does a usable, self-hosting, compiler-hosted
OS actually need behind the scenes?" from three necessary angles.

Companion studies (read each for the citations):
- `linuxmint_background_baseline.md` — the background runtime tiers (29 components, 36 sources).
- `zealos_holyd_study.md` — what the HolyC/ZealC compiler-runtime needs from the OS (6 needs, 7 sources).
- `reactos_holyd_study.md` — how compiled code runs on NT/ReactOS (the PE-loader host contract; 13 sources).

---

## The one-line picture

WuBuOS needs (a) a Mint-style **background runtime** (the always-on daemons plus the
service/DBus-activation contracts that make a desktop usable), (b) a **holyd runtime** whose
compiler is an in-OS JIT library (ZealOS's model: compile → `Call(machine_code)` from a REPL
on executable memory, a live symbol registry, and a module loader), and (c) a **PE host**
(ReactOS's loader contract) so holyd-compiled output can actually be loaded and started.

## LEG 1 — Mint background baseline → `/n/background` surface

Five tiers (inner→outer) that WuBuOS should mirror as surfaces, not one monolith:

| Tier | What runs | Mint component | WuBuOS `/n/background` surface |
|---|---|---|---|
| PID1 + system bus | service supervisor, systemwide IPC | systemd, dbus-daemon --system | `/n/svc/` (units/lifecycle), `/n/bus/` (system msg bus) |
| User manager | per-user units + session bus | systemd --user, dbus session | `/n/svc/user/`, `/n/bus/session/` |
| Session manager | holds logind session, xdg-autostart, parents DE daemons | cinnamon-session | `/n/session/` (autostart, seat) |
| Autostart/tray tier | always-on tray/indicator daemons | nm-applet, blueman, power, mintUpdate, mintreport-tray, warpinator | `/n/status/` (net/bt/power/updates) |
| DE daemon framework | cross-DE background + desktop-owner proc | XApp (xapp-sn-watcher, StatusIcon), nemo-desktop | `/n/bg/` daemon registry, `/n/desktop/` |

**The five background-runtime contracts** to steal (they define "a desktop is usable"):
1. D-Bus activation (service starts on first message) — `/n/bus/*.activate-file`
2. Socket activation (service starts on first connection) — `/n/svc/*.socket`
3. XDG Autostart spec (`~/.config/autostart/*.desktop`) — `/n/session/autostart/*`
4. logind session/seat lifecycle — `/n/session/:1` (class/type/seat/active)
5. polkit authorization gate — `/n/auth/` (allow/deny per action+caller)

## LEG 2 — ZealOS → holyd runtime needs (the compiler-as-OS JIT model)

ZealOS self-hosts its compiler: `ExeCmdLine` compiles a statement to a machine-code buffer
then executes it in place (`machine_code = LexStatement2Bin(...); ... Call(machine_code);`).
Verified from the cloned `Compiler/CMain.ZC`, `Kernel/KTask.ZC`, `Kernel/KLoad.ZC`. That
confirms **6 concrete holyd requirements**:

| ZealOS mechanism (source-confirmed) | holyd requirement |
|---|---|
| Executable code heap + `Call(machine_code)` | executable memory: map emitted code RWX and call via a function pointer in place |
| Global symbol/type hash registry (`HashGenericAdd`, `HTT_MODULE|HTF_PUBLIC`) | runtime-extensible symbol/type registry, live after boot, gains module entries |
| `ExeCmdLine` / `UserTaskCont` terminal REPL | REPL entry: compile a source string → execute → survive a failed line (try/catch) |
| `Load()` of `.ZXE` (alloc/align/copy/`LoadPass1`+`LoadPass2` relocate/link/call) | DLL-style app-loader with load-time relocation + call entry, registered by name |
| `ExePutS`/`ExePrint`/`Comp`/`CompBuf` exported as in-OS calls | compiler-as-library API: lex→parse→emit→call as ordinary callable functions |
| `LexStatement2Bin` returns runtime type + `Fs->answer/answer_type`; I64/F64 in `CCompCtrl` | first-class 64-bit scalar types + value-with-type result from executed statements |

Honest leftovers: identity-map mechanism, the `M(argc,...)` macro system, and exact
Just/Unsafe semantics were NOT source-confirmed this session → `[unverified]`.

## LEG 3 — ReactOS → holyd PE host (the loader/execution contract)

How a compiled image actually starts on NT/ReactOS (from the cloned ReactOS tree):
kernel pre-maps ntdll (`PspMapSystemDll`) → `PspCreateProcess` builds EPROCESS + PEB page →
`PspCreateThread` rides a user APC into `LdrInitializeThunk` → `LdrpInitializeProcess` runs
the loader: PE-header parse (`RtlImageNtHeader`), section mapping, import walk + IAT snap,
base relocation, TLS setup + callbacks, then `ImageBase + AddressOfEntryPoint`. The loader
rides on ntdll = the single user-mode syscall boundary, the ProcessHeap
(`RtlAllocateHeap`), and the loader lock/data.

**The 7-piece loader contract a holyd-compiled PE must satisfy** (and the two hosting
modes: load it via an Ldr-equivalent, OR present it as an in-process native module using the
same image-map + fixup + import + entry contract):

| Loader step (ReactOS source) | holyd/WuBuOS equivalent |
|---|---|
| PE-header parse (`RtlImageNtHeader`, data directories) | PE-header parser (RVAs vs in-memory base) |
| Section mapping per headers | image segment mapper (map by file offsets/sizes) |
| `LdrpWalkImportDescriptor` → `LdrpSnapIAT` | import resolver (empty IAT table OK for self-contained) |
| `LdrRelocateImageWithBias` / relocation blocks | relocator; OR reserve base / emit PIC to skip |
| `LdrpInitializeTls` + TLS callbacks | per-thread TLS template + TLS-callback runner |
| `LdrpFetchAddressOfEntryPoint` / init routines | entry-stub + init-order dispatcher into `main` |
| ntdll seed (`PspMapSystemDll`) + `RtlAllocateHeap`/ProcessHeap | resident runtime seed mapped into each hosted process + VSL/RTL heap published as ProcessHeap |

## Merged P-level build roadmap (go heavy first)

- **P1 — holyd runtime core (LEG 2 + LEG 3):** executable-memory backend in holyd
  (`map RWX + Call`), the REPL entry, and a PE loader (`wubu_pe_load`) implementing the
  image-map + relocation + IAT + entry contract of LEG 3, with a test harness that
  hand-assembles/round-trips a tiny PE through load→entry. This makes holyd able to run
  what it compiles, on bare metal and hosted.
- **P2 — symbol/type registry + module loader:** the runtime-extensible registry on `/n`
  (`/n/holyd/syms`, `/n/holyd/types`), plus the `.ZXE`-style module loader with load-time
  relocation registered by name — holyd's "OS is a live compiler" identity.
- **P3 — `/n/background` runtime (LEG 1):** session manager + bus + autostart + the five
  service contracts, starting with `wubu_session` (holds the seat, runs xdg-autostart) and
  `wubu_bus` (message bus with activation), then the status/tray surface (`/n/status/`).
- **P4 — first-boot onboarding + installer (Omarchy study):** the greeter
  (logo+tagline+hint), deferred provisioning, and the P1–P3 configurator from
  `omarchy_install_study.md` PART 2.

> Sequencing note: LEG 2/3 (make the compiler able to run its own output) is the forcing
> function — nothing else is "self-hosting" until a holyd-compiled module loads and runs.
> LEG 1 (background) is the desktop-usable layer on top. The Omarchy-first-boot work
> (already specced in `omarchy_install_study.md`) gives the polish/onboarding that makes
> the whole thing feel finished to a new user.