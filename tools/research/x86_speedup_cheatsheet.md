# x86-64 Speedup Cheatsheet (2025-2026, Agner Fog + uops.info)

Microarchitectural targets: Intel Golden Cove (ARL-P) · AMD Zen 4/Zen 5

## LATENCY / THROUGHPUT REFERENCE TABLE

| Instruction           | Zen 4 lat | Zen 4 tp | Zen 4 ports | Intel ARL-P lat | ARL-P tp | ARL-P ports |
|-----------------------|-----------|----------|-------------|------------------|----------|-------------|
| `mov r, r`            | 0 (R)     | <0.33    | — (renamed) | 0 (R)  | <0.33  | —           |
| `mov r, [mem]` (load) | 4–5       | 0.25     | 0,1,2,3     | 4–5    | 0.33   | 0,1,2,3     |
| `mov [mem], r` (store)| 0 (SB)    | 0.5      | 4,STD/SA    | 0      | 0.5    | 4,STD/SA    |
| `lea r, [r+r*2+imm]`  | 1        | 0.25     | 0,1         | 1      | 0.33   | 0,1,5       |
| `imul r, r, imm`      | 3        | 1        | 0           | 3      | 1      | 0,1         |
| `imul r, r`           | 3–4      | 1        | 0           | 3      | 1      | 0,1         |
| `div r64`             | 18–22    | 18–22    | 9,10,14,15  | 21–28  | 21–28  | —           |
| `cvttsd2si rax,xmm0`  | 3–4      | 1        | 5,6         | 3      | 1      | 0,5         |
| `vfmadd132ps`         | 3–4      | 0.25     | 0,1 (FMA)   | 3–4    | 0.5    | 0,5 (V0,5)  |
| `vaddps`              | 1        | 0.25     | 0,1         | 1      | 0.5    | 0,5         |
| `cmovcc r, r`         | 1        | 2        | 0,1,5       | 1      | 2      | 0,1,5       |
| `pext/pext r,r,r`     | 7–18 (*) | 1        | 0,1,5       | 200+   | 200+   | —           |
| `pdep/pext imm` (const)| 3       | 1        | 0,1         | 3      | 1      | 0,1         |
| branch mispredict     | 15–20    | —        | —           | 15–20  | —      | —           |

(*) `pext`/`pdep` with variable bit-count is microcoded and **extremely**
slow (~200 cyc on some Intel). With an immediate, it's fast (1 µop, p01).

## KEY INSIGHTS (with WuBuOS HC-compiler implications)

### 1. Register-to-register moves are FREE
`mov rax, rdi` has zero latency — the CPU's register renamer handles it
in the rename stage, no execution port. **Implication for HC codegen**:
don't avoid `mov r,r` to "save a cycle"; it costs nothing. Prioritize
clarity and let the renamer work.

### 2. LEA for cheap arithmetic (the shift-and-add replacement for IMUL)
`lea rax, [rdi + rdi*2]` = `rdi * 3` in 1 cycle, 1 µop, no flags.
Multiply table (Zen 4 / Intel):
```
*2  →  lea  r, [r*2]          (or shl r, 1)
*3  →  lea  r, [r + r*2]
*4  →  lea  r, [r*4]          (or shl r, 2)
*5  →  lea  r, [r + r*4]
*8  →  lea  r, [r*8]          (or shl r, 3)
*9  →  lea  r, [r + r*8]
*10 →  lea  r, [r*8 + r*2]    ← cheaper than imul imm (imul r32,imm8 is OK)
*15 →  lea  r, [r*16 - r]     ← lea with -base trick
```
For constants > scale*base+index combos, use `imul r, imm` (3 cyc, 1 µop).
The crossover is ~3-scale LEA chains vs 1 IMUL.

**HC codegen action**: in `holyc_codegen_expr.c`, detect constant
multiplication `* N` where N is a shift/add combo and emit `lea` instead
of a general multiply.

### 3. Division avoidance — the `div` penalty
`div r64` is 18–28 cycles and **completely blocking** (no pipelining).
Replace `x / D` with `x * (1/D)` via **fixed-point reciprocal**:
```
q = ((uint64_t)x * ((uint64_t)MAGIC >> shift)) >> adj_shift
```
For power-of-2 D, use `shr rax, log2(D)` (1 cyc, 0.25 tp) instead of div.

**HC codegen action**: for `x / 2^k`, emit `shr` not `div`.

### 4. Branchless programming — kill the 15-cycle mispredict penalty
Replace `if (x > 0) a = b; else a = c;` with:
```
mov    rax, rdi        ; b
mov    rdx, rsi        ; c
test   rdi, rdi
cmovg  rax, rdx        ; rax = (x>0) ? c : b
```
Even better for select-by-mask: `x ? a : b` where a,b are computed
arithmetic can use `and`/`or` with sign-extension:
```
mov    rax, rdi
sar    rax, 63         ; rax = mask (0 or -1)
and    rax, (b - a)    ; mask * delta
add    rax, a          ; a or b
```
**1.5 cyc, 0 mispredict risk** vs 15 cyc mispredict.

`pext`/`pdep` are great for bitmask→index but **only with immediates**.

### 5. Port pressure / ILP — the Zen 4 vs Intel difference
- Zen 4: 2 integer ALUs (p0, p1), 2 AGU/load ports (p2, p3), 1 store (p4),
  FMA on p0/p1, ADD on p2/p3.
- Intel ARL-P (Golden Cove): ALU on p015, loads on p023, stores on p4/STO.
- **Zen 4 secret**: FMA and integer add use DIFFERENT ports (01 vs 23),
  so you can do FMA + ADD simultaneously — double the FP throughput
  by interleaving independent dependency chains across ports.
  (ashvardanian: "330 GB/s/core" AVX-512 kernel exploits this)

**HC codegen action**: emit independent arithmetic chains back-to-back
to maximize ILP — modern OoO cores will overlap them automatically.

### 6. Cache / memory hints
- `prefetcht0/prefetchnta` — software prefetch for predictable patterns
- `movntdqa` — non-temporal load (bypasses cache, good for large streaming)
- `prefetchw` — prefetches the write path (store buffer), 2-3 cyc early
- **DRAM hedge** (tailslayer): see `notes_tailslayer.md` — replicate data
  across DRAM channels

### 7. Division by constant via magic multiply (Granlund–Montgomery)
For `x / D` where D has no power-of-2 factorization, use the multiplier
`m = (2^k + D - 1) / D` and shift. The compiler can precompt this at
compile time and emit `imul rax, rax, m; shr rax, k`.

### 8. Branch hint opcodes (rarely useful)
`xbegin` (TSX) for speculative execution — deprecated on recent CPUs.
`jcc` with hint (0x3e = not-taken, 0x3f = taken) — modern predictors
ignore these; don't bother.

## VERIFIED ENCODINGS (used in HC JIT)
| Op              | Encoding | Notes |
|-----------------|----------|-------|
| `cvttsd2si rax,xmm0` | F2 48 0F 2C C0 | float→int round-toward-zero |
| `and rax, rdi`       | 48 21 F8          | ALU, p01/p23 |
| `or rax, rdi`        | 48 09 F8          | |
| `xor rax, rdi`       | 48 31 F8          | |
| `shl rax, rdi`       | 48 0F A5 C7       | variable shift |
| `div rdi`            | 48 0F F7 F7       | 18-28 cyc — AVOID |
| `mov rax, [rax+off]` | 48 8B 80 <disp32> | 8-byte load |
| `mov eax, [rax+off]` | 8B 80 <disp32>    | **4-byte zero-extending** (no REX) |
| `call rax` (indirect)| FF D0             | indirect call |
| `call rel32`         | E8 <disp32>       | relative (patch after copy) |
| `mov rax, imm64`     | 48 B8 <imm64>     | 64-bit immediate |

Source: Agner Fog instruction tables (2025-09-20) + uops.info Zen4/ARL-P.
