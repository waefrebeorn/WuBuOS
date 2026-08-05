# AGENTS.md — wubuos (THE BODY)

> Agent context file. Read this before working in this repo. Kept current;
> update it when the structure changes. Humans: this is your onboarding too.

## What this repo is

wubuos is the **Body** half of the WuBu project — everything that acts:
the ZealOS/Win98-style kernel, the dosgui window manager, the HolyC
compiler, the VSL (Virtual System Layer) syscall personalities, the
Styx/9P namespace, container isolation, BearRL, and the firmware that
boots it. The **Brain** half is `wubuwizard`. One AGI, two repos.

**One sentence:** The Brain learns; the Body protects and acts. The Live
Colonel (ring-0 REPL) is where the Body hosts the Brain.

## Architecture in one paragraph

- **Hosted binary:** the whole OS runs as a single hosted C11 binary on
  Linux (the "Inferno emu" pattern) — the kernel runs in-process. A
  separate metal path boots on real hardware via WuBuFW (own UEFI
  firmware).
- **Namespace:** Styx/9P is the system bus — everything is a file,
  services register under `/svc/`, the WM and apps communicate through the
  namespace.
- **GUI:** dosgui WM with centralized chrome (`dosgui_chrome_draw_window`)
  — apps draw ONLY within the chrome-provided content rect, never their
  own title bars. Theme engine is runtime-switchable (Win98/XP).
- **Personalities:** VSL presents multiple OS syscall ABIs (Linux, Win32
  NT subset, Plan 9, Classic Mac OS 68K) translated onto one kernel core.
- **Compiler:** HolyC compiler ("My Seed") in-tree; JIT stack (x86-64
  encoder, disassembler) self-hosted.

## Directory map

| Path | What lives there |
|---|---|
| `src/kernel/` | Kernel core: scheduler, IPC, memory, AHCI, FAT32 |
| `src/gui/` | dosgui WM, chrome, theme engine |
| `src/apps/` | GUI apps (fm, calc, cmd, explorer, canvas, bonzi, ...) |
| `src/compiler/` | HolyC compiler + JIT |
| `src/bridge/` | NT-syscall bridge + VSL personalities |
| `src/framework/` | wubufx — the Styx9-namespaced app framework |
| `src/hosted/` | The hosted binary entry (`wubu`) |
| `src/firmware/` | WuBuFW — own UEFI firmware (no EDK2/OVMF) |
| `src/runtime/` | Runtime, container isolation (cgroups v2 + seccomp-bpf) |
| `src/shell/` | The shell (cmd-style terminal) |
| `src/bear/`, `src/worldsim/` | BearRL physics + world simulation |
| `docs/` | TOPOLOGY.md (master map), BUILDING.md, MODULES.md, compendium |

## Build & test

```bash
make all          # the full build (hosted binary + all subsystems)
make test         # the test gate — run this before claiming anything works
make hosted       # just ./src/hosted/wubu
```

Requires: C11 toolchain, `make`. Optional: `nvcc`, Vulkan SDK, `python3`.
The `src/runtime/container/wubucontainer` tree is a git submodule —
`git submodule update --init --recursive` after cloning.

## The non-negotiables (do NOT violate)

1. **No stubs.** Every called function does real work. Compile-time `#else`
   for unavailable hardware is the only exception.
2. **Centralized chrome.** GUI apps must use
   `dosgui_chrome_draw_window()` for the window frame/title bar/buttons.
   Apps draw ONLY within the chrome-provided content rect. Never compute
   content offsets from `win->x` + `title_bar_height()` manually — the
   legacy helpers must stay in sync with the chrome module.
3. **Theme-driven colors.** Use `wubu_theme_colors()` tokens, never raw
   hardcoded hex, in GUI code.
4. **No third party if we can write it** — self-contained C11 is the point.
5. **C11 strictness:** opaque structs at module seams, minimal includes,
   single-exit error handling.
6. **Verify before claiming.** Tests ≠ correct: read the code, run the
   target, read the FAIL lines.
7. **Personalities are translation layers** — the kernel core implements
   one set of primitives; each VSL personality maps its ABI onto them.

## How to work here (agent workflow)

1. Read `docs/TOPOLOGY.md` + `docs/MONOLITH_DISSOLUTION.md` first — know
   the boundaries before touching code.
2. For a new feature: research (7-hop) → design (ADR if architectural) →
   implement (one module, one test) → verify (`make test`) → commit.
3. After refactors: re-verify with FRESH tool calls (clean rebuild, full
   `make test`) before reporting done. Triple Devil's-Advocate.
4. When splitting a monolith: Strangler Fig — new module behind a clean
   interface, route calls through it, never big-bang rewrite.

## Architecture decision records

Architectural decisions are recorded in `docs/adr/` (Nygard template,
append-only). Read them before changing anything structural. Write one when
you make a structural decision.
