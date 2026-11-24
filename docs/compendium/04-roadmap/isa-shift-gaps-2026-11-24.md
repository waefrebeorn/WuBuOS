# ISA backend gaps found by executing ARM64 (2026-11-24)

## What changed first

`test_isa_driver` could only prove that a backend **emitted bytes** for a foreign
target. ARM64 could not run on x86-64, so it reported `COMPILE FAIL` for every
program and nobody saw it — because the encoder refused to encode on a foreign
host at all (`return -1; /* cannot compile ARM64 on this host */`), which is the
one thing a cross-compiler must never do.

QEMU's own history describes exactly this failure mode for its `tests/tcg`
directory:

> most developers never ran them. As the tests were hardly ever built inevitably
> a certain amount of bit-rot set in.
> — <https://www.qemu.org/2018/06/21/tcg-testing/>

QEMU's fix was to make cross-target tests runnable (cross compilers + docker
images). The cheap version: **qemu-user-static needs no root**, so a blob plus a
six-instruction stub is a runnable AArch64 program.

`tools/arm64_exec_gate.py` does that: `test_isa_driver` dumps each ARM64 blob
with the value it must compute, the gate wraps it in a minimal ELF64 (one
PT_LOAD, entry at the stub: `BL code; MOVZ x8,#93; SVC #0; RET`) and executes it.
The process exit status is `x0 & 0xFF`.

This is the difference between "the encoder produced something" and "the emitted
code computes the right answer" — and it found three real bugs immediately.

## The three bugs it found

1. **`mov x29, sp` / `mov sp, x29` were emitted through the register-form MOV.**
   In AArch64 the register MOV (`ORR Rd, XZR, Rm`) encodes register 31 as `XZR`;
   SP is only addressable in the ADD/SUB **immediate** forms. So the prologue
   produced `mov x29, xzr` and the epilogue `mov xzr, x29` — SP was never
   restored, the `ldp x29, x30, [sp], #16` that followed read 64 bytes below the
   saved frame and loaded garbage, and `ret` jumped into nothing. **SIGSEGV on
   every ARM64 machine**, invisible while the backend could not be executed.
   Fixed with `warm64_mov_sp` (which emits `add Rd, Rn, #0`).

2. **Every branch patch rewrote the opcode.** The patch loop emitted
   `0x54000000 | (imm19 << 5)` for *all* patched words whenever the offset fit in
   imm19 — including unconditional `b` (0x14000000). Every `b` silently became
   `b.eq` (cond = EQ), i.e. "jump only if Z is set", where Z is stale flags from
   whatever comparison ran last. Result: `eq(5,6)`, `lt(3,2)`, `gt(2,3)` and every
   other false comparison returned **1**. Fixed by preserving the original
   opcode: bits 31..26 == 0b010101 → B.cond (imm19 + cond), == 0b000101 → B
   (imm26), anything else → leave alone.

3. **The encoder refused to encode off-target** (above). Fixed: compile always
   works; `arm64_run` returns `WUBU_ISA_NO_EXEC` instead of executing AArch64
   bytes as x86-64.

## Gaps it then exposed, not yet fixed

Extending the battery with arithmetic (add/sub/mul/div/mod/and/or/xor/shl) passes
on every backend. The two shifts do not, and they are **not** in the battery yet
because a green gate must not hide them:

| op | fails on |
|---|---|
| `MIR_SHR` (arithmetic, 64-bit) | 8086, 6502, z80, mips, 8051, avr, pic |
| `MIR_SHR_U` (logical, 64-bit) | 8086, m68k, 6502, riscv, z80, mips, 8051, avr, pic |

Both are recent additions (see `wubu_mir.h`: SHR is `sar`, SHR_U is `shr`). Two
different fixes are needed and they must be told apart per backend:

- **implement** it (riscv, m68k, mips and 8086 all have a shift; `shru(16,2)`
  returning 4 should not be hard on any of them), or
- **declare** the capability limit, if a backend genuinely cannot do 64-bit
  arithmetic shifts, and make the driver say so instead of returning a wrong
  value.

The cases go back into the battery the moment each backend either implements the
op or refuses it explicitly. Until then, the gap is recorded here and the battery
stays green at what it can actually prove.

## Why this is the right shape

A gate that cannot run its target is not a gate. The three ways a test lies —
not building, not running, and running the wrong thing — were all present in this
one backend:

- **not building**: the encoder refused on a foreign host,
- **not running**: 31 checks were counted as failures because ARM64 "cannot
  execute here",
- **running the wrong thing**: `wubu_aligned_free` was an implicit declaration
  in a different TU of the same subsystem, and `ptx_run` returned `0` when
  nothing ran, which reads as a legitimate answer of zero.

`WUBU_ISA_NO_EXEC` exists so the third case cannot recur: a caller can always
tell "ran and produced X" from "did not run".
