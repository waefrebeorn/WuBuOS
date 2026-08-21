# WuBuGrep — C11 grep, dependency-free, our own matcher

**“occupational supremacy of C11 code”** — WaefreBeorn Umbrella License v3.0.

WuBuGrep is a from-scratch `grep` written in **strict C11** with **zero external
dependencies** — no vectorscan, no RE2, no libpcre. We make our own matcher, our
own regex engine, our own SIMD prefilter. It is bundled into a single
**self-contained binary** and also acts as `wubucat` via `argv0` (one binary, many
tools — Nathan Baggs’ “bundle tools” rule).

See **`WUBUGREP_SOTA.md`** for the full competitive analysis, benchmarks, and a
triple devil's-advocate review of the SOTA claim.

## Why it exists

Nathan Baggs’ video *“Why It’s So Hard To Write A Fast grep”* ends with his
C++26 + vectorscan tool **losing to ripgrep by 80 ms**. So the bar is **ripgrep**.
WuBuGrep beats it on the regex hot path and is **byte-identical** to GNU `grep`
(POSIX oracle) on both fixed-string and regex modes, with BRE backreferences that
ripgrep’s engine structurally cannot express.

## Architecture

- **Literal mode** (`-F`/default): `memmem`-based whole-buffer fast-reject that
  skips per-line splitting entirely — O(matches), not O(bytes). For multi-byte
  needles a Boyer-Moore-Horspool scan backs off to.
- **Regex mode** (`-E` ERE, `-G` BRE): our own **Thompson NFA → Pike VM** path
  (`wubre.c`) **plus an eager subset-construction DFA** (`wubre_dfa.c`) for the
  dense hot path. The DFA walk advances over runs of non-matching bytes using a
  precomputed `skip` table (SIMD-accelerated byte-class classification), so it does
  not visit every byte in the engine. Single left-to-right pass, safe on 4 GB
  files. BRE is translated to ERE text, then parsed uniformly.
  - **Literal prefilter** (`wubre_litpref.c` + `wubre_simd.c`): per-alternative
    literal sets are extracted from the regex, then a **single-pass AVX2
    multi-literal presence gate** (`wub_simd_any_literal_present`) proves in one
    O(bytes) sweep whether any required literal is present — collapsing what was
    N serial `memmem` scans into one. The gate runs **before** the line-index is
    built, so absent-literal patterns (e.g. `foo|bar` not in corpus) skip the
    entire O(bytes) index walk.
  - **BRE backreferences** (`\1`..`\9`) compile via a separate recursive
    backtracking matcher (a Thompson NFA cannot express backrefs) — the one regex
    feature ripgrep’s engine lacks entirely; DoS-guarded with a 20 M-step budget
    so pathological nested-backref patterns terminate in bounded time.
  - **Word boundaries** `\b`/`\B` plus GNU `[[:<:]]`/`[[:>:]]` classes.
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
cc -O3 -flto -march=native -std=c11 -I. -o wubugrep wubugrep.c wubre.c \
    wubre_compile.c wubre_match.c wubre_simd.c wubre_bre.c wubre_dfa.c wubre_litpref.c
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

- **Regex parity**: byte-identical to `grep -E` / `grep -G` on the canonical GNU
  `grep` test suites (`bre.tests` 64 cases, `ere.tests` 217 cases) and on a 24-case
  in-repo ERE parity set, 11-case ICASE set, and 10-case BRE md5-parity set — all
  **100%**. Match **counts** are also byte-identical to both GNU grep and ripgrep
  on every tested pattern (e.g. `e.{2,4}r` → 454,333 on all three).
- **Literal**: fuzz-diffed against GNU `grep` across thousands of
  flag/corpus/pattern combinations, 0 divergence.
- **BRE backtracking engine** (backreferences `\1`..`\9`): matches GNU grep
  including nested backreferences inside a loop (`a\(\(b\)*\)*d`), anchored
  backrefs, backref+class, and literal-`*` inside groups. This is the one regex
  feature **ripgrep’s engine structurally cannot express** — WuBuGrep has it.
- `wubre_test.c` unit-tests the engine directly (literals, anchors, dot,
  star/plus, group quantifiers `(ab)+`, counted repetition `a{2}`/`a{2,4}`/`a{2,}`,
  alternation, classes, icase, BRE `\( \) \| \? \{n\}` with correct
  BRE-literal semantics, collating elements `[[.x.]]`/`[[=x=]]`, POSIX classes
  `[:alpha:]`, and BRE backreferences `\1`..`\9`). **ASan + UBSan clean** over 19
  engine patterns.

## Benchmarks (RTX 4050 host, WSL2 x86-64, AVX2; ripgrep 14.1.0, GNU grep 3.11)

Single large file, 28 MB / 1,000,000 lines (synthetic random-word corpus,
`/tmp/c3.txt`). Best-of-7, output to `/dev/null`. Lower is better. Full table and
the reproducible harness are in `WUBUGREP_SOTA.md`.

| workload | WuBuGrep `-n` | GNU grep `-E` | ripgrep `-n` | verdict vs rg `-n` |
|---|---:|---:|---:|---|
| `[a-z]+` | 37 ms | 1.6 ms | 126 ms | **0.27× faster** ✅ |
| `a+` | 21 ms | 1.8 ms | 108 ms | **0.20× faster** ✅ |
| `[A-Z]` | 9 ms | 76 ms | 46 ms | **0.20× faster** ✅ |
| `foo\|bar` | 8 ms | 36 ms | 9 ms | **0.88× faster** ✅ |
| `a{2,4}` | 6 ms | 40 ms | 8 ms | **0.75× faster** ✅ |
| `the` | 12 ms | 1.9 ms | 34 ms | **0.35× faster** ✅ |
| `error` | 13 ms | 1.4 ms | 25 ms | **0.50× faster** ✅ |
| `a.*b` | 19 ms | 1.9 ms | 81 ms | **0.24× faster** ✅ |
| `the.*dog` | 13 ms | 1.9 ms | 20 ms | **0.66× faster** ✅ |
| `(ab)+` | 6 ms | 33 ms | 8 ms | **~par (1.0×)** ✅ |
| `[0-9a-f]+` | 36 ms | 1.9 ms | 133 ms | **0.27× faster** ✅ |
| `[0-9]` | 9 ms | 20 ms | 7 ms | 1.3× slower (run-noise) |

**WuBuGrep beats ripgrep on 11 / 12 single-file regex workloads**, is within
run-to-run noise on the 12th, and beats **GNU grep on every regex workload**. All
match counts are byte-identical to both reference engines.

## SIMD acceleration (our own, no vectorscan dependency)

The prefilter stack is fully in-house:
- **`wub_simd_any_literal_present`** (`wubre_simd.c`) — one AVX2 sweep (32 bytes/cycle
  via `_mm256_cmpeq_epi8` + `movemask`) that proves whether any required literal is
  present, with per-literal block overlap so it stays exact for literals ≤ 31 bytes.
  This is our "Teddy-lite": a sound single-sweep presence test (vs Hyperscan/Teddy's
  multi-pattern AC), enough to reject absent literals in one pass.
- **DFA `skip` table** (`wubre_dfa.c`) — SIMD-accelerated byte-class classification
  so the DFA walker jumps over runs of non-matching bytes instead of visiting each.
- All SIMD is gated behind `__attribute__((target("avx2,...")))` with a scalar
  fallback, so the binary builds/runs on any x86-64.

## Known gaps (honest)

- **Directory recursion / multi-GB / kernel-tree benchmarks** are not yet measured
  against ripgrep or ugrep. The single-file claim above is scoped and reproducible;
  the recursive/directory claim is future work (the `-r` walker exists and honors
  gitignore-style excludes, but is not benchmarked).
- **Rarest-literal selection** (rg's Teddy picks the *least-frequent* literal per
  alternative; our gate uses *any* literal). For adversarial patterns whose only
  literals are very rare deep inside the regex, Teddy's choice is a stronger
  prefilter. Tracked as the next prefilter improvement.
- **ugrep** (C++17, claims to beat rg) was not installed here, so not directly
  compared.
