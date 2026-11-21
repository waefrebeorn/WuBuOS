# Bignum-free fixed Ed25519 ladder — second-toolchain / optimization-level structural CT gate

This is the second-configuration closure of `docs/ED25519_FIXED_LADDER_CHECK.md`.
That gate bound the shipped bignum-free ladder's structure and real machine code
to **one** toolchain (gcc 13.3.0 `-O2`).  This leg runs the SAME existing checks
under multiple compiler configurations, requires identical verdicts, and proves
each configuration is non-vacuous.

It is **structural/machine-code evidence, not a formal constant-time proof**.
The same-run dudect statistical timing evidence
(`tools/ed25519_fixed_timing_gate.sh`, gcc 13.3 `-O1`, `CLOCK_PROCESS_CPUTIME_ID`)
is unchanged; this leg adds a config-diverse structural gate beside it.

| target | what it runs |
|---|---|
| `make test_ed25519_fixed_ladder_multitoolchain` | full gate: source contract + per-config real-machine-code audit + per-config injected-branch negative control + identical-verdict assertion |
| `make test_ed25519_fixed_ladder_multitoolchain_negctl` | standalone per-configuration injected-branch non-vacuity control |

`tools/ed25519_fixed_ladder_multitoolchain.sh` (with `--negctl` for the
standalone control) is the implementation.  It reuses:

* `tools/ed25519_fixed_ladder_check.py source` — toolchain-independent
  structural C contract (constant 256 trip count, no secret branch/ternary/index,
  mask-only `cswap`, zero bignum symbols), run once;
* `tools/ed25519_fixed_ladder_check.py asm` — the real-machine-code audit
  (constant trip count survives, no secret-governed jump or index, unconditional
  `cswap` calls, mask-select lowering), run **per configuration**;
* `tools/ed25519_fixed_ladder_inject.py` — injects
  `if (bit) { dbl.X.l[0] ^= 1u; }` into a TEMP COPY (working tree untouched).

## Configurations and why they are distinct

| config | compiler | opt | axis closed |
|---|---|---|---|
| `gcc13-O2` | gcc 13.3.0 | `-O2` | baseline (the original bound) |
| `gcc13-O3` | gcc 13.3.0 | `-O3` | optimization level: aggressive inlining/reordering |
| `gcc12-O2` | gcc 12.4.0 | `-O2` | toolchain version: a different compiler binary |

A candidate is used only if its compiler exists on `PATH` **and** actually
compiles the shipped module.  Distinctness is over `(binary, opt)`; the gate
requires at least two distinct configs spanning at least one changing axis.

## Checker generalization (same property, lowering-neutral)

The original `asm` checker required SIMD `neg + pand + por` for `cswap`, which is
gcc 13.3 `-O2`'s lowering.  `-O3` lowers the same mask select to GPR
`neg + and + or`.  The checker now accepts either lowering:

* `neg` must be present (the all-ones/all-zeros mask from `swap & 1`), and
* at least 4 SIMD selects (`pand`/`por`, two limbs each) **or** 8 GPR selects
  (`and`/`or`) must be present.

This is not a weakening: the independent taint analysis (`analyze_cswap`) still
fails on any secret-governed branch or index, and a branchy select has neither
the required `neg`+select body nor a clean taint result.  The existing
single-config gate and its negative control were re-run and remain GREEN.

`-O0` is deliberately **not** a configuration: at `-O0` the scalar bit and
`swap` live in stack slots, which the checker's GPR-taint model does not track,
so a `-O0` RED would not be a reliable non-vacuity signal.  That is recorded as
a remaining gap rather than papered over with a fabricated PASS.

## Recorded run (this leg, on this host)

```
[mt_ct] usable configs: 3 (distinct=3 binaries=2 optlevels=2)
[mt_ct]   gcc13-O2: gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 (-O2)
[mt_ct]   gcc13-O3: gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 (-O3)
[mt_ct]   gcc12-O2: gcc-12 (Ubuntu 12.4.0-2ubuntu1~24.04.1) 12.4.0 (-O2)
[mt_ct] injected: if (bit) { dbl.X.l[0] ^= 1u; }  (temp copy only)
[fixed_ladder] S4 PASS: zero mbedtls/bignum symbols in module
[fixed_ladder] S1 PASS: trip count = compile-time constant 256
[fixed_ladder] S2 PASS: no secret branch/ternary/index; 2/2 cswap calls driven by swap
[fixed_ladder] S3 PASS: cswap is mask-only over all 4 coordinates
[mt_ct] --- config gcc13-O2 (gcc 13.3 -O2) ---
[mt_ct]   shipped=GREEN injected=RED secret_governed_jcc=1
[mt_ct]   [fixed_ladder] JCC @aea aea:	74 09               	je     af5 <wubu_ed25519_fixed_ladder+0x155> | governing=ae7:	45 84 ff            	test   r15b,r15b
[mt_ct] --- config gcc13-O3 (gcc 13.3 -O3) ---
[mt_ct]   shipped=GREEN injected=RED secret_governed_jcc=1
[mt_ct]   [fixed_ladder] JCC @1b7a 1b7a:	74 09               	je     1b85 <wubu_ed25519_fixed_ladder+0x155> | governing=1b77:	45 84 ff            	test   r15b,r15b
[mt_ct] --- config gcc12-O2 (gcc 12.4 -O2) ---
[mt_ct]   shipped=GREEN injected=RED secret_governed_jcc=1
[mt_ct]   [fixed_ladder] JCC @bda bda:	74 09               	je     be5 <wubu_ed25519_fixed_ladder+0x155> | governing=bd7:	45 84 ff            	test   r15b,r15b
[mt_ct] identical verdicts across all 3 configurations (distinct verdict patterns=1)
[mt_ct] PASS: source + real machine code bound under 3 compiler configurations;
[mt_ct]       every configuration is non-vacuous; verdicts identical.
MULTI_TOOLCHAIN_GATE PASSED
```

The negative control's branch is in each configuration's REAL compiled machine
code (`test r15b,r15b` / `je`), not a hand-written fixture; the temp copy is
discarded and the shipped file is never modified.  Standalone control output:

```
[mt_ct] PASS: every configuration's detector is non-vacuous (injected branch RED)
MULTI_TOOLCHAIN_NEGCTL PASSED
```

## VERIFIED_NEGATIVE evidence (fail-closed, no fabricated PASS)

When fewer than two distinct configurations are usable the gate fails closed
and prints the exact probe transcript.  Two real exercised cases:

```
$ MT_CANDIDATES="gcc13-O2|gcc 13.3 -O2|gcc|-O2" bash tools/ed25519_fixed_ladder_multitoolchain.sh
[probe] command -v gcc => /usr/bin/gcc
[probe] gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
[mt_ct] VERIFIED_NEGATIVE: fewer than two DISTINCT usable compiler
configurations (1 configs, 1 distinct (binary,opt), 1
binaries, 1 opt levels). Exact probe transcript above; no PASS
is fabricated. Structural CT evidence remains bound to ONE config.
[mt_ct] FAIL: second compiler configuration unavailable
MULTI_TOOLCHAIN_GATE FAILED
```

```
$ MT_CANDIDATES="no-such-cc|X|definitely-not-a-compiler|-O2" bash tools/ed25519_fixed_ladder_multitoolchain.sh
[probe] --- candidate no-such-cc (definitely-not-a-compiler -O2) ---
[probe] command -v definitely-not-a-compiler =>
[mt_ct] VERIFIED_NEGATIVE: fewer than two DISTINCT usable compiler
configurations (0 configs, ...). ... no PASS is fabricated.
MULTI_TOOLCHAIN_GATE FAILED
```

`MT_CANDIDATES` is only a portability/self-test override: the distinctness and
uniqueness assertions still apply, so it cannot be used to weaken the gate to a
single config.

## Machine-checkable claim on success

> The shipped `wubu_ed25519_fixed.c` passes its source structural contract and
> its real compiled machine-code audit under gcc 13.3 `-O2`, gcc 13.3 `-O3` and
> gcc 12.4 `-O2` with IDENTICAL verdicts; on every configuration an injected
> `if (bit)` branch is detected as a secret-governed jump in real machine code,
> proving each check is non-vacuous.

## Remaining gaps

1. `-O0` was originally not covered (the GPR-taint model could not track
   stack-slot secret flow).  That gap is now closed by the stack-aware checker
   `tools/ed25519_fixed_ladder_o0_check.py` and gate
   `tools/ed25519_fixed_ladder_o0_gate.sh` (`make test_ed25519_fixed_ladder_o0`,
   `make test_ed25519_fixed_ladder_o0_negctl`); see
   `docs/ED25519_FIXED_LADDER_O0.md`.  A ctgrind/ct-verif proof is still absent.
2. Still structural/machine-code evidence, not a formal constant-time proof:
   no ct-verif/ctgrind, no interprocedural/assembly preservation argument, and
   microarchitectural channels are out of scope.
3. The differential timing evidence remains one toolchain (`-O1`) and process
   CPU time only; cross-config timing is not claimed here.
