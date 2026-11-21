# Ed25519 ladder compiler-IR constant-time gate

**Status: compiler-level structural check, NOT a formal constant-time proof.**

`make test_ed25519_ir_ct` (`tools/eddsa_ir_ct_gate.sh`) is the compiler-IR
increment after the root-independent source guard
`make test_ed25519_ladder_guard` (`tools/eddsa_ladder_guard.sh`).  The source
guard bounds the C text; this gate bounds what the compiler emits.

## What it does

1. Runs `tools/eddsa_ir_ct_negative_control.sh`: an analyzer self-test over
   hand-authored `-O2`-shape IR fixtures plus the injected `if (swap)`
   regression in the real source.  The checker must flag a secret branch and a
   secret GEP index, must accept the clean fixture, and must reject a "vacuous"
   verdict.  This part needs no compiler.
2. Discovers a **local** clang capable of `-emit-llvm -O2` through
   `tools/eddsa_ir_toolchain.sh`.  No fetch, no install, no guessed paths.
3. Compiles `src/gui/wubu_pkg_ed25519.c` to LLVM IR at `-O2` (mbedTLS include
   root from `tools/mbedtls_devroot.sh`) and prepends a provenance marker
   asserting `-O2`.  `opt`, when present, only runs `-verify`.
4. Runs `tools/eddsa_ir_ct_check.py` on `@ge_mul_ladder`: taints the scalar bit
   from `mbedtls_mpi_get_bit`, propagates through SSA data flow to a fixpoint,
   and fails on a secret-conditioned `br`/`switch` or a secret-derived
   `getelementptr` index/pointer.  A secret `select` is permitted at IR level
   (it is the constant-time data-selection primitive; its back-end lowering
   remains a separate concern).

## Fail-closed contract

No clang, no mbedTLS dev root, a failed compile, IR with no `-O2` marker, a
missing `@ge_mul_ladder`, or no discoverable taint source are all **FAIL**
(exit 1) with `IR_CT_GATE FAILED`.  The gate never prints PASS without a real
`-O2` compile and a real analysis.  It is deliberately **not** in the default
tier: on a host without clang it goes red by design, while the always-on
structural guard (`test_ed25519_ladder_guard`) remains in `test_medium_other`.

`make test_ed25519_ir_ct_negctl` runs only the compiler-independent negative
control and can pass on a toolchain-less host; that is a detector sanity test,
not a claim that the ladder IR was verified on that host.

## Negative control

The control injects the exact source regression used by
`tools/default_crypto_negative_control.sh`:

```
-        TRY(ge_cswap(&r0, &r1, swap));
+        if (swap) TRY(ge_cswap(&r0, &r1, 1));
```

It requires the source guard to go red on the regressed source, requires the
IR checker to detect the secret `br` in `tests/fixtures/ir_ct/ladder_leak.ll`
and the secret GEP index in `tests/fixtures/ir_ct/ladder_index.ll`, restores
the source through an `EXIT` trap, and requires the source guard green again.
With a local clang it additionally compiles the regressed source at `-O2` and
requires the checker to flag it; without clang that sub-step is reported as
`NEGCTL_SKIP` and the main gate fails closed.

## Remaining gaps

* The `.ll` fixtures are hand-authored IR-shape models, not compiler output;
  they prove the checker is discriminating, not that a specific clang emits
  the modelled code.
* On hosts without clang, no real `-O2` IR of the shipped ladder is produced
  or analysed at all -- the gate fails closed rather than asserting PASS.
* No proof about mbedTLS's hand-written `mbedtls_mpi_safe_cond_swap` assembly,
  nor about back-end lowering of IR `select` to branches/cmov.
* If `-O2` inlines `@ge_mul_ladder` away, the checker cannot find it and the
  gate fails closed; a dedicated `noinline` harness would be needed for that
  toolchain.
