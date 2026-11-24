# The orphan audit — 2026-11-24

**Question:** how many tests do we own that nothing runs, and how many of those
still work?

`tools/orphan_audit.py` reads `mk/tests.mk`, joins make continuations, walks the
aggregate graph from `make test`, and builds+runs every target that graph does
not reach, with a timeout. Logs: `build/logs/orphan/<target>.log`; TSV:
`build/orphan-audit.tsv`.

## Result on this tree

- 662 `test_*` targets exist; **413 are reachable from `make test`; 249 are not.**
- Of the ones audited: **166 PASS**, 16 FAIL, 14 BUILD, 5 TIMEOUT (the CT/timing
  gates need minutes, by design).
- 68 of those passing targets were missing from the existing `test_orphans`
  tier and have been added to it, so the green set now actually runs.

## CORRECTION: the first measurement was wrong

The first pass claimed "202 unreachable, 166 of them passing — free coverage
nobody was collecting". That was a **parser bug**, not a discovery: the detector
read only the *first line* of an aggregate, and `test_orphans` spans a dozen
continuation lines. With continuations joined and the graph walked from `test`,
the count went **up** (318 unreachable at the time) and the claim went **down**:
most of those 166 were already in `test_orphans`, already running.

Two lessons, both now in the tool:
1. Join `\`-continuations before parsing a Makefile. A one-line regex over a
   multi-line list is a measurement you cannot trust.
2. Before creating a tier for a gap, check the tier does not already exist. A
   duplicate tier built on a phantom gap is the double-coding the tree forbids —
   and it passes every test, which is worse.

## What the failures were

**The big one — a security regression was being shipped.** Four Ed25519
constant-time gates failed identically: `expected exactly one ladder cswap
anchor, found 0`. The cause was in `src/gui/wubu_pkg_ed25519.c`, not in the
gates: a killed negative-control run had left its *injected* regression in the
source —

```c
if (swap) TRY(ge_cswap(&r0, &r1, 1));  /* INJECTED CT REGRESSION */
```

— a branch on a secret bit inside the scalar-multiplication ladder. Restored to
the branchless `TRY(ge_cswap(&r0, &r1, swap));`; `test_ed25519_gimple_ct` and
its negative control now pass and prove the property again. Prevention is in
place at two levels: every injecting script refuses to start on a dirty source,
and `test_no_ct_residue` (in `test_critical_runtime`) fails the suite if the
marker exists anywhere in `src/` or `include/`.

`test_ed25519_ir_ct` still fails **closed**: there is no clang with
`-emit-llvm` on this host, so it refuses to assert a pass it cannot measure.

**Dead recipes.** Five named test sources that do not exist
(`test_bpm`, `test_bpm_games`, `test_wm_tray_world`, `test_zip_extract`,
`test_cab_extract` — the last built a CLI it never ran, from an expansion that
had emptied out) and were removed with a note saying what a real test would
assert; `test_cab` already runs the real CAB/LZX self-test. Three more
(`test_cp_hardware`, `test_cp_dnt`, `test_controlpanel`) were replaced by one
`test_cp_applets`.

**Wrong prerequisites.** `test_secmon` could not run at all — it asked for
`src/kernel/wubu_secmon.h`, which lives in `src/runtime`. Fixed, runs, all pass.

**Missing link lines, fixed.** `test_vsl_nt_ext` (16 passed: needed
`wubu_modsym.c`), `test_screenshot_clipboard` (`wubu_math.c`), `test_session_mgr`
(13 passed: needed `wubu_ns_effect.c` and `wubu_effect.c`), and five compiler
recipes that were missing the MIR passes which exist as TUs
(`wubu_mir_fuse/gvn/sccp/interp`).

## What is still broken, and why it is not a one-liner

The compiler gates (`test_isa_driver`, `test_mir_opt`, `test_album`,
`test_drivers`, `test_gauntlet`) reference symbols that **no translation unit in
the tree defines**:

- `x86_decode_all` — used by `src/compiler/test_isa_driver.c`
- `wubu_sf_f32_add` (and the rest of softfloat) — used by the 8051/AVR/RISC-V/MIPS interpreters
- `wubu_tensor_dispatch`, `wubu_tgemm_f32_mir` — used by `wubu_isa_x86_64.c`
- `wubu_isa_wasm` — the router named a WebAssembly backend that has no TU

The wasm case is now honest: `wubu_isa_driver.c` declares it only under
`WUBU_HAVE_ISA_WASM` and returns NULL otherwise, instead of failing the link of
every binary that includes the router. The other three need real
implementations (a softfloat library, an x86 full-decode path, a tensor
dispatch shim) — that is a feature, not a link fix, and it is the next honest
piece of work on the compiler leg.

## The rule this establishes

A test that no aggregate reaches is a claim nobody checks: it can rot for
months, and the *silence* is the defect. Two consequences, both cheap:

1. New tests go in a tier the same day they are written.
2. `tools/orphan_audit.py` is run after any batch that touches `mk/tests.mk`,
   so the orphan count and its verdicts are a number we can watch, not a guess.
