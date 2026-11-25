# The loop case: what straight-line programs cannot see

2026-11-24. Every ISA case before this one was straight-line: forward branches
only, each virtual register written exactly once. `test_isa_driver` passed
394/394 and `test_isa_exec` executed 38 blobs under qemu with 0 failures — and
that battery was blind to an entire class of bug.

The case added, `loop(1..4)`:

```
acc = 0; i = 1;
loop:  if (i > 4) goto done;
       acc = acc + i;  i = i + 1;
       goto loop;
done:  return acc;            -> 10
```

Two things it exercises that nothing else did: a **backward** label patch, and a
**virtual register re-assigned across iterations** (`wubu_mir_mov_to`).

## Result: 7 of 13 backends fail it, in four distinct ways

| backend | verdict | reason |
|---|---|---|
| x86-64 | PASS | — |
| 8086 | 21451 (want 10) | re-assignment across the back edge keeps a stale slot/register |
| m68k | 2380953 | same class as 8086 |
| 6502 | does not terminate | **no `MIR_JNZ` case** — the exit branch is never emitted |
| z80 | does not terminate | no `MIR_JNZ` case |
| riscv | does not terminate | `MIR_JNZ` added; the **forward** patch of that branch resolves to `pos+4`, i.e. the next instruction |
| mips | 0 | loop body never runs |
| 8051 / avr / pic | 1 | no `MIR_JZ` **and** no `MIR_JNZ` at all |
| arm64 | **5** (want 10) | register allocation clobbers the accumulator at the loop head |
| ptx | COMPILE FAIL | cannot compile the case at all |

## The bigger finding: the battery only tested backends that could compile

Every case in this file is shaped

```c
if (run_with_driver(d, &prog, &result) == 0) { CHECK(...); }
```

so a backend that could **not compile** a case was skipped from the tally entirely
and the run still said ALL TESTS PASSED. That is why arm64 "passed" the memory
selftest: it has **no `MIR_LOAD` case at all**, the store wrote nothing, the value
stayed in a register, and the register was compared against itself.

Now a hole is counted and must be declared:

    arm64: COMPILE FAIL -- MIR op 37 (MIR_LOAD) has no case
      GAP  arm64 does not implement MIR_LOAD: no load at all -- the memory
           selftest 'passed' because the store wrote nothing
    8051: GAP  the 8051 pseudo-VM has no jump at all   (same for avr, pic)

and any hole *not* in that declared list fails the run by name.

## The systemic finding: an unhandled MIR op is silent

`MIR_JNZ` is implemented by 4 of 13 backends (x86-64, arm64, ptx, spirv). In the
rest it falls through the compile switch's `default:` and **nothing is emitted** —
no error, no warning, no diagnostic. A loop assembled with `jnz` therefore runs
its body forever, and the only symptom is a hang.

That is the same shape as the shift bugs (`MIR_SHR_U` existing in one backend),
and it deserves the same treatment: **a backend must fail the compile on an op it
does not implement.** Falling through is how "unsupported" becomes "wrong answer"
or "hangs".

## arm64: acc clobbered at the back edge

Executed under qemu (so this is not a harness artifact). The emitted code opens
the loop body with:

```
24: mov x9, x10        <- vr9 (acc) overwritten with vr10 (i)
28: mov x10, x11
2c: cmp x9, x10
30: b.gt 0x3c
```

then compares and, later, `add x0, x9, x10`. The accumulator's home register is
overwritten at the top of every iteration, so the sum comes out as 5. Every
straight-line case passes because straight-line code never revisits an
instruction with a different register mapping.

## State

- `test_isa_driver`: 396/396 with 9 declared gaps (each with its measured
  symptom) and 2 SKIPs (arm64/ptx cannot execute on this host).
- `test_isa_exec`: 58 cases executed, 0 failed, with the arm64 loop listed as a
  `KNOWN FAIL` carrying the diagnosis above — printed every run, removed when the
  fix lands.
- `wubu_isa_riscv.c`: gained a real `MIR_JNZ` case (`bne`), which exposed the
  forward-patch bug underneath it.

## arm64 MIR_LOAD/MIR_STORE: what is actually missing (measured)

Not two missing switch cases. Three layers, and only the first is obvious:

1. **Encoder primitives exist after all -- and the first claim here was WRONG.**
   `warm64_ldr_reg`/`warm64_str_reg`/`warm64_ldr_imm`/`warm64_str_imm` are
   declared at wubu_arm64.h:109-112 with an `int sf` size flag. The grep that
   "proved" they were missing was run from `wubunos`, where `src/jit` is a
   SYMLINK: `grep -r` does not follow symlinks, so it reported an empty result
   for a header full of LDR/STR helpers. Lesson: a negative grep result across a
   symlinked path is not evidence -- add `-L` or run it in the real tree.
2. **No memory base reaches the code.** `arm64_run` calls
   `int64_t (*fn)(void)` and discards `arg`, while x86-64 keeps the base in rbx
   from the prologue. arm64 needs the base in a callee-saved register (x19),
   which means the run stub must call `fn(int64_t mem)` and the prologue must
   `mov x19, x0`.
3. **MIR_ALLOC is a no-op on both backends** (`break; /* home base already
   emitted as a CONST vr */`), so the address in `in->a` is a **byte offset**
   into the mem buffer, not a cell index -- no `lsl #3` scaling, same as x86.

Ordering that avoids a red tree: encoders (LDR/STR reg-offset + scaled imm) ->
exec-gate stub passes mem in x0 -> prologue `mov x19, x0` -> ALLOC no-op ->
LOAD `ldr x10, [x19, x11]` with x11 = the byte offset -> STORE symmetric.
The driver's memory case is the acceptance test; the arm64 loop already passes
under qemu-aarch64 (`test_isa_exec`: 60 executed, 0 failed).

## arm64 loop: FIXED -- the allocator was handing out the operand scratch

(Correction: an earlier version of this section claimed the back edge resolved
forward because `b 0x40` appeared at 0x38. That reading was WRONG -- 0x38's
branch is the conditional materialisation `x0 = a>b ? 1 : 0`, which is exactly
right. Reading a 20-instruction window instead of the whole blob is what
produced the false diagnosis.)

The real defect, from the full disassembly:

    28: aa0a03e9  mov x9, x10      ; every arithmetic case copies operands to x9/x10
    2c: aa0b03ea  mov x10, x11     ;   ... so the compare DESTROYED acc (x9) and i (x10)
    54: 8b0a0120  add x0, x9, x10  ; sum = i + four = 1 + 4 = 5

`phys_to_arm64()` was `8 + phys`, so the vreg pool was x8..x27 -- which contains
x9 and x10, the fixed operand scratch of every arithmetic op. Reserving them (and
x19, which holds prog.mem) in ONE pool table fixed it; the alternative, changing
the scratch register in 30 call sites, is the kind of fix that drifts with every
new op. arm64 now executes the loop under qemu-aarch64 and returns 10, and its
declared KNOWN_FAIL entry is deleted rather than left to mask the next one.

## (superseded) the false lead

`qemu-aarch64-static` runs the loop blob and answers 5 where 10 is wanted. The
disassembly of the emitted loop names two defects, and they are separate:

    0:  a9bf7bfd  stp  x29, x30, [sp, #-16]!
    4:  910003fd  mov  x29, sp
    8:  d10123ff  sub  sp, sp, #0x48
    c:  d2800000  mov  x0, #0          ; acc = 0 -> x9
   14:  d2800020  mov  x0, #1          ; i   = 1 -> x10
   1c:  d2800080  mov  x0, #4          ; four= 4 -> x11
   24:  aa0a03e9  mov  x9, x10         ; acc = i        <-- L_loop is HERE
   28:  aa0b03ea  mov  x10, x11        ; i   = four
   2c:  eb0a013f  cmp  x9, x10         ; compares acc,i -- MIR says i,four
   30:  5400006c  b.gt 0x3c
   38:  14000002  b    0x40            ; <-- should be `b 0x24`

1. ~~The label offset is wrong~~ -- **withdrawn.** `L_loop` IS at 0x24 and the
   0x38 branch is the cond materialisation, not the back edge. Nothing in the
   label or patch path is broken.
2. **Operands are shifted by one vr.** `MIR_GT(a=i, b=four)` emits `cmp x9, x10`
   (acc,i) instead of `cmp x10, x11` (i,four), and the two entry copies read
   `acc = i; i = four` where the MIR asks for `acc = sum; i = next`. The consts
   land in the right registers, so the vr->reg map is sound at the prologue; what
   disagrees is the *operand resolution inside the loop body*.

Because the back edge leaves the loop after one pass, the straight-line cases all
pass (20/20 under qemu) and only the loop fails -- the same shape as riscv,
6502, 8051, pic and avr: correct where the test looks, wrong at the back edge.

## Progress against that list

**1. DONE** — every backend's op-switch default now records the op
(`WUBU_MIR_UNSUPPORTED`), the driver refuses to run code compiled with a hole,
and an *undeclared* hole fails the run by name. It exposed arm64 having no
`MIR_LOAD`/`MIR_STORE` at all and 8051/avr/pic having no `MIR_JMP`.

**Also fixed on the way:** the battery's `if (run == 0) CHECK(...)` shape silently
dropped backends that could not compile a case; holes are now counted.

**New gate:** `test_gate_scripts` — `bash -n` plus `py_compile` of every embedded
Python block in `tools/*.sh`. A `nonlocal` in a top-level gate script was a
SyntaxError that `bash -n` cannot see, and it cost a full suite cycle by
presenting as a SCANOUT failure.

## Next, in order of value

1. ~~Make an unhandled MIR op a compile error in every backend~~ DONE.
2. `MIR_JNZ` for 6502, z80, mips, 8086, m68k; `MIR_JZ`/`MIR_JNZ` for 8051/avr/pic.
3. arm64 register allocation across a back edge — the accumulator must keep its
   home if it is live at the loop head.
4. riscv forward-patch offset.
5. Re-run `loop(1..4)` and delete the gap lines one by one.
