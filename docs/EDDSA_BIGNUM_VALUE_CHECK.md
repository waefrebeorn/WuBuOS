# EdDSA bignum VALUE-independence gate — exact verified negative

This is the next real increment after `docs/ED25519_CALLEE_CT_CHECK.md`.
That gate proved *selection-bit / swap-byte* independence of the ladder and
its in-tree callees, plus an instruction-level audit of the shipped
`mbedtls_mpi_safe_cond_swap`.  It explicitly modelled every callee result and
memory load as PUBLIC, so a secret-derived **value** flowing into
`mbedtls_mpi_add_mpi` / `sub_mpi` / `mul_mpi` / `mod_mpi` / `cmp_mpi` stayed
out of scope.  This leg brings those values into scope for the *shipped*
object code and records the exact boundary that the local devroot permits.

Entry points:

| target | what it runs |
|---|---|
| `make test_ed25519_bignum_value` | full gate (negative control first, then shipped-object audit); **fails closed with `BIGNUM_VALUE_GATE VERIFIED_NEGATIVE`**, exit 1 |
| `make test_ed25519_bignum_value_negctl` | standalone injected-branch negative control |

Both are opt-in, like the other compiled/machine-code CT gates: a missing
gcc/objdump/mbedTLS archive must never become a silent PASS.  The always-on
structural ladder guard stays in the default tier.

## What is machine-checked

`tools/eddsa_bignum_toolchain.sh` extracts the real `bignum.c.o` from the
LOCAL `libmbedcrypto.a` and disassembles it with `objdump -d -M intel`.  The
dump carries a provenance marker with the archive SHA-256; the auditor refuses
to run without it.  Nothing is fetched, installed, or substituted by a fixture.

`tools/eddsa_bignum_value_check.py` is a bounded abstract interpretation over
those instructions.  Per register and flags the lattice is a subset of:

* `P` — public (unseeded argument, immediate, `xor r,r` kill);
* `SP` — pointer to a secret-derived `mbedtls_mpi` (seeded argument register);
* `S` — secret-derived value (load through `SP`/`S`, or a seeded scalar arg);
* `U` — unproven (call result, `pop`, unknown mnemonic). NOT blessed as public.

Loads through a secret mpi pointer make the limb / limb-count / sign values
secret.  Transfer covers `mov`/`movzx`/`movsx`/`lea`, the `xor r,r` kill,
binary arithmetic, `neg`/`not`/`inc`/`dec`, `setcc`, `cmovcc`, `cmp`/`test`,
`mul`/`div`, `call` (caller-saved registers and flags become `U`), `push`/
`pop`/`ret` and no-effect instructions.  A monotone **worklist fixpoint** joins
state at every branch target, so this is not a naive linear scan.  Every
conditional jump records its exact flag-setting instruction; a memory operand
with a secret index register is recorded too.

Seed sets are explicit per function and bound to the SysV argument positions
that carry mpi pointers / secret limb values.  `add_sub_mpi` is the real body
behind `mbedtls_mpi_add_mpi`/`sub_mpi`; `mpi_mul_hlp`, `mbedtls_mpi_div_mpi`
and `mbedtls_mpi_cmp_abs` are the in-archive helpers behind `mul_mpi`,
`mod_mpi` and `cmp_mpi`.

## Recorded result — Ubuntu mbedTLS 2.28.8 / gcc 13.3.0 / archive
`sha256=6bd509d3de1b8e885a64d655a69ef497b44c0167357437c8c94a7e4c18673f4f`

| shipped function | insns | sites | jcc | SECRET_GOVERNED | UNVERIFIED |
|---|---:|---:|---:|---:|---:|
| `mbedtls_mpi_cmp_mpi` | 71 | 21 | 17 | 21 | 0 |
| `mbedtls_mpi_cmp_abs` | 54 | 16 | 12 | 16 | 0 |
| `add_sub_mpi` | 64 | 5 | 5 | 1 | 4 |
| `mbedtls_mpi_mul_mpi` | 150 | 20 | 19 | 7 | 4 |
| `mpi_mul_hlp` | 294 | 7 | 7 | 7 | 0 |
| `mbedtls_mpi_div_mpi` | 459 | 48 | 48 | 5 | 31 |
| `mbedtls_mpi_mod_mpi` | 86 | 7 | 7 | 0 | 6 |
| **total** | **1178** | **124** | **115** | **57** | **45** |

**102 exact non-public instructions** (57 secret-governed control/address
sites, 45 unproven flag sources) are listed instruction-by-instruction in
`docs/EDDSA_BIGNUM_VALUE_EVIDENCE.txt`.  Each line gives function, address,
kind (`JCC` or `INDEX`), the raw disassembly, the exact governing instruction,
the taint tags and the verdict.  Examples:

```
mbedtls_mpi_cmp_mpi @11a6 INDEX  cmp QWORD PTR [rcx+rax*8-0x8],0x0 | secret index rax | tags={P,S} | SECRET_GOVERNED
mbedtls_mpi_cmp_mpi @11d8 JCC    jb 1230 | governing: cmp rax,rdx | tags={P,S} | SECRET_GOVERNED
mpi_mul_hlp         @0012 JCC    jbe 390 | governing: cmp r9,0xf | tags={P,S} | SECRET_GOVERNED
mbedtls_mpi_mod_mpi @28cd JCC    js 299d | governing: test eax,eax | tags={U} | UNVERIFIED
```

The refutation is concrete: limb counts (`mpi->n`), limb values and the
sign fields loaded through the secret mpi pointers govern conditional jumps
and `reg*8` indices in `cmp_mpi`/`cmp_abs`/`add_sub_mpi`/`mpi_mul_hlp`, and a
large set of `div_mpi`/`mod_mpi` branches depend on call results that cannot
be proven public from headers + `.a` alone.

## Fail closed, never a fake PASS

No provenance marker, missing gcc/objdump/devroot/archive, a disassembly
without all seven required functions, an empty/vacuous inventory, fewer than
the minimum instructions, an auditor crash, a negative control that does not
fire, or an unexpected verdict: all FAIL with exit 1.  When non-public sites
exist the gate prints `BIGNUM_VALUE_GATE VERIFIED_NEGATIVE` and exits 1; it
only prints `BIGNUM_VALUE_GATE PASS` if every audited site is PUBLIC.

## Non-vacuity / negative control

`tests/fixtures/value_ct/callee_value_probe.c` compiles one callee two ways
with the same local gcc `-O2`.  Without the macro, `bn_branchy_limb_select`
is a branch-free `sar/and/xor` mask select and the auditor accepts it
(0 secret-governed sites).  With `WUBU_INJECT_VALUE_BRANCH`, an `if
(a->p[0] > b->p[0])` on a secret limb becomes, in real `-O2` machine code:

```
3e: cmp rax,rcx
41: jae 4b        | governing: cmp rax,rcx | tags={S} | SECRET_GOVERNED
```

The recorded run accepted the clean callee and refuted the injected one, then
audited the shipped object and refused to assert a PASS.

## STATUS and remaining gaps

**CLOSED at the verified-negative boundary:** this leg adds a real detector,
a non-vacuity control, an exact instruction-level inventory, and a precise
refusal distinct from a toolchain failure.  It does **not** claim, and must
not be read as, a value-independence PASS for the shipped bignum callees.
What the refusal still blocks:

1. **No mbedTLS C source in the devroot** (headers + `.a` only).  Proving the
   48 `U` sites public needs the source and a value/call analysis of the
   `grow`/`div` result paths.  This is the same class of boundary recorded for
   the five public-by-assumption `jcc` in `safe_cond_swap`, now extended to
   the bignum internals.
2. **The `S` sites are conservative under the seeded precondition.**  The
   ladder reduces field elements modulo p before most calls, so a
   value-range / limbs-bound proof could discharge some `mpi->n`-driven
   branches (fixed public limb count for reduced inputs).  No such proof is
   available from this devroot, so they are recorded as refuted, not waived.
3. **One toolchain, one library build.**  Results bind gcc 13.3.0 `-O2` and
   the pinned mbedTLS 2.28.8 archive SHA-256 above; there is no
   cross-toolchain/cross-version preservation argument.
4. **Control/address independence, not data independence.**  Arithmetic
   results legitimately depend on operand values; the check proves (or here,
   refutes) that the *operation sequence and memory addresses* do not depend
   on secret-derived values.  A full constant-time claim would still need a
   ct-verif/ctgrind-style proof of the C and the assembly plus a compiler
   preservation argument.
