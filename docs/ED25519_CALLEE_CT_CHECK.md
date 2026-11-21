# Callee secret-independence check for the Ed25519 ladder

This is the next increment after `docs/ED25519_GIMPLE_CT_CHECK.md`.  That gate
proved one thing and declared a boundary: in the real gcc `-O2` GIMPLE of
`ge_mul_ladder` no `mbedtls_mpi_get_bit`-derived SSA value controlled a branch,
switch or memory index — but **callee results were modelled as public**, so the
secret `swap` byte handed to `ge_cswap()` was never checked inside the callee,
and the mbedTLS machine code that actually performs the swap was never
inspected.  This document closes both halves of that gap as far as local,
real artifacts allow, and records exactly what remains unverified.

Entry point: `make test_ed25519_callee_ct`
(`tools/eddsa_callee_ct_gate.sh`).  Negative control:
`make test_ed25519_callee_ct_negctl`.  Both are opt-in because they FAIL CLOSED
(no gcc / objdump / mbedTLS archive => no PASS), like the other compiled-tree
CT gates; the always-on structural ladder guard remains in the default tier.

## What is machine-checked

### 1. Interprocedural secret-taint over real `-O2` GIMPLE

`tools/eddsa_callee_ct_check.py` parses **every** function in the real
`-fdump-tree-optimized` output of the shipped
`src/gui/wubu_pkg_ed25519.c`, seeds the scalar bit from `mbedtls_mpi_get_bit`
in `@ge_mul_ladder`, then propagates taint **across call sites by argument
position** to a fixpoint: a callee parameter is tainted when a tainted caller
argument reaches it.  SSA defs / casts / xor / PHI propagate taint inside each
function; call results (error codes) stay public.  It then fails on any
secret-derived branch, switch or memory index in **any reachable callee**, and
emits a per-callee verdict table.

Recorded gcc 13.3.0 `-O2` result (9 reachable callees):

| Callee | secret tainted params | secret-controlled branch/switch/index | verdict |
|---|---|---|---|
| `ge_mul_ladder` | `_3` (scalar bit) | 0 | INDEPENDENT |
| `ge_cswap` | `swap_14` (the swap byte) | 0 | INDEPENDENT |
| `ge_add`, `ge_copy`, `ge_grow.constprop`, `ge_identity`, `ge_base`, `mpi_addmod`, `mpi_mulmod`, `mpi_submod` | none | 0 | INDEPENDENT (no secret selection input) |

It also enforces the **sink contract**: `ge_cswap` must forward its tainted
`swap` into the 3rd (data) argument of all **4** `mbedtls_mpi_safe_cond_swap`
calls (`4/4` observed).  In GIMPLE the selection branches in `ge_cswap` are
`if (rc_19 == 0)` / `rc_21` / `rc_23` — error codes from the calls, not the
secret byte.  The loop PHI retains the literal `256` trip count.

### 2. Shipped mbedTLS `safe_cond_swap` MACHINE CODE (the assembly path)

The devroot has headers + `libmbedcrypto.a` but **no mbedTLS C source**, so a
source proof is impossible here.  Instead the gate extracts `constant_time.c.o`
from the local archive and disassembles the **shipped** function with
`objdump -d -M intel` — real object code, not a fixture.  Ubuntu's
`libmbedcrypto7t64` 2.28.8 / gcc 13.3.0 build was audited with a conservative
forward register-taint walk seeded on `edx` (SysV arg 3 = `swap`), with kills
(`xor r,r`), `setcc`/`cmov` handling and call clobbers.

Result: **5 conditional jumps, 0 governed by the secret swap byte.**  The
secret-derived instruction trace is the branch-free mask/XOR select:

```
773: mov r13d,edx        7b6: mov edx,ecx        7c0: sar edx,0x1f
7b0: movzx ecx,r13b      7b8: neg edx           7c5: and ecx,edx
7cf: xor eax,ecx         7d1: and eax,edx       7d3: xor eax,ecx
7e0: movzx r13d,r13b     7ef: mov rdi,r13       7f2: neg rdi
7f5: or rdi,r13          7f8: sar rdi,0x3f      800: mov rax,[r8+rdx*8]
808: xor rcx,rax         80b: and rcx,rdi       80e: xor rcx,rax
```

`swap` is converted to a 32/64-bit all-ones/all-zeros mask by
`neg|or|sar` and applied with `xor/and/xor` on every limb — the documented
constant-time pattern.

### 3. GCC `-O2` lowering of a constant-time select to cmov

`tests/fixtures/callee_ct/ct_select_probe.c` (`return cond ? x : y;`) is
compiled with the same local gcc at `-O2` and disassembled.  The auditor
requires a `cmov`/`setcc` to be present and zero secret-governed jumps.
Observed lowering: `test edi,edi; mov eax,esi; cmove eax,edx; ret` — branch
free.  This closes the "could `-O2` turn the select idiom into a branch?"
question for this compiler; the mask idiom above is checked separately in (2).

## Fail closed, never a fake PASS

No provenance marker, no `funcdef_no=`/PHI artifacts, no `@ge_mul_ladder`, no
taint source, no `ge_cswap`, fewer than 4 safe_cond_swap sinks, a disassembly
without the real function, an unparseable/vacuous audit, a missing `cmov`, or
any secret-controlled control/index site: all FAIL with exit 1.  The mbedTLS
root is resolved offline from `WUBU_MBEDTLS_ROOT` / `mbedtls_devroot.sh` or the
same repo's `--git-common-dir` cache; nothing is fetched.

## Non-vacuity / negative control

`tools/eddsa_callee_ct_negative_control.sh` (also run first by the gate)
injects a secret-dependent cswap at **both** levels and requires both detectors
to fire, then restores and requires green:

* GIMPLE: `if (swap) TRY(ge_cswap(&r0, &r1, 1));` in the shipped source becomes
  `if (bit_37 != swap_76)` in real `-O2` GIMPLE — the interprocedural checker
  flags it.
  Assembly: `tests/fixtures/callee_ct/branchy_cswap_probe.c` compiles to
  `test dil,dil; je` — the register-taint auditor flags the secret-governed
  jump.  Recorded run: both fired; shipped ladder green after restore.

## Remaining gaps — VERIFIED evidence, NOT a formal proof

This is structural + machine-code evidence for specific artifacts, not a
mechanized constant-time proof.  The exact unverified instructions are:

1. **The 5 conditional jumps that remain in the shipped `safe_cond_swap` are
   classified public by assumption, not by a machine proof.**  Their exact
   instructions and flag sources are inventoried above:
   * `767: je 840` ← `764: cmp rdi,rsi` — `X == Y` pointer identity;
   * `78e: je 7a0` ← `78c: test eax,eax` — return code of
     `mbedtls_mpi_grow(X, n)`;
   * `7ae: jne 790` ← `7ac: test eax,eax` — return code of
     `mbedtls_mpi_grow(Y, n)`;
   * `7de: je 830` ← `7d5: cmp QWORD PTR [rbx+0x8],0x0` — `X->n == 0`;
   * `82e: jb 800` ← `82a: cmp rdx,[rbx+0x8]` — limb loop counter `< X->n`.
   Proving those governing values non-secret needs the mbedTLS C source (absent
   from this devroot — headers + `.a` only) plus a value/call analysis through
   the two `call mbedtls_mpi_grow` sites.  The register-taint walk treats memory
   contents and call return values as public; that is recorded here as the
   assumption it is, not as a verified fact.  (The ladder pre-grows all limbs
   before the loop, so the grow-failure path is not taken mid-ladder; that
   mitigation is source-level evidence, not part of the machine check.)
2. **Bignum-internal data-dependent behaviour — now IN SCOPE, VERIFIED
   NEGATIVE.** The GIMPLE checker models every external call result as public
   and does not taint the secret-derived *values* that flow into
   `mbedtls_mpi_add_mpi/sub_mpi/mul_mpi/mod_mpi/cmp_mpi`.  That boundary has
   been advanced by `docs/EDDSA_BIGNUM_VALUE_CHECK.md`
   (`make test_ed25519_bignum_value`): a bounded abstract interpretation of the
   SHIPPED bignum machine code from the local `libmbedcrypto.a` finds 102 exact
   non-public control/address sites (57 secret-governed, 45 unproven) and
   **fails closed with `BIGNUM_VALUE_GATE VERIFIED_NEGATIVE`** rather than
   claiming value-independence.  The gate proves *selection-bit / swap
   independence*; value-independence of the group field arithmetic is NOT
   established, and the exact refuting instructions are recorded in
   `docs/EDDSA_BIGNUM_VALUE_EVIDENCE.txt`.  A full CT claim would still need a
   ct-verif/ctgrind-style verification of those routines plus their C source,
   which this devroot (headers + `.a`) does not contain.
3. **One toolchain, one library build.**  Results are bound to gcc 13.3.0
   `-O2` and Ubuntu's mbedTLS 2.28.8 `libmbedcrypto.a`; they are not a
   cross-toolchain/cross-version preservation argument (e.g. compiler-iris).
4. **The cmov probe is a model, not the production call.**  It proves gcc's
   lowering of the `?:` idiom; the production `safe_cond_swap` is precompiled
   object code audited in (2), which used `neg|or|sar|and|xor` (also branch
   free) rather than cmov.  No single artifact proves both implementations.

Consequently this document claims **machine-checked secret-selection
independence of the ladder and every reachable callee, plus an instruction-level
audit of the shipped swap primitive**, and explicitly does **not** claim formal
constant-time verification.  The residual gap is also tracked in
`docs/ED25519_CONSTANT_TIME_EVIDENCE.md`.
