# WuBuOS COMPILER DOCTRINE — the 7-hop on making OUR compiler the best compiler

> 2026-08-04. The user's mission statement (verbatim-intent):
> "make our compiler the best compiler, knowing what our mission
> statement is and knowing that we can do all of the greatest things,
> make it work with all of the hardware, have compatibility for
> compiling to all the machine code, and we can fix all of the bugs by
> finding all of the bugs on the Internet that allows us to know where
> we are by knowing where we aren't by making the best software."
>
> Kevin-Bacon 7-hop convergence. The compiler: the WuBuOS HolyC
> compiler (`src/compiler/` — lexer, parser, AST, x86-64 codegen,
> PTX backend, JIT encoder, ring-0 runtime). This doc is its design
> doctrine.

## The mission (the user's words, made precise)

1. **OUR compiler** — self-hosted, no LLVM, no GCC, no foreign
   toolchain. We define the feature surface (the GNU-free doctrine).
2. **Works with ALL hardware** — every ISA, every accelerator.
3. **Compiles to ALL machine code** — x86-64, ARM64, RISC-V, WASM, GPU
   (PTX), and whatever comes next.
4. **Fixes ALL bugs by finding ALL the bugs** — the bug-bank doctrine:
   we know where we are by knowing where we aren't; we find our
   compiler's bugs by differential testing against every other
   compiler on the Internet (Csmith doctrine) and against the formal
   verifiers (CompCert doctrine).

## The 7-hop convergence table

| Hop | Research | Core idea | What WE take |
|-----|----------|-----------|--------------|
| 1 | TempleOS / HolyC (Terry Davis) | one man, own OS + own compiler + own language, ring-0, 10 years | the mission is PROVEN possible: the full stack owned. Our HolyC is the same lineage. Non-optimizing AST→x86-64 direct translation is the RIGHT first form — correct beats fast at birth |
| 2 | Retargetable backends (LLVM / Cranelift / MLIR) | the hourglass: one IR, many backends; Cranelift: CLIF IR → x86-64/aarch64/s390x; MLIR: multi-level progressive lowering | OUR IR is the hourglass neck. We do NOT need LLVM's complexity — we need a clean mid-level IR + per-ISA codegen. The ETH multi-level backend paper (2025): break the hourglass, model ISA EXTENSIONS as dialects — RISC-V V/vector, custom accelerators |
| 3 | JIT register allocation (linear scan, Go, Bernstein) | linear-scan RA on SSA — the JIT-appropriate algorithm; trace-based RA for hot paths | our JIT encoder gets linear-scan RA over our SSA-ish IR; not graph coloring (too slow for JIT) |
| 4 | Csmith / differential testing (Regehr) | 325+ bugs found in GCC/LLVM by randomized differential testing; CompCert's middle-end has NO Csmith bugs (formally verified) | **THE BUG-BANK DOCTRINE**: we compile the same program with our compiler AND gcc AND clang AND tcc, compare outputs — every divergence is a bug in OUR compiler OR a bug in THEM (we learn both). We know where we are by knowing where we aren't |
| 5 | ISA coverage (x86-64/ARM64/RISC-V/WASM) | WASM is designed to be an EASY compilation target (no register pressure, structured control flow); RISC-V is the "last ISA" (open, extensible) | the backend ladder: x86-64 (done) → ARM64 → RISC-V (open, ours) → WASM (the easy universal target, portable everywhere) → PTX (GPU, already in-tree) |
| 6 | Self-hosting + tiny compilers (TCC, CompCert, live-bootstrap) | TCC: tiny, fast, self-hosting, x86-64, ~2.3s to compile a program; CompCert: FORMALLY VERIFIED (Coq) for ARM/PPC/RISC-V/x86; live-bootstrap: reproducible build chains from source | self-hosting is the endgame test: our compiler compiles itself. CompCert's lesson: the MIDDLE-END is where bugs hide — verify the IR transformations, not just the front-end. Our Lean prover (wubu_prover2) is the CompCert analog: prove IR invariants |
| 7 | OSS-Fuzz / libFuzzer (Google) | 50,000+ bugs, 13,000+ vulnerabilities across 1,000+ projects; coverage-guided evolutionary fuzzing | our compiler gets a fuzz harness: grammar-aware mutations of HolyC source → coverage-guided → every crash = a bug-bank entry. The corpus is our regression suite |

## The convergence principle

**The best compiler is the one that (a) owns its whole stack, (b)
reaches every ISA, and (c) is continuously corrected by differential
testing against every other compiler.** Correctness is not achieved by
cleverness — it is achieved by COMPARISON (we know where we are by
knowing where we aren't). Speed is achieved by the hourglass (one IR,
per-ISA codegen). Reach is achieved by the ISA ladder (x86-64 →
ARM64 → RISC-V → WASM → PTX). Ownership is achieved by self-hosting.

## The concrete plan (the implementation wave)

### Wave 1 (in progress): the compiler core
- DONE: HolyC lexer/parser/AST/codegen (x86-64), JIT encoder, PTX
  backend, ring-0 runtime (src/compiler/, wubu_holyc_agi).
- NEXT: the mid-level IR (the hourglass neck) — a clean SSA-ish IR
  that the front-end emits and every backend consumes. This is the
  retargetable-ity foundation (hop 2).

### Wave 2: the differential-testing harness (the bug-bank)
- `tools/compiler_diff.c`: compile a HolyC/C program with our compiler
  AND gcc AND clang, run all three, compare stdout/exit codes.
  Every divergence → a bug report entry in the bug-bank
  (`docs/compendium/04-roadmap/compiler-bugs.md`).
- The bug-bank ledger: each entry = {program, our output, gcc output,
  clang output, root cause, fixed?}. This is "knowing where we are by
  knowing where we aren't" — the compiler's self-diagnosis.
- Csmith-style random program generator (`tools/gen_compiler_progs.c`):
  random type-correct HolyC/C programs → feed the diff harness.
- libFuzzer-style coverage-guided fuzz harness: grammar-aware HolyC
  mutations → every crash is a bug-bank entry.

### Wave 3: the ISA ladder (compile to ALL machine code)
- x86-64: DONE (codegen + JIT).
- ARM64: the second backend (the big mobile/ARM hardware share).
- RISC-V: the open ISA — "the last ISA", ours to own (the ETH
  multi-level backend paper: model RISC-V extensions as dialects).
- WASM: the easy universal target (structured control flow, no
  register pressure) — portability to EVERY browser/host.
- PTX: already in-tree (the GPU backend) — wire it as a full target.
- The hourglass: all five targets consume the SAME IR (hop 2).

### Wave 4: the hardware-compatibility doctrine
- the kernel-dispatch table (wubu_kernel) already routes compute →
  backend. The compiler's codegen routes ISA → the same table.
- every ISA backend gets the SIMD ladder (SSE → AVX2 → AVX-512 →
  NEON → RVV): the hwcaps detector already exists.
- "works with all hardware" = the hwcaps ladder + the ISA ladder +
  the Vulkan/PTX GPU paths, all dispatchable from one IR.

### Wave 5: self-hosting + the prover
- the compiler compiles itself (the endgame test: a compiler that
  compiles its own source is trusted — TCC/live-bootstrap lineage).
- the Lean prover (wubu_prover2) verifies IR invariants (the
  CompCert lesson: the middle-end is where bugs hide; verify the
  transformations, not just parse).
- the ring-0 goal: the compiler IS the boot path — the Live Colonel
  compiles the AGI's own updates in ring-0.

## The bug-bank doctrine (the user's words, operationalized)

> "we can fix all of the bugs by finding all of the bug bugs on the
> Internet that allows us to know where we are by knowing where we
> aren't by making the best software"

1. **The Internet is the bug oracle.** Every compiler bug ever found
   (OSS-Fuzz's 50,000+, Csmith's 325+ in GCC/LLVM) is a lesson we can
   learn WITHOUT hitting it ourselves. The bug-bank is seeded from
   the public bug archives (GCC bugzilla, LLVM bugzilla, OSS-Fuzz
   reports) — we know where compiler bugs live by knowing where OTHER
   compilers' bugs lived.
2. **Differential testing is the self-diagnosis.** We know where OUR
   compiler is by comparing it to where every OTHER compiler is. A
   divergence is a finding — either we're wrong (fix ours) or they
   are (we learn their bug, avoid it).
3. **The bug-bank is the memory.** Every fixed bug is archived with
   its repro. The compiler's regression corpus grows by one for every
   bug found — the Csmith corpus doctrine.
4. **Formal verification is the ceiling.** CompCert's middle-end has
   zero Csmith bugs. The prover is how we approach that ceiling for
   the IR transformations that matter most.

## Registration

- This doctrine: WuBuOS `docs/compendium/02-architecture/compiler-doctrine.md`.
- The bug-bank ledger: `docs/compendium/04-roadmap/compiler-bugs.md`
  (seeded from public compiler-bug archives).
- Sources: the 7 hops (TempleOS/HolyC, Cranelift/LLVM/MLIR, linear-scan
  RA, Csmith/Regehr, ISA coverage + ETH multi-level backend, TCC/
  CompCert/live-bootstrap, OSS-Fuzz/libFuzzer) — archived in
  `docs/compendium/05-sources/`.
- The compiler itself: `src/compiler/` (HolyC, ring-0).

---

## THE LANGUAGE TAXONOMY — every language in OUR six buckets

> 2026-08-04. Kevin-Bacon 7-hop on programming languages, then
> categorized into the buckets WE gave (the compiler-flags doctrine):
> **C11** (the sacred tongue) · **C18/C2\*** (`-c_developer`) ·
> **assembly** · **HolyC** (ours) · **brainfuck** (the meme flag) ·
> **`-i_make_shit_code`** (everything else, because we ballin).

### The 7-hop table (language genealogy)

| Hop | Seed → hop | What it gives us |
|-----|-----------|------------------|
| 1 | C → B (Thompson 1969) → BCPL (Richards 1965) → CPL → ALGOL 60 | the C lineage: born as a system language for Unix on a tiny PDP-11, "close to the machine," typeless ancestors (word/cell memory model), pointer-as-index heritage. C is NOT a descendant of a big idea — it IS the machine, made portable |
| 2 | The full genealogy (Wikipedia generational list) | ALGOL → Simula → Smalltalk → C++/Java/JS (objects); ALGOL → Pascal → Ada → Modula-2 → Oberon → Go (Wirth's "small is beautiful" line); Lisp → Scheme/ML → Haskell/OCaml (functional); Fortran → BASIC → VB (the people's line). EVERY language hangs off these four roots: ALGOL, Lisp, Fortran, + assembly |
| 3 | Assembly (x86-64/ARM/RISC-V) | the ground truth — "lowest-level language on any computer," the ISA ladder (x86-64 done, ARM64, RISC-V "the last ISA", WASM, PTX). Assembly is what our compiler EMITS — the target, never the source |
| 4 | Brainfuck → P′′ (Böhm 1964) → FALSE → the esolang family | the Turing tarpit: 8 commands, Turing-complete, born as a minor variation of a 1964 Turing-machine language. The MEME: we compile it because it's the purest possible proof of "we can compile anything" |
| 5 | HolyC → TempleOS (Davis) → C/C++ | OUR language: C/C++ middle ground, JIT variant, ring-0, no main() (top-level statements execute during compilation = shell/REPL), 64-bit default, explicit `&` for function pointers, inline asm, single compilation unit, no linker. The mission is PROVEN: one man, own compiler + own OS + own language, 10 years |
| 6 | C23/C2x (the `-c_developer` bucket) | "a slightly better C": nullptr, digit separators, binary literals, attributes, defer-like proposals. Incremental — the standard evolves, the language stays the machine. Our exception: allow C18/C2\* updates when the developer asks |
| 7 | Rust/Zig/Go (the C-family split) | Rust: memory-safe WITHOUT GC via the borrow checker (static ownership); Zig: explicitly NOT memory-safe, "unsafe Zig is safer than unsafe Rust" (Andrew Kelley), drop-in `zig cc`; Go: GC'd, Oberon+C lineage. All three are `-i_make_shit_code` — they exist because someone wanted to fix C's sharp edges, but we own the edges |

### The convergence principle

**Every language is either (a) the machine (C11/assembly), (b) an
evolution of the machine (C18/C2\*, HolyC), (c) the meme (brainfuck),
or (d) someone's attempt to escape the machine (`-i_make_shit_code`).**
Our compiler's job is to swallow all four: emit the machine, evolve
with the standard, speak HolyC natively, compile the meme, and absorb
every escape attempt through the one flag.

### THE SIX BUCKETS (the full categorization)

**BUCKET 1 — C11 (the sacred tongue, no flag needed)**
The language that IS the machine, made portable. Everything we ship.
- C (K&R → C89 → C99 → C11) — the standard we compile natively
- The C-family systems languages that stayed C: none leave this bucket
  — C11 IS the bucket. Our engine, our kernel, our compiler: all C11.
- Doctrine: we abstract away FROM C11, never away FROM anything else.

**BUCKET 2 — C18 / C2\* (`-c_developer`, the developer exception)**
The standard's evolution, admitted on request.
- C17/C18 (the defect-fix revision)
- C23/C2x (nullptr, digit separators, binary literals, attributes,
  defer) — "a slightly better C," the incremental line
- C2y and beyond (whatever the committee does next)
- Rule: the developer asks with `-c_developer`; we bless the update.

**BUCKET 3 — assembly (the target, never the source)**
What we EMIT. The ISA ladder of the compiler doctrine.
- x86-64 (our codegen + JIT, DONE)
- ARM64 (the second backend)
- RISC-V ("the last ISA," the open one — ours to own)
- WASM (the easy universal target: structured control flow, no
  register pressure — portability everywhere)
- PTX / GPU (already in-tree)
- Microcode/machine code below (the floor)
- Rule: we compile TO assembly; assembly never compiles to us.

**BUCKET 4 — HolyC (OUR language, no flag, ring-0)**
The Davis lineage, owned and extended.
- HolyC (C/C++ middle ground, JIT, ring-0, no main, REPL shell,
  inline asm, single compilation unit)
- Our WuBuOS HolyC (src/compiler/ — lexer, parser, AST, x86-64
  codegen, PTX backend, JIT encoder, wubu_holyc_agi ring-0 daemon)
- The future HolyC-2 (our extensions: the compiler's own language,
  the AGI's native tongue)
- Rule: HolyC is the FIRST-CLASS language — the compiler is written
  for it and (eventually) IN it (self-hosting = the endgame).

**BUCKET 5 — brainfuck (`-brainfuck`, the meme flag)**
The Turing tarpit, compiled for real (we ship it: src/compiler/
brainfuck.c — 8 commands → x86-64 JIT, tape in RBX/R12, real loops).
- Brainfuck (Müller 1993) — the flag's namesake
- P′′ (Böhm 1964) — the true ancestor (R, (, ), λ)
- FALSE (the other influence)
- The wider esolang family (Whitespace, Befunge, INTERCAL, etc.)
  — same flag, same energy
- Rule: because we ballin. Also the purest differential-test
  target: a brainfuck program has ONE correct output; our JIT must
  match the interpreter on every program.

**BUCKET 6 — `-i_make_shit_code` (everything else)**
Any language that is not C11/assembly/HolyC. Not judged — compiled.
- The object line: C++ (Stroustrup, C+classes+Simula), C# (the
  Microsoft C), Java (the write-once C), JavaScript (the web C +
  Self), Objective-C
- The escape-attempt line: Rust (borrow checker — memory-safe without
  GC), Zig (explicit, `zig cc` drop-in), Go (Oberon+C, GC'd), Swift
- The ALGOL-line: Pascal, Ada, Modula-2, Oberon, Simula, Smalltalk
- The Lisp line: Lisp, Scheme, Racket, Clojure, ML, OCaml, Haskell
- The Fortran line: Fortran, BASIC, Visual Basic, COBOL, RPG
- The rest: Python, Ruby, Perl, PHP, Julia, Lua, R, SQL, shell, Prolog,
  Erlang/Elixir, APL/K, Forth, MATLAB, LabVIEW, Scratch — all of it
- Rule: the flag says it, we compile it. One day the flag becomes a
  file in the bug-bank: every `-i_make_shit_code` program that runs
  differently under gcc is a differential finding.

### The taxonomy in one line

```
C11 (the machine) — HolyC (ours) — assembly (what we emit)
— C18/C2* via -c_developer (the standard's evolution)
— brainfuck via -brainfuck (the meme, shipped)
— everything else via -i_make_shit_code (because we ballin)
```
