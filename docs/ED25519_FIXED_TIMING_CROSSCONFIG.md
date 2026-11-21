# Cross-configuration fixed-EdDSA timing gate (dudect-style, non-formal)

**Status: EMPIRICAL STATISTICAL EVIDENCE — NOT A FORMAL CONSTANT-TIME PROOF.**

This document describes `make test_ed25519_fixed_timing_crossconfig` and its
standalone negative control `make test_ed25519_fixed_timing_crossconfig_negctl`,
driven by `tools/ed25519_fixed_timing_crossconfig.sh`.  It closes the
"one toolchain, one optimisation level" limitation recorded in
`docs/ED25519_FIXED_TIMING.md`.

## What it does — reuse, not a second gate

This increment does **not** reimplement the timing harness.  For every usable
compiler configuration it invokes the existing, unchanged gates:

* `tools/ed25519_fixed_timing_gate.sh` (shipped ladder flat **and** control
  flagged), and
* `tools/ed25519_fixed_timing_negctl.sh` (detector flags the secret-dependent
  control).

The only interface added to those two scripts is an optional environment
override — `CC` and `EDDSA_TIMING_OPT` — which defaults to the original
`cc -O1`, so `make test_ed25519_fixed_timing` is unchanged.  The cross-config
driver passes a different compiler/optimum to each run.  Each run still builds
the same self-contained probe (`tools/ed25519_fixed_timing_probe.c`, which
includes the shipped leaf `src/gui/wubu_ed25519_fixed.c`), performs ONE
interleaved same-run Welch-t pass with **fixed thresholds and no retries**, and
times the shipped bignum-free ladder against a deliberately secret-dependent
double-and-add.  No mbedTLS bignum, no network fetch.

Thresholds are fixed and identical in every configuration:

| Check | Criterion | Meaning |
|---|---|---|
| Shipped ladder | `|t_shipped| <= 10` | no significant class-dependent leak |
| Variable control | `|t_control| > 10` | detector is non-vacuous: a real leak is flagged |

## Configuration discovery (real probes, no fabricated PASS)

A candidate is `name|label|compiler|opt`.  A candidate is **usable** only if its
compiler exists and actually compiles the probe source.  Two configurations are
DISTINCT when `(compiler, opt)` differs, so -O1/-O2 on one binary and gcc-12
both count.  Diversity requires either two binaries or two optimisation levels.
The host-run transcript below discovered four usable configurations
(4 distinct, 2 binaries, 2 opt levels):

```
[ftx] usable configs: 4 (distinct=4 binaries=2 optlevels=2)
[ftx]   gcc13-O1: gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 (-O1)
[ftx]   gcc13-O2: gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 (-O2)
[ftx]   gcc12-O1: gcc-12 (Ubuntu 12.4.0-2ubuntu1~24.04.1) 12.4.0 (-O1)
[ftx]   gcc12-O2: gcc-12 (Ubuntu 12.4.0-2ubuntu1~24.04.1) 12.4.0 (-O2)
```

Set `FTC_CANDIDATES` (semicolon-separated, same pipe format) to override the
candidate list for portability or fail-closed self-tests; the DISTINCT/diversity
assertions still apply.

## Recorded full-gate output (this host, real compile + run)

```
[ftx] === cross-configuration fixed-EdDSA timing gate (mode=gate) ===
[ftx] usable configs: 4 (distinct=4 binaries=2 optlevels=2)
[ftx] --- config gcc13-O1 (gcc 13.3 -O1) ---
[ftx]   gcc13-O1: gate PASSED (shipped flat, control flagged)
[ftx]   gcc13-O1: fixed_timing_shipped_t=+0.619 low=496887..501828 high=496948..500238 fixed_timing_control_t=-1500.731 low=271610..277081 high=491988..499353
[ftx] --- config gcc13-O2 (gcc 13.3 -O2) ---
[ftx]   gcc13-O2: gate PASSED (shipped flat, control flagged)
[ftx]   gcc13-O2: fixed_timing_shipped_t=+1.758 low=423754..452180 high=423193..430360 fixed_timing_control_t=-448.239 low=230422..233657 high=417020..443200
[ftx] --- config gcc12-O1 (gcc 12.4 -O1) ---
[ftx]   gcc12-O1: gate PASSED (shipped flat, control flagged)
[ftx]   gcc12-O1: fixed_timing_shipped_t=+0.680 low=439349..467583 high=439704..450067 fixed_timing_control_t=-528.535 low=239059..253387 high=432882..447703
[ftx] --- config gcc12-O2 (gcc 12.4 -O2) ---
[ftx]   gcc12-O2: gate PASSED (shipped flat, control flagged)
[ftx]   gcc12-O2: fixed_timing_shipped_t=-0.192 low=433038..450760 high=432208..473237 fixed_timing_control_t=-2886.064 low=235049..237017 high=427036..430269
[ftx] PASS: shipped ladder flat AND variable control flagged under all 4 configurations
[ftx] Reused tools/ed25519_fixed_timing_gate.sh; fixed thresholds, no retries.
FIXED_TIMING_CROSSCONFIG_GATE PASSED
```

Every configuration keeps `|t_shipped| <= 10` while the variable control is
flagged with `|t_control| >> 10`.  Interpretation: the flat shipped result is
not a vacuous detector (the control leaks loudly in the same pass), and it
holds across optimisation levels and two gcc versions — not just one `-O1`
build.

## Negative control (per configuration)

`--negctl` reruns the standalone detector under each configuration and requires
that the deliberately secret-dependent path be flagged.  All four configs
passed (`FIXED_TIMING_CROSSCONFIG_NEGCTL PASSED`); observed control
`t` values: gcc13-O1 `-982.295`, gcc13-O2 `-1440.035`, gcc12-O1 `-213.656`,
gcc12-O2 `-126.937` — all far beyond `|t| > 10`.

## Fail-closed / VERIFIED_NEGATIVE evidence (no fabricated PASS)

If fewer than two distinct, diverse configurations are usable, the driver emits
the exact config-probe transcript, prints `VERIFIED_NEGATIVE`, and exits 1 —
never a SKIP, never a manufactured PASS.  Self-test with a single candidate:

```
$ FTC_CANDIDATES='gcc13-O1|gcc 13.3 -O1|gcc|-O1' \
    bash tools/ed25519_fixed_timing_crossconfig.sh
[ftx] VERIFIED_NEGATIVE: fewer than two DISTINCT, diverse usable compiler
configurations (1 configs, 1 distinct (binary,opt), 1
binaries, 1 opt levels). Exact probe transcript above; no PASS is
fabricated. Cross-configuration timing evidence is unavailable on this host.
[ftx] FAIL: second compiler configuration unavailable
FIXED_TIMING_CROSSCONFIG_GATE FAILED
```

Exit status was 1.  The same fail-closed path applies if a required tool/source
is missing or if any individual configuration's reused gate fails.

## Why this is STILL not a formal constant-time proof

1. Welch's t on one host bounds an observable statistical correlation only;
   absence of a detected signal is not proof of its absence.  Thresholds are
   fixed and there are no retries, so a near miss fails rather than being
   re-sampled until it passes.
2. This covers four gcc configurations on one x86-64 host.  Clang, other
   architectures, and cross-compilation targets are not covered.
3. The clock is `CLOCK_PROCESS_CPUTIME_ID`; microarchitectural channels (cache,
   branch predictor, speculation) are not modeled.
4. The variable control is a synthetic in-probe path, not a regression injected
   into shipped source; it validates the detector, not a formal CT property.
5. SHA-512 and sign/verify orchestration remain out of scope — this measures the
   bignum-free scalar-multiply leaf only (see `docs/ED25519_FIXED_TIMING.md`).

For complementary structural evidence see `docs/ED25519_FIXED_LADDER_MULTI_TOOLCHAIN.md`
and `docs/ED25519_FIXED_LADDER_O0.md`.
