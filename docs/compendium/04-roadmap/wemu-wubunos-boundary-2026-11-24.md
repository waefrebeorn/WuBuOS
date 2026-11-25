# WEMU and WuBuNOS: where the two models of the machine meet (2026-11-24)

They live in the same tree and they are two halves of one concern:

```
src/wemu/      WEMU      x86-64 EMULATOR   bytes -> WemuInsn -> execute
src/compiler/  WuBuNOS   COMPILER          MIR  -> bytes for 13 ISAs
```

`tools/cross_tree_overlap.py` inventories what both sides model, and
`test_cross_tree_overlap` fails if a NEW shared concern appears without a written
verdict. Today's map:

| concern | WEMU | WuBuNOS | verdict |
|---|---|---|---|
| x86 decode | `wemu_decode.c` (240 lines, semantic: fields + operands for execution) | `x86_decode.c` (130 lines, length/fields for its own tests) | **two decoders, two jobs.** Accepted. Drift is the risk — see "what would prove it" below |
| x87 / float | `wemu_x87.c` (x87 stack, 80-bit) | `wubu_softfloat.c` (f32/f64 in integer math) | **different targets.** Accepted |
| ELF | `wemu_elf.c` — LOADS a guest ELF | `holyd_elf.c`, `holyd_emit_elf.c` — EMITS an ELF | **complementary, and not yet wired together.** See below |

## The finding that came out of trying to run one in the other

The obvious cross-check — execute WuBuNOS's x86-64 output inside WEMU, the way the
ARM64/RISC-V/MIPS backends are executed under qemu — does not work, and the reason
is architectural, not a bug in either side:

```
wubu_isa_x86_64.c:694
    movabs rbx, &wubu_jit_mem_ptr      /* 48 BB imm64 */
```

The x86-64 backend **is** the in-tree JIT. It bakes the address of a HOST global
into the emitted bytes, because the memory base lives in the process that compiled
the code. That is correct for a JIT and fatal for an emulator: the bytes are not
relocatable.

`tools/wemu_blob_gate.c` proves it rather than assuming it — it loads each dumped
blob at a flat address, sets up a stack and an HLT return, runs it in WEMU, and
reports what it finds:

```
== 19 case(s) executed in WEMU, 0 failed, 19 host-bound ==
wemu_blob_gate: SKIP (19 blob(s) carry a baked host address -- the x86-64
  backend is a JIT: `movabs rbx, &wubu_jit_mem_ptr` points at a global in
  the compiling process, so the bytes are not relocatable and cannot run
  under an emulator.)
```

So the two sides answer different questions and cannot validate each other on
x86-64 **until one of them changes**:

* the backend could take the memory base from a register the caller sets (which
  would also make its output relocatable and therefore testable), or
* WEMU could be taught the JIT's convention by mapping the guest arena at the
  address baked in — possible, but it would test a layout rather than the code.

## What would prove each of the shared concerns

These are the gates that do not exist yet, in order of value:

1. **decode parity** — for the bytes WuBuNOS emits, `x86_decode_one`'s length must
   equal WEMU's decoded length. Cheap once a `WemuCpu` with a flat memory image is
   set up (the harness above does exactly that), and it would catch both decoders
   drifting on a prefix/REX/ModRM corner.
2. **emit -> load -> run** — CLOSED. `test_holyc_elf_in_wemu` compiles
   `tools/selfbuild_checksum.hc` (sums "WUBU", returns 323) with WuBuNOS's
   `holyd_emit_elf`, then executes the resulting ELF in WEMU through
   `tools/wemu_run_elf` and asserts the exit status is 67:

       test_holyc_elf_in_wemu: PASS (WEMU ran WuBuNOS's ELF; main()=323 -> exit 67)

   It found a real bug on the first run, which is the point:

       holyd_elf.c:  _start trampoline used  mov al, 60      (0xb0 0x3c)
                     it must use           mov eax, 60      (0xb8 0x3c 00 00 00)

   `mov al` replaces only the low byte, so main's return value stayed in bits
   8..63 and the syscall number became `(ret & ~0xFF) | 60`. main returning 0
   gives 60 (exit) and looks perfect; main returning 323 gives 316, which is not
   a syscall, and the process died with a guest fault. The trampoline grew
   12 -> 15 bytes and the call displacement is computed from its size, so the
   layout follows.

   Proven both ways: reverting the trampoline to `mov al,60` makes the gate
   report `FAIL (exit 125, want 67)`; restoring it returns PASS.
3. **x87 vs softfloat** — both implement floating point, at different widths, for
   different consumers. A shared corpus of values compared on add/sub/mul/div/cmp
   (allowing for the 80-bit vs f64 difference) would find the cases where one is
   wrong.

## The rule

A shared concern is not a defect. **Silent divergence** is. The audit freezes
today's map in `tools/cross_tree_overlap_baseline.json`; adding a fourth model of
something already modelled on the other side now fails a gate and asks for a
verdict in writing.
