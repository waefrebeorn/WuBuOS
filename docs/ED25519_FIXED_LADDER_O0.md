# Bignum-free fixed Ed25519 ladder — stack-aware `-O0` structural CT gate

**Status: structural / machine-code evidence, NOT a formal constant-time proof.**

This closes the `-O0` gap recorded in `docs/ED25519_FIXED_LADDER_MULTI_TOOLCHAIN.md`.
That gate covered gcc 13.3 `-O2`/`-O3` and gcc 12.4 `-O2`, but deliberately
excluded `-O0`: at `-O0` the scalar bit and the swap byte are spilled to stack
slots (`[rbp-X]`), and the register-only taint model in
`tools/ed25519_fixed_ladder_check.py` cannot see them, so a `-O0` RED verdict
would be vacuous.  This increment adds a **stack-aware** checker that tracks a
stack-slot taint lattice alongside the register lattice, and proves on real
`-O0` machine code that the shipped ladder is clean and an injected secret
branch is caught.

| target | what it runs |
|---|---|
| `make test_ed25519_fixed_ladder_o0` | full gate: source contract + per-binary real `-O0` machine-code audit + per-binary injected-branch negative control + identical-verdict assertion |
| `make test_ed25519_fixed_ladder_o0_negctl` | standalone per-binary injected-branch non-vacuity control |

## What the `-O0` code actually looks like

gcc 13.3 `-O0` for `wubu_ed25519_fixed_ladder` (objdump, Intel syntax):

```
124b: mov  QWORD PTR [rbp-0x238],rdi       ; r  argument slot
1252: mov  QWORD PTR [rbp-0x240],rsi       ; scalar pointer argument -> slot
133b: mov  DWORD PTR [rbp-0x224],0x100     ; 256-trip counter slot
1358: mov  rax,QWORD PTR [rbp-0x240]       ; reload scalar pointer slot
135f: add  rax,rdx                         ; + byte offset
1362: movzx eax,BYTE PTR [rax]             ; <-- SECRET SOURCE (byte load)
137d: mov  BYTE PTR [rbp-0x225],al         ; bit slot
1383: movzx eax,BYTE PTR [rbp-0x225]       ; bit reload
138a: xor  BYTE PTR [rbp-0x226],al         ; swap ^= bit  (swap slot)
1390: movzx edx,BYTE PTR [rbp-0x226]       ; swap -> cswap 3rd argument
13ab: call <wubu_ed25519_fixed_cswap>      ; unconditional
1599: sub  DWORD PTR [rbp-0x224],0x1
15a0: cmp  DWORD PTR [rbp-0x224],0x0
15a7: jne  134a                            ; public loop back-edge
```

The secret is carried by stack slots `[rbp-0x240]` (scalar pointer),
`[rbp-0x225]` (bit), `[rbp-0x226]` (swap), not by registers across calls.

## Checker (`tools/ed25519_fixed_ladder_o0_check.py`)

`source` mode delegates to the toolchain-independent source contract in
`tools/ed25519_fixed_ladder_check.py` (S1–S4).  `asm` mode walks real
`objdump -d -M intel` output for the two functions with a taint lattice over
**registers, stack slots, and flags**:

* **O1** — constant 256 trip count survives `-O0` (`mov [slot],0x100` +
  `sub [slot],1`, same slot);
* **O2** — a byte load through a register derived from the scalar-pointer slot
  (`mov [rbp-X],rsi` pre-scan → `mov rax,[rbp-X]` → `add` → `movzx ...[rax]`) is
  a taint source;
* **O3** — taint propagates through registers **and** stack slots to the
  unconditional `cswap` call's 3rd argument, which must be secret-derived;
* **O4** — no conditional jump is governed by a tainted register, stack slot or
  flag (unconditional `jmp` is excluded; the public loop back-edge is allowed);
* **O5** — no memory access is indexed by a value-tainted register (a
  secret-*pointer* register is not itself a leak);
* **O6** — `cswap` is a mask select at `-O0` (`neg` + `not` + GPR `and`/`or`;
  the body is a loop, not unrolled), swap taint reaches ≥ 2 select ops, and no
  branch there is secret-governed.

Call handling kills only caller-saved registers — at `-O0` all locals live in
stack slots, which deliberately survive calls.  Every unmet obligation calls
`die()` (exit 1); no PASS is printed without a real `-O0` compile and walk.

## Configurations and fail-closed contract

| config | compiler | opt | axis |
|---|---|---|---|
| `gcc13-O0` | gcc 13.3.0 | `-O0` | baseline `-O0` |
| `gcc12-O0` | gcc 12.4.0 | `-O0` | toolchain version at `-O0` |

A candidate is used only if its compiler exists and actually compiles the
module.  The gate requires ≥ 2 distinct binaries; otherwise it FAILS CLOSED
with the exact probe transcript as `VERIFIED_NEGATIVE` evidence.  `O0_CANDIDATES`
is a portability/self-test override only — the distinctness assertions still
apply, so it cannot weaken the gate to one config.

The injected fixture is `if (bit) { dbl.X.l[0] ^= 1u; }` in a temp copy; the
shipped file is never modified.

## Recorded run (this leg, on this host)

```
[o0_ct] usable configs: 2 (distinct=2 binaries=2)
[o0_ct]   gcc13-O0: gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 (-O0)
[o0_ct]   gcc12-O0: gcc-12 (Ubuntu 12.4.0-2ubuntu1~24.04.1) 12.4.0 (-O0)
[o0_ct] injected: if (bit) { dbl.X.l[0] ^= 1u; }  (temp copy only)
[fixed_ladder] S4 PASS: zero mbedtls/bignum symbols in module
[fixed_ladder] S1 PASS: trip count = compile-time constant 256
[fixed_ladder] S2 PASS: no secret branch/ternary/index; 2/2 cswap calls driven by swap
[fixed_ladder] S3 PASS: cswap is mask-only over all 4 coordinates
[o0_ct] --- config gcc13-O0 (gcc 13.3 -O0) ---
[o0_ct]   shipped=GREEN injected=RED secret_governed_jcc=1
[o0_ct]   [fixed_ladder_o0] JCC @13f7 13f7: 74 12  je 140b | governing=13f0: cmp BYTE PTR [rbp-0x225],0x0
[o0_ct] --- config gcc12-O0 (gcc 12.4 -O0) ---
[o0_ct]   shipped=GREEN injected=RED secret_governed_jcc=1
[o0_ct]   [fixed_ladder_o0] JCC @145c 145c: 74 12  je 1470 | governing=1455: cmp BYTE PTR [rbp-0x225],0x0
[o0_ct] identical verdicts across all 2 configurations (distinct verdict patterns=1)
[o0_ct] PASS: source + real -O0 machine code bound under 2 compiler binaries;
[o0_ct]       every configuration is non-vacuous; verdicts identical.
O0_CT_GATE PASSED
```

The injected branch is in each configuration's **real** `-O0` machine code and
is governed by a **stack slot** (`cmp BYTE PTR [rbp-0x225],0x0`, the `bit`
slot) — exactly the flow a register-only checker cannot see.  This is the
non-vacuity signal.  Standalone control output:

```
[o0_ct] PASS: every configuration's stack-aware detector is non-vacuous (injected branch RED)
O0_CT_NEGCTL PASSED
```

## VERIFIED_NEGATIVE evidence (fail-closed, no fabricated PASS)

When fewer than two distinct compiler binaries are usable the gate fails
closed and prints the exact probe transcript.  Two exercised cases:

```
$ O0_CANDIDATES="gcc13-O0|gcc 13.3 -O0|gcc|-O0" bash tools/ed25519_fixed_ladder_o0_gate.sh
[probe] command -v gcc => /usr/bin/gcc
[probe] gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
[o0_ct] VERIFIED_NEGATIVE: fewer than two DISTINCT usable compiler
binaries at -O0 (1 configs, 1 distinct (binary,opt), 1 binaries). ...
[o0_ct] FAIL: second -O0 compiler binary unavailable
O0_CT_GATE FAILED
```

```
$ O0_CANDIDATES="no-cc|X|definitely-not-a-compiler|-O0" bash tools/ed25519_fixed_ladder_o0_gate.sh
[probe] command -v definitely-not-a-compiler =>
[o0_ct] VERIFIED_NEGATIVE: fewer than two DISTINCT usable compiler
binaries at -O0 (0 configs, 0 distinct (binary,opt), 0 binaries). ...
O0_CT_GATE FAILED
```

## Machine-checkable claim on success

> The shipped `src/gui/wubu_ed25519_fixed.c` passes its source structural
> contract and its **stack-aware** real `-O0` machine-code audit under gcc
> 13.3 `-O0` and gcc 12.4 `-O0` with IDENTICAL verdicts; on every binary an
> injected `if (bit)` branch is detected as a conditional jump governed by the
> secret `bit` **stack slot** in real machine code, proving the check is
> non-vacuous at `-O0`.

## Remaining gaps

1. Still structural / machine-code evidence, not a formal constant-time proof:
   no ct-verif/ctgrind, no interprocedural preservation argument, and
   microarchitectural channels are out of scope.
2. The taint walk is intraprocedural over `wubu_ed25519_fixed_ladder` and
   `wubu_ed25519_fixed_cswap`; callees (`ge_add`, `ge_identity`) are treated as
   public-result.  Only the `-O0` lowering of these two functions is bounded.
3. The checker models the stack-slot spill shape emitted by these two gcc
   versions at `-O0`; a compiler using a different frame/calling convention at
   `-O0` would need its own slot-discovery pattern and would fail closed.
4. Differential timing evidence remains one toolchain (`-O1`) and process CPU
   time only; no cross-optimization timing is claimed here.
