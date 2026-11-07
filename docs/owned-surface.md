# WuBuOS Owned & Tested Surface — DERIVED FROM THE MAKEFILE (not prose)

Snapshot: `make test-module` is the authoritative "owned & tested" gate. Generated
2026-11-06 by scanning `mk/tests.mk`. Rule: a module is OWNED only if it appears
in a named test target that passes; everything else is UNTESTED surface (the DA#2
junk-drawer debt), regardless of how much code "exists."

## THE CORE GATE — `make test-module` (11 targets, must always be GREEN)

The loader/PE → emitter → registry → namespace → 9P → self-host continuum:

| target | what it proves |
|---|---|
| test_holyd_pe      | holyd blob wraps as PE .text, runs at two bases -> 42 |
| test_pe_globals    | module global store lands in mapped .data -> 40 |
| test_pe_program    | emitter program (module+funcs+call), DETERMINISM gate -> 42, byte-identical |
| test_pe_import     | holyd emits an import-carrying PE -> resolves |
| test_wubu_zxload   | load a PE by name from /n (stdio) |
| test_wubu_zxreg    | registry: dedup by SHA256, refcount, unload/compact |
| test_wubu_applaunch| session starts an app by loading its PE from /n (registry) |
| test_wubu_selfbuild| FOUR self-hosted programs (data-array/string/recursion/computed-index) |
| test_wubu_zx9p     | loader consumes a module over REAL Styx/9P (chunky read too) |
| test_w9pc          | KERNEL-CLEAN 9P client: freestanding compile + drives real server |
| test_wubu_tokenizer| SELF-HOSTED compiler stage (tokenizer) runs through the loader |

These feed the honest number: they prove a self-hosted, /n-and-9P-capable
continuum end to end; the gate failing ANY = the OS-app-launch story is RED.

## The WIDER VERIFIED PE/styx surface (named targets, same file tree)

`test_pe_reloc/exec/iat/full/load/personality`, `test_wubu_zx`, `test_styx`,
`test_styxfs`, `test_ns_9p` also pass individually but are NOT members of the
aggregate gate — they can rot without `make test-module` noticing. Consider promoting
the PE-contract + 9P-server ones into the gate if they must stay honored.

## The REPO'S TOTAL TESTABLE SURFACE

- **445** `test_*` targets exist in `mk/tests.mk`. (Most are runnable driver
  selftests; 11 are the module gate above.)
- **~450** `wubu_*.c` files live in `src/kernel/` — a large fraction have NO test
  target (NO_OWNED), i.e. unclassified vendor/LKDDM orphans.

## Classification rule (C1/C2)
A `wubu_*.c` is OWNED iff `grep -l <base> mk/tests.mk` finds a passing target that
links it. To promote a module to OWNED-and-gated: add its target to `test-module`.
To demote/debt: move it to `src/kernel/vendor/` OR add a test. No third state: a
kernel module with no passing test is unowned, whatever its line count.

> Update this manifest whenever the aggregate gate membership changes; the gate is
> the single source of truth.
