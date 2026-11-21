# Bignum-free fixed-width Ed25519 ladder — proof-style gate

This is the next real increment after `docs/EDDSA_BIGNUM_VALUE_CHECK.md`.
That leg proved the mbedTLS bignum callees are **not** value-independent from
headers + `.a` alone and recorded an exact verified negative.  Rather than
assert a property we cannot see through, this leg builds the scalar
multiplication leaf we **do** own end to end: a self-contained fixed-width
(4 x 64-bit limbs, mod `q = 2^255 - 19`) Edwards ladder with no mbedTLS bignum,
and a proof-style gate over the shipped source AND its real gcc `-O2` machine
code.  The package-signing gate now uses that bignum-free leaf BY DEFAULT;
the mbedTLS ladder remains available as an explicit legacy override.

| target | what it runs |
|---|---|
| `make test_ed25519_fixed_ladder` | full gate: negative control, source contract, `-O2` disassembly audit, 14 RFC 8032 vectors |
| `make test_ed25519_fixed_ladder_negctl` | standalone injected-branch non-vacuity control |
| `make test_pkg_ed25519` | real package-signing gate — **default** backend is the fixed-width ladder |
| `make test_pkg_ed25519_fixed` | same gate with the fixed ladder forced (`WUBU_EDDSA_FIXED=1`) |
| `make test_pkg_ed25519_legacy` | same gate through the **legacy override** (`WUBU_EDDSA_FIXED=0`, mbedTLS MPI ladder) |
| `make test_ed25519_fixed_ladder_default` | default-tier adapter: full gate when python3/gcc/objdump exist, explicit `SKIP` otherwise (wired into `test_medium_other`) |

`make test_ed25519_fixed_ladder` and `test_ed25519_fixed_ladder_negctl` stay
strict (fail closed on a missing tool); only the default-tier adapter skips.
The opt-in crypto gates still need an mbedTLS dev root, but the proof-style
CT gate itself is root-independent and now runs in the default tier where the
toolchain permits.

Set `WUBU_EDDSA_FIXED=0` in the environment of `tools/pkg_ed25519_gate.sh`
(and via `make test_pkg_ed25519_legacy`) to return to the legacy mbedTLS bignum
ladder.  Any other value — including unset — selects the fixed-width ladder.

All gates fail closed (exit 1, never a fake PASS) on a missing tool or an
unmet obligation; the default-tier adapter is the sole, explicit `SKIP` path.

## What is machine-checked

`tools/ed25519_fixed_ladder_gate.sh`:

1. **Non-vacuity first.** `tools/ed25519_fixed_ladder_negctl.sh` copies the
   shipped module to a temp dir (the working tree is never touched), injects
   `if (bit) { dbl.X.l[0] ^= 1u; }` into the ladder, and requires BOTH detectors
   to go red on the injected copy and green on the shipped source.
2. **Source contract** (`tools/ed25519_fixed_ladder_check.py source`):
   * S1 trip count is the compile-time constant `WUBU_ED25519_FIXED_LADDER_BITS == 256`;
   * S2 `bit`/`swap` never select a branch, ternary or memory index; every
     `wubu_ed25519_fixed_cswap` call is unconditional and driven by `swap`;
   * S3 `cswap` builds an all-ones/all-zeros mask with `0 - swap` and selects
     both sides of every limb, over all four projective coordinates;
   * S4 zero `mbedtls_*` / bignum symbols anywhere in the module.
3. **Compiled-tree contract** (`... check.py asm`, real `gcc -O2` + `objdump -d -M intel`):
   * A1 `mov r13d,0x100` and `sub r13d,1` survive `-O2` (constant trip count);
   * A2 scalar-bit load from `[scalar_ptr + loop_counter]`, `xor` into the swap
     register, and an unconditional `call wubu_ed25519_fixed_cswap`;
   * A3 no conditional jump governed by a scalar-bit-derived register/flags;
   * A4 no memory operand indexed by a scalar-bit-derived register;
   * A5 `cswap` lowers to `neg` + `pand` + `por` (implicit mask), with no
     secret-governed branch.
   A bounded linear taint analysis (GPR aliases, RMW taint, loop-counter kills)
   drives A3/A4/A5.  Every PASS prints the exact governing instruction.
4. **Independent correctness oracle.** `tools/gen_ed25519_fixed_vectors.py`
   is a pure-Python big-int RFC 8032 scalar multiplication (extended Edwards).
   The gate generates 11 fixed base-point scalars (0, 1, 2, 3, 7, `L-1`, `L`,
   `q`, a 254-bit scalar, a full-width scalar, `bytes(0..31)`) and 3 multiples
   of an arbitrary point, then requires the shipped C ladder to match all 14.

Machine-checkable claim on success:

> The shipped `wubu_ed25519_fixed.c` contains no bignum call, runs a fixed
> 256-iteration ladder, and in real gcc -O2 machine code no scalar-bit-derived
> value governs a branch or a memory index; `cswap` is a mask select; the
> ladder agrees with an independent RFC 8032 oracle on all 14 vectors.

## Recorded run (this leg, gcc 13.3.0, `objdump -d -M intel`)

```
[fixed_ladder] S4 PASS: zero mbedtls/bignum symbols in module
[fixed_ladder] S1 PASS: trip count = compile-time constant 256
[fixed_ladder] S2 PASS: no secret branch/ternary/index; 2/2 cswap calls driven by swap
[fixed_ladder] S3 PASS: cswap is mask-only over all 4 coordinates
[fixed_ladder] A1 PASS: `mov r13d,0x100` and `sub r13d,1` survive gcc -O2
[fixed_ladder] A2/A3/A4 PASS: 1 scalar-bit loads, 2 unconditional cswap calls, 0 secret-governed jcc/index
[fixed_ladder] A5 PASS: cswap lowered to neg+pand+por mask, no secret-governed branch
[fixed_gate] 14/14 scalar-mult vectors match the Python oracle
FIXED_LADDER_GATE PASSED
```

## Non-vacuity / negative control (exact output)

```
[negctl] injected: if (bit) { dbl.X.l[0] ^= 1u; }
[fixed_ladder] FAIL: secret-dependent branch in ladder
[fixed_ladder] JCC @aea aea: 74 09 je af5 | governing=ae7: 45 84 ff test r15b,r15b
[fixed_ladder] FAIL: 1 secret-governed branch/index site(s) in compiled ladder
[negctl] PASS: detector red on injected branch, green on shipped ladder
FIXED_LADDER_NEGCTL PASSED
```

The branch is in REAL `-O2` machine code (`test r15b,r15b` / `je`), not a
hand-written fixture; the temp copy is discarded and the shipped file is never
modified.

## Integration boundary (what is and is not bignum-free)

`src/gui/wubu_pkg_ed25519.c` swaps **only** the scalar multiplication leaf when
`WUBU_EDDSA_FIXED_LADDER` is defined: `ge_mul` becomes `ge_mul_fixed`, which
writes the scalar to 32 fixed bytes, encodes the point, calls
`wubu_ed25519_fixed_scalarmult`, and decodes the result.  The module
`wubu_ed25519_fixed.c` itself has zero bignum references (S4).

Out of scope for this increment, unchanged from the mbedTLS path and explicitly
**not** claimed constant-time here:

1. SHA-512 hashing and scalar arithmetic mod `L` (`S = k*a + r`) remain on
   mbedTLS; they are secret-dependent in values but are not the ladder.
2. Point encode/decode in the adapter uses mbedTLS MPI inversion and square
   roots.  Its inputs are the public base point, the public key, or the
   published signature `R`, but no value-independence claim is made for it.
3. Full data-independent instruction timing (ct-verif/ctgrind style), cache /
   branch-predictor effects beyond the audited jcc/index sites, and compiler
   preservation across toolchains.  One toolchain (gcc 13.3.0 `-O2`) is bound.

## STATUS and remaining gaps

**CLOSED at this increment's boundary:** a real bignum-free ladder exists, is
the DEFAULT backend for package signing, and is gated by source + machine-code +
non-vacuity + independent-oracle evidence.  The legacy mbedTLS bignum ladder is
retained behind the documented `WUBU_EDDSA_FIXED=0` override and still passes
the same end-to-end differential.

1. The fixed path is the signing default, but `WUBU_EDDSA_FIXED=0` still selects
   the mbedTLS bignum ladder whose value-independence is a verified negative;
   that override exists for compatibility and is deliberately not removed.
2. End-to-end constant-time signing still needs fixed-width SHA-512 and
   scalar-mod-`L` arithmetic; only the group scalar multiplication is
   bignum-free today.
3. The gate bounds branches/indices, not data-dependent *timing of individual
   instructions* or other microarchitectural channels; a ctgrind/ct-verif
   proof and a second toolchain are still open.
