# ReactOS Win32 Backbones Source Study
## Making WuBuOS a Windows-running OS — the win32k + NT-kernel reference

Scope note: this is the research backbone for a build plan. Every load-bearing fact is
cited to an authoritative primary source (reactos.org, the ReactOS source tree via
doxygen.reactos.org, and the VEctree explainer). WuBuOS already ships an extensive VSL
NT syscall layer (ALPC/WNF/IoRing/Enclave/KTM + `/n/nt/registry`); this study answers the
next question: what is the win32k-style kernel window manager + GDI + CSRSS backbone, and
how does WuBuOS map it onto `/n`?

---

## 1. What it is / architecture

### 1.1 The NT kernel underneath: what "Windows personality" actually rides on

The ReactOS kernel is an independent reimplementation of the Windows NT kernel and
Executive "that follows Microsoft's published architecture closely." It is split into
managers: the Object Manager (`Ob`), Memory Manager (`Mm`), I/O Manager (`Io`), Process
and Thread Manager (`Ps`), Security Reference Monitor (`Se`), Configuration
Manager/registry (`Cm`), Cache Manager (`Cc`), and the low-level Kernel (`Ke`) which
handles scheduling, interrupts and synchronization.[15] The ReactOS kernel is "the only
complete implementation of an NT kernel outside of the Microsoft campus," targets NT 5.2
(Windows Server 2003) and supports x86 and x64, with complete implementations of the
Object manager, IO manager, Executive and the NT thread scheduler.[14] Because its
internal structures and exported functions match Windows, real Windows drivers can link
against the ReactOS kernel.[15]

The Win32 API is the "lowest layer of user mode," and its three main DLLs split the work:
`Kernel32.dll` manages threads, processes and kernel services such as file interaction;
`Gdi32.dll` contains the drawing code; `User32.dll` contains code for things such as
scroll bars and buttons.[14] `ntdll.dll` is the single user-mode gateway: when
`kernel32.dll` calls `CreateFileW`, that ends up as `NtCreateFile` in NTDLL, which then
runs a `sysenter`/`syscall` (or `int 2Eh` on old CPUs) to switch into kernel mode.[15]

### 1.2 The Win32 subsystem: the two-part "turns an NT kernel into Windows"

The Win32 subsystem is what "turns the general-purpose NT kernel inside ReactOS into
something that behaves like Windows. It supplies windows, message queues, fonts, drawing
and console handling," and "has two parts, just as on Windows."[15]

- **CSRSS** (Client/Server Runtime Subsystem) is a user-mode server process that handles
  console windows, process bookkeeping and shutdown.[15] It is "an essential component of
  the Windows NT family," providing "the user mode side of the Win32 subsystem," and is
  "mainly responsible for Win32 console handling and GUI shutdown."[17] When a
  user-mode process calls a function involving console windows or process/thread creation,
  the Win32 libraries (kernel32.dll, user32.dll, gdi32.dll) send an *inter-process call*
  (Local Procedure Call) to the CSRSS process instead of issuing a system call — while the
  window manager and GDI services are instead handled by the kernel-mode driver
  win32k.sys.[17]
- **win32k.sys** is a kernel-mode driver that holds the window manager (USER) and the
  graphics engine (GDI).[15] "Since NT4, Windows has put these in the kernel for speed,"
  and `user32.dll` / `gdi32.dll` are "thin user-mode front ends" that send requests into
  win32k "through special system calls (the *shadow* system-call table)."[15] Win32k's GDI
  half provides output to monitors/printers/other devices and "communicates directly with
  the graphics driver."[16] Getting win32k's behaviour to match Windows exactly "is one of
  the hardest and most bug-prone areas of ReactOS."[15]

### 1.3 The win32ss source tree: what a window manager actually contains

Kernel-mode USER lives under `win32ss/user/ntuser/`. The `ntuser.h` shared header shows
the object model in concrete C terms. The desktop object carries a
`DESKTOPINFO` structure typed exactly as: `pvDesktopBase` / `pvDesktopLimit` (the
desktop-heap address range), `spwnd` (the active window), `fsHooks`, an
`aphkStart[NB_HOOKS]` list of per-hook-type hook chains, the special shell windows
`hTaskManWindow`, `hProgmanWindow`, `hShellWindow`, `spwndShell`, `spwndBkGnd`, and
`ppiShellProcess`.[20] Desktops and window stations are first-class, securable objects
with `NtUserCreateWindowStation` and `NtUserCloseWindowStation` in the same header.[20]

`desktop.c` makes the Object-Manager coupling explicit: a desktop open "should 'emulate'
an opening from user-mode (i.e. using an ObjectAttributes allocated in user-mode, with
AccessMode == UserMode) for the Object Manager"; and it registers shell hook windows via
`IntRegisterShellHookWindow`, which inserts into a per-desktop `SHELL_HOOK_WINDOW`
list.[19] `hook.c` implements `NtUserSetWindowsHookEx` and the `co_HOOK_CallHooks` /
`co_CallHook` dispatch that runs the per-hook-type chain stored on each desktop's
`DESKTOPINFO`.[20][21]

GDI's internal layout (from the Win32k design guideline) is a set of well-named modules:
`objects/` (bitmap.c, brush.c, clipobj.c, dc.c, palette.c, path.c, region.c, surface.c,
xlate.c...), `painting/` (bitblt.c, fill.c, lineto.c, textout.c, stretchblt.c, ...),
`driver/`, `dib/`, `misc/` and `ntuser/`.[18]

---

## 2. Stealable techniques

Each row: the ReactOS fact (cited) → the WuBuOS `/n` mapping → our delta-by-design.

1. **Kernel-manager split (Ob/Mm/Io/Ps/Se/Cm/Cc/Ke).**[15][14]
   WuBuOS already re-implements the NT syscall surface; model the win32 window manager as a
   peer manager inside the existing VSL layer rather than a separate "subsystem process."
   Delta: win32k is a monitor (a kernel window-manager authority core), not a ring-0 driver
   loaded late — it is compiled into the ring-0 image and audited like the NT layer.

2. **The two-part split: CSRSS (user-mode console/bookkeeping/shutdown) vs win32k
   (kernel USER+GDI).**[15][17]
   Map CSRSS to a WuBuOS user-mode subsystem server reachable over Styx/9P ALPC, and win32k
   to the kernel window manager. Delta: CSRSS speaks the existing VSL ALPC personality, so
   console + process bookkeeping ride the control plane without a new IPC.

3. **`user32.dll`/`gdi32.dll` as thin front-ends over a *shadow* system-call
   table.**[15]
   WuBuOS maps this to a `NtUserXxx`/`NtGdiXxx` syscall slice that is authoritative (the 
   window manager's behavior — matching on exactly is the hardest part[15]) while the DLLs
   stay thin marshallers. Delta: the shadow-table boundary is *our* enforcement point for
   "no third-party GDI/WM."

4. **Object model: securable window station → desktop → window classes, with
   per-desktop `DESKTOPINFO` holding shell windows + hook chains.**[20]
   Replicate with WuBuOS cap-backed NT Objects (the `wubu_cap` authority core). Desktops
   and window stations are securable objects with create/close syscalls; hooks anchor per
   desktop, not globally. Delta: authorization comes from the cap authority, so a desktop
   is a capability (can achieve "multi-desktop = multi-session" isolation).

5. **Hooks: `NtUserSetWindowsHookEx` + per-hook-type chains `aphkStart[NB_HOOKS]` with
   `co_HOOK_CallHooks` dispatch.**[20][21]
   Expose as `/n/win32/hook/*`: register/unregister/query per desktop, type-indexed
   (`WH_KEYBOARD`, `WH_MOUSE`, `WH_CALLWNDPROC`, ...). Delta: keep the ReactOS lock order
   and list semantics; store hooks in the /n tree for observability instead of only in a
   paged heap.

6. **GDI object discipline: shared references (`ulSharedCount`) vs exclusive locks
   (`cExclusiveLock`), mandatory lock order DCs → Brushes → Pens → Bitmaps, delayed
   deletion until last shared reference drops.**[18]
   This is the corollary of "self-contained C11": GDI is internally *immutable-object +
   refcount* — the exact regime WuBuOS wants for zero-copy surfaces. Delta: fold
   lock-ordering into the cap authority (a multi-lock helper `GDIOBJ_LockMultipleObjects`
   so deadlock-by-lock-order is impossible).

7. **Driver interface split: `NtGdiXxx` (syscall, probed buffers) vs `GreXxx` (internal)
   vs `EngXxx` (exported, for display drivers, no probe).**[18]
   WuBuOS keeps this layering but routes `EngXxx` to `/n/win32/gdi/eng/*` so display
   drivers are control-plane subscribers. Delta: "probe always at the Nt boundary" is a
   hard invariant (matches the existing VSL NT-transliteration discipline).

---

## 3. /n control-plane mapping

Concern | Source (ReactOS) | WuBuOS /n surface
--- | --- | ---
Window station (securable object) | `NtUserCreateWindowStation`, `NtUserCloseWindowStation`[20] | `/n/win32/winsta/*`
Desktop object + `DESKTOPINFO` (shell wnds, hook chains) | `desktop.c`, `DESKTOPINFO`[19][20] | `/n/win32/desktop/*`
Window classes (built-in + registered) | user32/native classes, WM[14][20] | `/n/win32/class/*`
Message queues | Win32 subsystem "message queues"[15] | `/n/win32/queue/*`
Hooks (per desktop, type-indexed chains) | `NtUserSetWindowsHookEx`, `hook.c`[20][21] | `/n/win32/hook/*`
GDI object (DC/brush/pen/bitmap/surface) + locks | design guideline `objects/`, locking[18] | `/n/win32/gdi/obj/*`
GDI painting ops | design guideline `painting/`[18] | `/n/win32/gdi/paint/*`
Display-driver engine interface | `EngXxx`[18] | `/n/win32/gdi/eng/*`
Console / process bookkeeping / shutdown | CSRSS[15][17] | `/n/win32/subsys/*` (+ VSL ALPC)

---

## 4. P-level build spec (go-heavy-first)

- **P1** — Kernel window-manager authority core in ring-0 C11, mirroring `win32ss/conhost-free`
  object model: window station → desktop (`DESKTOPINFO`) → desktop heap range, all as
  `wubu_cap`-backed securable objects; gate with `NtUserCreateWindowStation` /
  `NtUserCloseWindowStation` syscalls in the VSL shadow table.[20]
- **P2** — Window class + message-queue core: built-in classes, per-thread message queues
  with `SendMessage`/`PostMessage`, paste the `aphkStart[NB_HOOKS]` per-desktop hook chains
  and `co_HOOK_CallHooks` dispatch.[15][20][21]
- **P3** — GDI object layer with the ReactOS locking discipline: shared refs
  (`ulSharedCount`) + exclusive locks (`cExclusiveLock`) with fixed order
  DC→brush→pen→bitmap, delayed deletion; `NtGdi*` probed syscall boundary vs `Eng*` driver
  interface.[18]
- **P4** — GDI `painting/` subset first (spans `objects/` + `painting/`): bitblt, fill,
  lineto, textout, rect — enough to draw a real Win98/XP-style shell.[14][18]
- **P5** — CSRSS-equivalent user-mode subsystem server over VSL ALPC: console windows,
  process bookkeeping, shutdown, wired to `/n/win32/subsys/*`, and expose the whole window
  manager under `/n/win32/*` in the same pattern as `/n/nt/registry`.[17]

---

## 5. What is NOT directly sourced (deliberate gaps)

- Exact `NtUserCreateWindowStation` / `NtUserCloseWindowStation` parameter lists are
  referenced from the header but not transcribed here; a build plan must read the header
  inline[20].
- Message-queue internals (specific `Q`/queue struct fields) are asserted conceptually
  from the "message queues" mention in the VEctree page[15], not from a `queue.c` source
  page; a follow-up should pull `win32ss/user/ntuser/message.c`.
- The GDI `EngXxx`/`NtGdiXxx` live driver-export list belongs to `winddi.h` in the source,
  not extracted here.

---

## Sources

[14] https://reactos.org/architecture — ReactOS Architecture — NT kernel & managers (official)
[15] https://vectree.io/c/reactos — VEctree: ReactOS Visual Explainer (Win32 subsystem/CSRSS/win32k)
[16] https://reactos.org/wiki/Win32k.sys — ReactOS Wiki: Win32k.sys
[17] https://reactos.org/wiki/Csrss.exe — ReactOS Wiki: csrss.exe
[18] https://reactos.org/wiki/Win32k_design_guideline — ReactOS Wiki: Win32k design guideline
[19] https://doxygen.reactos.org/d2/dc3/win32ss_2user_2ntuser_2desktop_8c_source.html — ReactOS source: win32ss/user/ntuser/desktop.c
[20] https://doxygen.reactos.org/dd/d79/include_2ntuser_8h_source.html — ReactOS source: win32ss/include/ntuser.h
[21] https://doxygen.reactos.org/d4/d32/ntuser_2hook_8c_source.html — ReactOS source: win32ss/user/ntuser/hook.c
