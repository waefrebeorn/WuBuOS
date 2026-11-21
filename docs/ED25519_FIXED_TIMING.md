# Fixed-path EdDSA same-run timing gate (dudect-style, non-formal)

**Status: EMPIRICAL STATISTICAL EVIDENCE — NOT A FORMAL CONSTANT-TIME PROOF.**

This document describes `make test_ed25519_fixed_timing` and its standalone
negative control `make test_ed25519_fixed_timing_negctl`.  Every number the
gate prints comes from a real compile and run on this host; re-run the target
to regenerate it.

## What is measured

The product ships the bignum-free fixed-width ladder in
`src/gui/wubu_ed25519_fixed.c`, selected by `WUBU_EDDSA_FIXED_LADDER` in
`mk/vars.mk`.  The older `make test_ed25519_dudect` gate times the mbedTLS MPI
ladder in `src/gui/wubu_pkg_ed25519.c`, which is **not** the shipped default
signing backend.  This gate closes that coverage gap: it times the code that
actually ships.

`tools/ed25519_fixed_timing_probe.c` includes the real
`src/gui/wubu_ed25519_fixed.c` directly, so it calls the exact
`wubu_ed25519_fixed_base_scalarmult()` used by the signer.  No mbedTLS bignum
is needed for the measurement, and nothing is fetched from the network.

## Method

One process, one clock (`CLOCK_PROCESS_CPUTIME_ID`), one scalar set.  Two
secret classes have a **fixed 256-bit length** and differ only in Hamming
weight (`pc=8` vs `pc=255`):

1. **Shipped path** — `wubu_ed25519_fixed_base_scalarmult()`, the 256-iteration
   mask-only ladder.
2. **Intentionally variable control** — an in-probe left-to-right
   double-and-add, `if (bit) ge_add(...)`, using the *same* field/point
   primitives from the included shipped leaf.  Its work tracks popcount.

Samples of both paths and both classes are collected in the same run, shuffled
per sample so monotonic clock drift cannot manufacture a Hamming-weight
signal.  A single pass computes Welch's t for each path.  Thresholds are fixed
with **no retries**:

| Check | Criterion | Meaning |
|---|---|---|
| Shipped ladder | `|t_shipped| <= 10` | no significant class-dependent leak |
| Variable control | `|t_control| > 10` | detector is non-vacuous: a real leak is flagged |

A functional-equivalence check first proves the control computes the same
group element as the shipped ladder (`equiv_mismatches=0/24`), so it is a fair
baseline rather than an unrelated slow path.  If the control is *not* flagged,
the gate fails — a detector that cannot see leakage is treated as broken.

## Fail-closed behaviour

`tools/ed25519_fixed_timing_gate.sh` and
`tools/ed25519_fixed_timing_negctl.sh` exit non-zero when `cc` is missing, when
a required source is missing, or when the probe fails to build.  They never
print a SKIP and never emit a manufactured PASS; an absent harness is a failed
gate, not a waived one.  This differs deliberately from the older
`tools/eddsa_dudect_gate.sh`, which SKIPs when the mbedTLS dev root is absent.

## Why this is NOT a formal proof

1. Welch's t-test bounds an observable statistical correlation on this host;
   absence of a detected signal is not proof of its absence.
2. One toolchain and one compiler optimisation level (`cc -O1`) are used.  A
   second toolchain, and what other compilers emit, are not covered.
3. Process CPU time is the clock.  microarchitectural / instruction-level
   channels (cache, branch predictor, speculative execution) are not modeled
   by this test.
4. The deliberately secret-dependent control is a synthetic probe path, not a
   regression injected into the shipped source; it validates the detector and
   sampling harness, not a formal constant-time property of the product.
5. SHA-512 (the other mbedTLS primitive used by signing) and the surrounding
   sign/verify orchestration in `wubu_ed25519_fixed_sign.c` are out of scope
   here; this gate measures the bignum-free scalar-multiply leaf only.

For the structural/bignum evidence that complements this gate, see
`docs/ED25519_FIXED_FULL.md` and `docs/ED25519_CONSTANT_TIME_EVIDENCE.md`.
