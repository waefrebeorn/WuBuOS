# The NT stack slerm: a staged plan

The goal, stated plainly: **ape the Windows NT architecture into the templeOS
kernel** so WuBuOS can run Windows software natively, keep Linux and macOS legs,
and expose all of it through the Styx/9P namespace -- with WuBuOS's own naming
and its own standards.

This document is the plan. It records what already exists (a lot), what is
missing, and the order to do it in. Every stage has an acceptance test; none of
it is "port NT" as one task, because that is not a task, it is a decade.

---

## 0. The naming law

> `.exe` is `.app` · `.dll` is `.spell`

WuBuOS does not rename things to be different; it names them to mean something.
A Windows `.exe` is *one application* here: `.app`. A Windows `.dll` is *one
loadable spell* (this tree already calls its loadable units spells --
`src/runtime/wubu_spell.h`, `wubu_spellbook.c`): `.spell`.

Everything else -- folders, paths, windows, menus, shortcuts, the desktop -- keeps
the shape users already know. The user experience is unchanged; only the
vocabulary is honest.

Consequences that must be enforced mechanically:

- a WuBuOS executable image is `.app`;
- a WuBuOS loadable module is `.spell`;
- `.desktop` files remain the freedesktop interop format (they are the bridge,
  not the identity);
- the registry of installed programs maps `.app` <-> start-menu entry.

Owner for enforcement: extend `tools/std_audit.py` with a naming rule once any
`.app`/`.spell` artifact exists to measure. Today there are none, so this is a
forward-looking law, recorded before the first violation appears.

---

## 1. What already exists

Do not rebuild any of this. It is real, split per subsystem, and C11.

**NT syscall transliteration** (`src/runtime/vsl/`) -- 21 modules, each one
subsystem, dispatched through `vsl_nt_<subsystem>_register()` and sharing
`vsl_nt_internal.h`:

```
process  thread  token  job  section  vmem  io  alpc  atoms
registry  sync  timer  partition  ktm  wnf  enclave  ioring
misc  misc_w11  collision_test
```

Syscalls are implemented as `int64_t vsl_nt_<name>(...)` returning `NTSTATUS`,
with `Nt*` behaviour documented at each site (e.g. `vsl_nt_process.c` documents
`NtOpenProcess`). Handles are minted into a table (`vsl_nt_allocate_handle`),
and errno is translated back through `vsl_errno_to_nt_status`.

**PE / executable personalities**:
- `src/runtime/wubu_pe.c` -- PE loader
- `src/runtime/wubu_exec_dos.c` -- DOS .COM/.EXE (real 8086 interpreter)
- `src/runtime/wubu_holyd_exec.c` -- HolyC JIT session
- `src/runtime/wubu_exec_macho.c` -- Mach-O via Darling
- `src/runtime/wubu_exec_format.c`, `wubu_exec_container.c`, `wubu_exec_wasm.c`

**Packaging**: `.desktop` parser and MIME associations
(`src/gui/wubu_mime_desktop.c`, `parse_desktop_file`), the spellbook
(`wubu_spellbook.c`), and the package engine (`pkgmgr_engine`, `pkgmgr_install`,
`pkgmgr_add_remote`).

**Kernel plumbing**: cap-backed authority (`wubu_cap`), the EDR ring
(`EDR_EV_BOOT_ATTEST = 27`), boot attestation, Styx/9P namespace
(`hosted_styxfs.c` + `guest_file.h`), and the `install-wubuos.sh` installer.

---

## 2. Coverage matrix

| NT area | State | Owner |
|---|---|---|
| Process / thread lifecycle | implemented | `vsl_nt_process.c`, `vsl_nt_thread.c` |
| Tokens & access checks | implemented | `vsl_nt_token.c` |
| Jobs, sections, virtual memory | implemented | `vsl_nt_job.c`, `_section.c`, `_vmem.c` |
| Object handles & manager | implemented | `vsl_nt_internal.h`, `_partition.c` |
| Registry | implemented | `vsl_nt_registry.c` |
| ALPC / IPC | implemented | `vsl_nt_alpc.c` |
| Synchronization (events, mutant, semaphore) | implemented | `vsl_nt_sync.c` |
| Timers, WNF, atoms | implemented | `vsl_nt_timer.c`, `_wnf.c`, `_atoms.c` |
| KTM / transactions | implemented | `vsl_nt_ktm.c` |
| I/O manager, IRPs | implemented | `vsl_nt_io.c` |
| Enclave, IoRing | implemented | `vsl_nt_enclave.c`, `_ioring.c` |
| Win11-specific behaviour | partial | `vsl_nt_misc_w11.c` |
| **Win32 subsystem (kernelserver / GDI / USER)** | **not built** | -- |
| **CSRSS / session manager integration** | **not built** | -- |
| **Structured exception handling (SEH) exposed to apps** | **partial** | -- |
| **PE exports/imports resolved against `.spell`** | **not built** | -- |
| **DirectX / D3D12 on Vulkan** | spells exist | `src/spells/graphics/render/vkd3d/` |

---

## 3. The gaps, ranked by what they unlock

### Gap A -- no Win32 personality (blocks every real Windows app)

An NT syscall layer alone does not run Notepad.exe. A Windows binary calls
`kernel32!CreateFileW`, which calls `ntdll!NtCreateFile`, which dispatches into
this layer. The middle layer -- the Win32 API surface, including Unicode
conversions, error mapping, heap, and the CRT -- does not exist.

Work: a `win32.personality` over the existing NT layer, using the same pattern
as `wubu_exec_dos.c` (translate the API, dispatch to NT). Start with the ~40
calls a minimal GUI program needs; do not attempt the whole Win32 surface.
Acceptance: a real small C program compiled against our headers links and runs,
creating a file, opening a window, and exiting.

### Gap B -- `.app` / `.spell` loader (blocks packaging)

Today the loader knows PE, DOS, HolyC, Mach-O and WASM. It has no notion of a
WuBuOS-native `.app` bundle or a `.spell` module, so there is nothing for the
package manager to install and nothing for the desktop to launch by name.

Work: define the formats, then wire them into the existing loader switch rather
than writing a parallel one.
Acceptance: `wm_write_desktop_shortcut`-style flow produces an `.app` whose
icon launches it; a `.spell` can be loaded and provides at least one callable
export.

### Gap C -- SEH (blocks most real-world binaries)

Structured exception handling is mandatory for MSVC-compiled code -- even
programs that never raise an exception emit unwind metadata. Partial support
exists but nothing is exposed to applications.

Acceptance: a binary that installs a `__try/__except` frame and triggers a page
fault inside it resumes at the handler instead of dying.

### Gap D -- Win32k / display integration (blocks "looks like Windows")

`src/spells/graphics/render/vkd3d` gives D3D12-on-Vulkan, but the window system
is still the WuBu WM. For a Windows app to appear correctly, the DWM-equivalent
surface model and composition need a Win32-compatible front end.

Acceptance: a D3D12 sample renders into a desktop window alongside native apps.

### Gap E -- Linux and macOS legs (keeps the other half of the claim)

Linux: already real via `wubu_exec.c` host delegation and containers.
Mach-O: `wubu_exec_macho.c` via Darling. What is missing is the same packaging
honesty as Gap B -- an `.app` that may contain any of these personalities, with
the personality chosen by content rather than by the user.

---

## 4. Order of work

Each stage lands on its own gates. Nothing below depends on a future stage.

**Stage 1 -- Extend the NT contract gate (small, and the base everything rests on).**
The layer is already tested far more than the architecture talk assumes:
`vsl_syscall_nt_test.c` carries 280 checks, `vsl_syscall_nt_ext_test.c` another
17, and `vsl_nt_collision_test.c` asserts that no two modules claim the same
dispatch slot in the shared 512-entry table -- a real failure mode, since a
collision silently let registration order decide who won.

What does NOT exist is an argument-contract gate: enumerate every registered
syscall and assert each returns a valid NTSTATUS for NULL/garbage arguments
instead of crashing. Half of transliteration quality is that bad arguments are
handled politely, and right now only individual syscalls are checked that way.
Acceptance: `make test_vsl_nt_contract`, and its syscall count rises
monotonically as new NT calls land.

**Stage 2 -- `.spell` before `.app`.**
A module format is simpler than a program format and it is what a linker needs
anyway. Define `.spell` (exports, dependencies, load address), implement the
loader, and let the spellbook consume it. Existing spells
(`src/spells/**`) become `.spell` packages without changing their contents.

**Stage 3 -- Minimal Win32 personality (Gap A), smallest useful slice.**
Unicode string conversion, `GetLastError`/`SetLastError`, the CRT heap, and the
file/thread/process subset that a toy GUI program needs. Built *over* the NT
layer, never beside it.

**Stage 4 -- `.app` bundles (Gap B).** A container that names a personality +
an entry point + resources, with the desktop icon binding from Stage 2's
shortcut flow.

**Stage 5 -- SEH (Gap C).**

**Stage 6 -- Win32k front end (Gap D).**

Stages 2-6 each follow the same workflow, from `docs/STANDARDS_AND_WORKFLOW.md`:
measure -> pick one offender -> implement -> gate -> re-measure -> commit ->
push. A stage that cannot be measured is not done.

---

## 5. How this meets the house standards

- **One owner per concern.** Each NT subsystem already owns one file; Win32
  gets `runtime/win32_*.c`, one per API family, dispatching into NT. No file
  becomes "the Windows stuff".
- **Reuse, never copy.** Win32 translates to NT -- it does not reimplement
  `NtCreateFile`. `.app` resolution goes through the existing loader switch in
  `wubu_exec_format.c`. Spell loading goes through `wubu_spellbook.c`.
- **Fail closed.** Every NT call returns an NTSTATUS; invalid arguments return
  `NT_STATUS_INVALID_PARAMETER` and leave output pointers untouched -- the
  pattern already present in `vsl_nt_open_process`.
- **Behaviour gates.** Stage 1's contract test and each later stage's "does a
  real binary run" acceptance are tests that can fail.
- **Measured by the auditor.** Once `.app`/`.spell` artifacts exist,
  `tools/std_audit.py` grows naming rules so the vocabulary stays honest.

---

## 6. Honest status

The NT *syscall* layer is further along than the architecture conversation
usually assumes: 21 subsystems, split cleanly, C11, documented at each site,
with 280 syscall checks and a dispatch-collision contract already in place.
What is missing is everything above it -- the API personality, the exception
model, and the packaging vocabulary -- plus the display integration.

That ordering is deliberate. Building Win32 before the argument contract would
mean writing an API on top of syscall stubs nobody had checked.
