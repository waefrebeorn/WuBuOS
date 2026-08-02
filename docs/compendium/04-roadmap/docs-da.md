# Triple-DA — Documentation Generators, Scanners, and the Docs Philosophy

*DA performed 2026-08-02 (the "solo overhaul" session). Each finding is
verified against the repo. Output: the fixes implemented in this batch.*

## DA-1 — Generator correctness (tools/gen_docs.py)

1. **REAL — Coverage is 23%.** SRC_DIRS lists 4 dirs
   (kernel/firmware/apps/hosted); the tree has 16. Generated modules.md =
   135 modules; the tree has **582 .c files**. The gui, compiler, jit,
   runtime, framework, bridge, audio, bear, init, shell, tools, worldsim
   modules are undocumented. *Fix: recursive walk of all of src/.*
2. **REAL — "Public API" heuristic is a guess.** It pattern-matches
   top-level function definitions; it misses prototypes-only APIs and
   mis-identifies macros. The real contracts live in the HEADERS. *Fix:
   scan *.h for prototypes as the API source of truth.*
3. **REAL — "Purpose" extraction reads only the first comment block** —
   usually the license/header banner, not the module's actual job. *Fix:
   prefer a dedicated purpose comment, then the file banner, then a
   filename-derived guess.*
4. **REAL — symbols.md only reads kernel.elf** — the hosted binaries
   (runtime, gui, wubufx) are absent. *Fix: scan all built ELFs that exist.*
5. **CLEAN — The generated-banner discipline is right.** GENERATED-FILE
   headers + `make docs` + never-edit-by-hand is the correct mechanism.
   Keep it.

## DA-2 — The scanners (what "new generation" means)

1. **REAL — tests.md is a static target list, not a scanner.** It should
   RUN the host test targets and record PASS/FAIL + a timestamp — a real
   diagnostic scanner, not a table of names.
2. **REAL — No AGI-OS state is captured.** The generated docs don't reflect
   the LIVE system (tick, promoted_total, attestation, /theme writes) even
   though the boot probe is trivially scriptable. *Fix: a `state.md`
   generator that boots QEMU, reads the console probes, records them.*
3. **REAL — No parity scanner.** The "compiled binaries usable on other
   OSes" (PARITY project) claim has no generated evidence: which hosted
   legs exist (x11/evdev/drm/vulkan), what the portable core is, what
   builds where. *Fix: a `parity.md` generator that inspects the hosted
   legs + build flags and reports the portability matrix.*
4. **REAL — The philosophy docs predate the AGI-OS integration.** The
   verifier (promote loop live), the /theme namespace, the unified HID,
   and the Colonel-space framing are absent from the docs philosophy.
   *Fix: update the README + add the documentation philosophy entry.*

## DA-3 — The documentation philosophy (the way forward)

1. **The docs are the AGI's institutional memory, not a developer
   appendix.** The prestige ledger + generated reference + addendum
   philosophy ARE the recursive-learning substrate. The philosophy must
   say this explicitly.
2. **Generated docs must be RUN, not listed.** Every generated file should
   be the output of a scanner that executes something (tests, boot probes,
   build inspection) — the "new generation" means the docs prove the
   system's state at generation time.
3. **The AGI-OS integration is the organizing principle.** The compendium's
   sections map to the four levels (Colonel space / user space / overlays /
   anti-cheat); the generated reference feeds the AGI's context.
4. **PARITY is a first-class doc citizen.** The hosted layer is the
   scaffold that must run on Linux/Windows/macOS with parity; the docs
   track the portability matrix as generated evidence.

## Implemented in this batch

- gen_docs.py rewritten: recursive src walk (all 16 dirs), header-sourced
  API, better purpose extraction, all-ELF symbols.
- New scanners: `tests` (RUNS the host tests, records PASS/FAIL + time),
  `state` (boots QEMU, records the live AGI-OS probes), `parity`
  (inspects the hosted legs + portability matrix).
- Philosophy updated: README + `00-philosophy/documentation.md`.
