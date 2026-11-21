# Compiled-tree secret-independence check for the Ed25519 ladder

This is the next increment after the opt-in LLVM-IR checker
(`tools/eddsa_ir_ct_check.py`, `tools/eddsa_ir_ct_gate.sh`).  That checker
requires `clang -emit-llvm`; on a host without clang it fails closed and can
only run over **hand-authored** `.ll` fixtures
(`tests/fixtures/ir_ct/ladder_clean.ll` and friends).  This increment uses the
compiler that is actually present on this host: **GCC**, via its
`-fdump-tree-optimized` GIMPLE dump.

## What is machine-checked

`make test_ed25519_gimple_ct` (`tools/eddsa_gimple_ct_gate.sh`):

1. discovers a **local** gcc that really supports `-O2 -fdump-tree-optimized`
   (a compile probe that must emit GIMPLE with SSA PHI nodes);
2. compiles the **shipped** `src/gui/wubu_pkg_ed25519.c` at `-O2` and captures
   the real `-fdump-tree-optimized` GIMPLE of `ge_mul_ladder` — compiler
   output, not a model;
3. runs `tools/eddsa_gimple_ct_check.py`, which taints the scalar bit returned
   by `mbedtls_mpi_get_bit`, propagates it through SSA defs / casts / xor /
   PHI / select to a fixpoint, and fails on any secret-derived **conditional
   branch, switch, or memory index** in the compiled ladder.

The machine-checkable claim printed on success is:

> In gcc -O2 GIMPLE of `@ge_mul_ladder`, no `mbedtls_mpi_get_bit`-derived SSA
> value controls a branch, switch or memory index.

Also emitted as evidence: the loop PHI seeded with literal `256`
(`WUBU_ED25519_LADDER_BITS`), i.e. the constant trip count surviving `-O2`.

## Trust boundary (why this is a check, not a full proof)

The taint analysis is deliberately **intraprocedural over `ge_mul_ladder`**:

* the only taint source is the secret scalar-bit call;
* SSA defs, casts, `xor`, PHI and select propagate taint monotonically;
* the **result of every other call** (`ge_cswap`, `ge_add`, `ge_copy`,
  `ge_grow`, error codes, ...) is modeled as **public**.  This is the callee
  contract: constant-timeness of the selection primitive is a separate
  obligation, bounded at source by the C1 check in
  `tools/eddsa_ladder_guard.py` and by mbedTLS's documented `safe_cond_swap`
  assembly.  Without this boundary a legitimate `cswap(..., swap)` would
  taint its own return code and raise a false leak on every `if (rc != 0)`
  error check.

Consequently this does **not** replace a full interprocedural / assembly proof;
it closes the specific "could `-O2` reintroduce a data-dependent branch in the
ladder itself?" gap for this compiler and these flags.

## Fail closed, never a fake PASS

No provenance marker, a dump that is not recognisably GCC GIMPLE (no
`funcdef_no=`, no PHI), a missing `@ge_mul_ladder`, no taint source, a failed
compile, a missing mbedTLS dev root, or no capable gcc at all: all FAIL with
exit 1.  `opt`/clang/z3/network are not required and their absence is never
pretended to be evidence.  The mbedTLS dev root is resolved offline from
`WUBU_MBEDTLS_ROOT` or, in a linked worktree, from the same repo's local cache
via `--git-common-dir`; it is never fetched.

## Non-vacuity / negative control

`make test_ed25519_gimple_ct_negctl` (also run first by the gate) injects the
real source regression

```c
if (swap) TRY(ge_cswap(&r0, &r1, 1));   /* INJECTED CT REGRESSION */
```

into the working tree, recompiles at `-O2`, and requires the checker to flag a
secret-dependent branch in the emitted GIMPLE (`if (bit_37 != swap_76)` in the
recorded gcc 13.3.0 run).  It then restores the source and requires the shipped
ladder green again.  If no capable gcc exists, no compiled PASS is asserted:
the source guard is still required to catch the regression and a **verified
negative** is recorded.
