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

## Next, in order of value

1. Make an unhandled MIR op a compile error in every backend (mechanical, kills
   the whole silent class).
2. `MIR_JNZ` for 6502, z80, mips, 8086, m68k; `MIR_JZ`/`MIR_JNZ` for 8051/avr/pic.
3. arm64 register allocation across a back edge — the accumulator must keep its
   home if it is live at the loop head.
4. riscv forward-patch offset.
5. Re-run `loop(1..4)` and delete the gap lines one by one.
