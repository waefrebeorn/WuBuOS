# Formal / memory-access constant-time tooling probe — VERIFIED_NEGATIVE

**Status: VERIFIED_NEGATIVE. No formal constant-time proof is asserted.**

This leg (`leg/ct-formal-probe`) answers one narrow question with exact,
reproducible evidence: is any formal CT engine, memory-access CT tool, or SMT
solver installed locally with which to machine-check the shipped bignum-free
scalar ladder (`src/gui/wubu_ed25519_fixed.c`)? The answer on this host is
**no**, across every category probed. The gate fails closed: it prints
`FORMAL_CT_TOOL_PROBE VERIFIED_NEGATIVE` and exits 1. It never prints a PASS
it did not measure.

Reference gate: `tools/formal_ct_tool_probe.sh` (make target
`test_formal_ct_tool_probe`). The doc you are reading files the gate's exact
transcript from the run recorded below. Re-run the target to regenerate the
evidence.

## What was probed

| Category | Tools / artifacts | Result |
|---|---|---|
| Formal / memory-access CT binaries | `ct-verif`, `ctgrind`, `valgrind`, `boogie` | MISSING |
| SMT solver binaries | `z3`, `cvc5`, `cvc4`, `yices`, `boolector`, `bitwuzla`, `stp`, `smtinterpol` | MISSING |
| Python SMT bindings | `z3` (z3py), `cvc5` (cvc5py) | MISSING |
| Shared libraries (`ldconfig -p`) | `libz3`, `libboolector`, `libyices`, `libcvc4`, `libcvc5` | MISSING |
| apt cache (`/var/cache/apt/archives`) | matching `.deb` packages | 0 |
| dpkg installed package names | exact-name match | 0 |
| On-disk executables (`/home/wubu`, `/usr/local`, `/opt`) | `z3`, `cvc5`, `cvc4`, `valgrind`, `ct-verif`, `boolector`, `yices` | 0 |

`ctgrind` is a valgrind tool, so it is doubly unavailable: no `ctgrind`
binary and no `valgrind` runtime. No SMT solver exists in any form (binary,
Python binding, shared library, or cached/installed package).

## Non-vacuity / negative control

A detector that always reports MISSING would make this evidence worthless.
The gate proves the *same detection functions* used for the MISSING results
are live and discriminating:

* **Present controls → FOUND:** `gcc`, `python3`, `objdump` (binaries), the
  `sys` Python module, and `libc.so.6` (shared library) must all be reported
  FOUND.
* **Absent sentinels → MISSING:** deliberately impossible names
  (`wubu_formal_ct_sentinel_9f3a7c`, `wubu_no_such_pymod_9f3a7c`,
  `wubu_no_such_lib_9f3a7c`) must be reported MISSING.
* **On-PATH shim flip:** a temporary executable named `ct-verif` is placed on
  `PATH` and the primary detector must flip `MISSING -> FOUND` for it; the
  shim is removed afterwards. This directly demonstrates that if a real
  `ct-verif` were installed on `PATH`, this probe would find it.

Any control failure hard-fails the gate with `FORMAL_CT_TOOL_PROBE FAILED`
(exit 2), so the VERIFIED_NEGATIVE verdict below cannot be produced by a
broken detector.

## Exact transcript (gate run, 2026-11-21T12:23:16Z, exit 1)

```
============================================================
 formal CT tool probe -- 2026-11-21T12:23:16Z
 host: Linux 7.0.0-42-generic x86_64
 gcc:  gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
 py:   Python 3.14.9
============================================================

[1] PATH binaries -- formal / memory-access CT tools
  BIN  ct-verif = MISSING
  BIN  ctgrind = MISSING
  BIN  valgrind = MISSING
  BIN  boogie = MISSING
[2] PATH binaries -- SMT solvers
  BIN  z3 = MISSING
  BIN  cvc5 = MISSING
  BIN  cvc4 = MISSING
  BIN  yices = MISSING
  BIN  boolector = MISSING
  BIN  bitwuzla = MISSING
  BIN  stp = MISSING
  BIN  smtinterpol = MISSING
[3] Python SMT bindings
  PY   z3 = MISSING
  PY   cvc5 = MISSING
[4] shared libraries (ldconfig -p)
  LIB  libz3 = MISSING
  LIB  libboolector = MISSING
  LIB  libyices = MISSING
  LIB  libcvc4 = MISSING
  LIB  libcvc5 = MISSING
[5] ecosystem sweep -- apt cache / dpkg package names / on-disk executables
  apt-cache matching .deb    = 0
  dpkg matching package names = 0
  on-disk CT/SMT executables = 0

[N] NON-VACUITY CONTROLS -- prove the detector is live, not an always-MISSING stub
  present-control  BIN  gcc = FOUND (expected) OK
  present-control  BIN  python3 = FOUND (expected) OK
  present-control  BIN  objdump = FOUND (expected) OK
  present-control  PY   sys = FOUND (expected) OK
  present-control  LIB  libc.so.6 = FOUND (expected) OK
  absent-control   BIN  wubu_formal_ct_sentinel_9f3a7c = MISSING (expected) OK
  absent-control   PY   sentinel = MISSING (expected) OK
  absent-control   LIB  sentinel = MISSING (expected) OK
  shim-control     ct-verif: baseline=MISSING, shim-on-PATH=FOUND (MISSING->FOUND) OK

=== VERDICT ===
FORMAL_CT_TOOL_PROBE VERIFIED_NEGATIVE
Probed categories: PATH binaries, Python bindings, shared libraries, apt cache,
dpkg package names, and on-disk executables. Every formal CT tool, SMT solver,
binding and library is MISSING on this host.
MISSING tools: ct-verif ctgrind valgrind boogie z3 cvc5 cvc4 yices boolector bitwuzla stp smtinterpol z3 cvc5 libz3 libboolector libyices libcvc4 libcvc5
Why no PASS is asserted:
  - no ct-verif / SMT engine exists to discharge a non-interference obligation
    for the fixed 256-iteration scalar ladder;
  - no valgrind / ctgrind exists to run a secret-shadow memory-access check on
    the shipped binary (ctgrind is built on valgrind, which is absent);
  - the existing source / GIMPLE / IR guards and dudect timing statistics are
    structural / statistical EVIDENCE, not a mechanised constant-time proof;
  - no network fetch is permitted, so the absent tools cannot be installed here.
Non-vacuity controls passed: present tools=FOUND, sentinels=MISSING, on-PATH shim=FOUND.
This is exact VERIFIED_NEGATIVE evidence. It does NOT assert a constant-time proof.
```

Gate exit code: 1 (fail-closed VERIFIED_NEGATIVE, as designed).

## Why no PASS is asserted

1. **No formal engine.** There is no `ct-verif`, no Boogie/SMT stack, and no
   SMT solver in any form. A machine-checked non-interference proof for the
   fixed 256-iteration ladder cannot be discharged with what is installed.
2. **No memory-access CT tool.** `ctgrind` requires valgrind; valgrind is
   absent. No secret-shadow memory-access check can be run on the shipped
   binary.
3. **Existing evidence is not a proof.** The source/GIMPLE/IR secret-dependence
   guards and the dudect-style timing statistics are structural and
   statistical evidence (see `docs/ED25519_CONSTANT_TIME_EVIDENCE.md` and
   `docs/ED25519_GIMPLE_CT_CHECK.md`), not a mechanised constant-time proof.
4. **No installation path.** This leg forbids network fetch, so the missing
   tools cannot be installed here. Fabricating a PASS would be dishonest.

This verdict is deliberately distinct from a toolchain probe failure: every
control passed and every category returned MISSING, so this is exact evidence
of absence, not a broken harness.

## Remaining gaps

1. Install a formal CT toolchain locally (e.g. `ct-verif` or an SMT solver with
   a non-interference encoding) **out of band** and re-run this probe; on a
   TOOL_PRESENT verdict, implement and execute a real formal/memory-access CT
   check against `src/gui/wubu_ed25519_fixed.c` with a non-vacuous injected
   regression before any PASS may be asserted.
2. A static non-interference proof would still need a compiler-preservation
   argument to connect source-level CT to emitted machine code across
   toolchains.
3. The shipped fixed ladder's SHA-512 calls and sign/verify orchestration
   (`src/gui/wubu_ed25519_fixed_sign.c`) remain out of scope for a scalar-ladder
   proof and would need separate treatment.
