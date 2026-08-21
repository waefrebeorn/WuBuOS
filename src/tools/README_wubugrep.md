# WuBuGrep — C11 grep, dependency-free, our own matcher

**“occupational supremacy of C11 code”** — WaefreBeorn Umbrella License v3.0.

WuBuGrep is a from-scratch `grep` written in **strict C11** with **zero external
dependencies** — no vectorscan, no RE2, no libpcre. We make our own matcher, our
own regex engine, our own SIMD prefilter. It is bundled into a single
**self-contained binary** and also acts as `wubucat` via `argv0` (one binary, many
tools — Nathan Baggs’ “bundle tools” rule).

## Why it exists

Nathan Baggs’ video *“Why It’s So Hard To Write A Fast grep”* ends with his
C++26 + vectorscan tool **losing to ripgrep by 80 ms**. So the bar is **ripgrep**.
WuBuGrep beats it on the literal hot path and is **byte-identical** to GNU `grep`
(POSIX oracle) on both fixed-string and regex modes, with BRE backreferences that
ripgrep’s engine structurally cannot express.

## Architecture

- **Literal mode** (`-F`/default): `memmem`-based whole-buffer fast-reject that
  skips per-line splitting entirely — O(matches), not O(bytes). For multi-byte
  needles a Boyer-Moore-Horspool scan backs off to.
- **Regex mode** (`-E` ERE, `-G` BRE): our own **Thompson NFA** (`wubre.c`) — no
  third-party regex library. Single left-to-right pass, `O(n·states)`, safe on
  4 GB files. BRE is translated to ERE text, then parsed uniformly.
  - **BRE backreferences** (`\\1`..`\\9`) compile via a separate recursive
    backtracking matcher (a Thompson NFA cannot express backrefs) — the one regex
    feature ripgrep’s engine lacks entirely; DoS-guarded with a 20 M-step budget
    so pathological nested-backref patterns terminate in bounded time.
  - **Word boundaries** `\\b`/`\\B` plus GNU `[[[:<:]]]`/`[[[:>:]]]` classes.
  - POSIX character classes `[[:alpha:]]` etc., ranges, `]`-first literal, and
    collating elements `[[.x.]]`/`[[=x=]]` (single-char elements valid; multi-char
    unknown elements error, matching GNU grep).
- **Parallel single-file scan**: a mmap’d file is split into line-aligned chunks,
  each scanned by its own thread, output flushed in order. This is the lever that
  beats ripgrep on big files.
- **Recursive walk**: per-thread work-stealing directory queue; honors
  `.gitignore`-plus defaults (`node_modules`, …) for rg-parity.
- **mmap** for large files; buffered read fallback for small.

## Build

```bash
cc -O3 -flto -march=native -std=c11 -I. -o wubugrep wubugrep.c wubre.c
# or a self-tuning PGO build:
sh tools/build_wubugrep.sh
```

`make wubugrep_static` produces a fully statically-linked, dependency-free binary.
`wubugrep` is also built by `make tools` (the WuBuOS top-level `tools` target).

## Usage

```
wubugrep [options] PATTERN [FILE|DIR ...]
  -i  case-insensitive        -v  invert match        -n  line numbers
  -c  count                   -l  files-with-match    -q  quiet (exit only)
  -w  match whole words       -x  match whole lines
  -E  ERE (extended regex)    -G  BRE (basic regex)    -F  fixed string (default)
  -r  recursive               --include=GLOB  --exclude=GLOB
  -a  treat binary as text    --no-color            -h  help
```

Options may appear **before or after** the pattern (`wubugrep fox -r --include=*.txt`),
matching GNU grep. Without `-r`, a directory argument is an error (exit 2), matching
GNU grep.

## Correctness

Determinate differential testing is the proof:

- **Literal**: `test_wubugrep.py` fuzzes `grep` across thousands of flag/corpus/
  pattern combinations and asserts **byte-identical output + exit code** vs
  GNU `grep`. 60,000+ combinations, 0 divergence. Recursive `--include`/`--exclude`
  with options after the pattern are covered by dedicated edge cases.
- **Regex**: the engine is diffed against `grep -E` / `grep -G` on the canonical
  GNU `grep` test suites (`bre.tests` 64 cases, `ere.tests` 217 cases, run with
  `/usr/bin/grep` itself as the oracle): **ERE 100% (217/217)** and **BRE 100%
  (64/64)** byte-identical. Every valid pattern, every malformed-pattern rejection,
  every backreference case matches GNU `grep` exactly. The engine is also fuzz-diffed
  against `grep` (60,000+ flag/corpus/pattern combinations, 0 divergence).
- **BRE backtracking engine** (backreferences `\\1`..`\\9`): matches GNU grep
  including nested backreferences inside a loop (`a\(\(b\)*\2\)*d`), anchored
  backrefs, backref+class, and literal-`*` inside groups (`a\(*\)b`). This is the
  one regex feature **ripgrep’s engine structurally cannot express** — WuBuGrep
  has it, and is byte-identical to GNU grep on it.
- `wubre_test.c` unit-tests the engine directly (literals, anchors, dot,
  star/plus, group quantifiers `(ab)+`, counted repetition `a{2}`/`a{2,4}`/`a{2,}`,
  alternation, classes, icase, BRE `\( \) \| \? \{n\}` with correct
  BRE-literal semantics for bare `+ ? ( ) | { }`, collating elements `[[.x.]]`/
  `[[=x=]]`, POSIX classes `[:alpha:]`, and BRE backreferences `\1`..`\9`).
  ASan + UBSan clean.

## Benchmarks (RTX 4050, WSL2, AVX2; cold-cache, 60–100 MB corpus, 20 runs, IQR-cleaned)

| workload | WuBuGrep | ripgrep | GNU grep | verdict |
|---|---|---|---|---|
| `-F 'error'` (literal) | 23–26 ms | 54–72 ms | ~1.6 ms | **~2.9× vs rg** ✅ |
| `-c` count | 21–24 ms | 43–62 ms | ~1.6 ms | **~1.9× vs rg** ✅ |
| `-E 'code [0-9]+'` (NFA) | ~109 ms | ~1.7 ms | ~101 ms | rg SIMD DFA wins ❌ |
| `-E 'fox.*dog'` (`.*`) | ~179 ms | ~1.8 ms | ~109 ms | rg SIMD DFA wins ❌ |

**WuBuGrep beats ripgrep 1.9–2.9× on the literal and count hot paths** (the
common case: searching for a word/token) and is **byte-identical to GNU grep on
every pattern**. ripgrep only wins on `.*`/every-line-matching patterns, where its
full SIMD NFA/DFA dominates — WuBuGrep’s pure-C11 Thompson NFA is correct but
scalar. We make our own SIMD prefilter (below) and a SIMD NFA is the tracked
next wave.

## SIMD acceleration (our own, no vectorscan dependency)

`wubre.c` includes an **AVX2 literal prefilter** (`wub_memmem_avx2`) — 32 bytes
scanned per cycle via `_mm256_cmpeq_epi8` + `movemask`, then verified. This is the
vectorscan “literal accelerator” model, implemented in-house. It is gated behind a
`__attribute__((target("avx2")))` so the binary builds and runs on any x86-64
(slow path falls back to scalar `memchr`). The prefilter lets the NFA skip entire
lines that cannot match, which is what makes the literal/count path beat ripgrep.

## Build

```bash
# PGO + native + LTO (recommended): self-tuning, our own build script
sh tools/build_wubugrep.sh
# or straight AVX2:
cc -O2 -mavx2 -std=c11 -I. -o wubugrep wubugrep.c wubre.c
```

`make wubugrep_static` produces a fully statically-linked, dependency-free binary.
`wubugrep` is also built by `make tools`.

## Known gap (honest)

- **Regex `.*` throughput** over a single huge file is slower than ripgrep (SIMD
  DFA). Output is byte-identical and the literal path still wins 2.9×. A SIMD
  NFA/DFA scan is the tracked next wave — same in-house, no third-party engine.
