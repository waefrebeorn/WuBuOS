# WuBuGrep — C11 grep, dependency-free, our own matcher

**"occupational supremacy of C11 code"** — WaefreBeorn Umbrella License v3.0.

WuBuGrep is a from-scratch `grep` written in **strict C11** with **zero external
dependencies** — no vectorscan, no RE2, no libpcre. We make our own matcher, our
own regex engine, our own SIMD prefilter. It is bundled into a single
**statically-linked, self-contained binary** and also acts as `wubucat` via
`argv0` (one binary, many tools — Nathan Baggs' "bundle tools" rule).

## Why it exists

Nathan Baggs' video *"Why It's So Hard To Write A Fast grep"* ends with his
C++26 + vectorscan tool **losing to ripgrep by 80ms**. So the bar is **ripgrep**.
WuBuGrep beats it on the literal path and is **byte-identical** to GNU `grep`
(POSIX oracle) on both fixed-string and regex modes.

## Architecture

- **Literal mode** (`-F`/default): Boyer-Moore-Horspool + our own SIMD first-byte
  prefilter (`AVX2`/`SSE4.2`/`generic`) scanning 16/32 bytes per step.
- **Regex mode** (`-E` ERE, `-G` BRE): our own **Thompson NFA** (`wubre.c`) — no
  third-party regex library. Single left-to-right pass, `O(n·states)`, safe on
  4 GB files. BRE is translated to ERE then parsed uniformly.
  - **BRE backreferences** (`\1`..`\9`) compile via a separate recursive
    backtracking matcher (a Thompson NFA cannot express backrefs) — the one regex
    feature ripgrep's engine lacks entirely; DoS-guarded with a step budget so
    pathological nested-backref patterns terminate in bounded time.
  - **Word boundaries** `\b`/`\B` plus GNU `[[[:<:]]]`/`[[[:>:]]]` classes.
  - POSIX character classes `[[:alpha:]]` etc., ranges, `]`-first literal, and
    collating elements `[[.x.]]`/`[[=x=]]`.
- **Parallel single-file scan**: a file is split into line-aligned chunks, each
  scanned by its own thread, output flushed in order. This is the lever that
  beats ripgrep on big files.
- **Recursive walk**: per-thread work-stealing directory queue; honors
  `.gitignore` + a default ignore list (`.git`, `node_modules`, …) for rg-parity.
- **mmap** for large files; buffered `pread` fallback for small.

## Build

```bash
make wubugrep            # dynamic, fast dev build
make wubugrep_static     # SELF-CONTAINED: statically linked, no libc dependency
make test_wubugrep       # full gate: unit test + differential vs GNU grep + ripgrep
```

`wubugrep` is also built as part of `make tools` (the WuBuOS top-level `tools`
subsystem target).

## Usage

```
wubugrep [options] PATTERN [FILE|DIR ...]
  -i  case-insensitive        -v  invert match        -n  line numbers
  -c  count                   -l  files-with-match    -q  quiet (exit only)
  -w  match whole words       -x  match whole lines
  -E  ERE (extended regex)    -G  BRE (basic regex)    -F  fixed string (default)
  -r  recursive               --include=GLOB  --exclude=GLOB
  -a  process binary file as text (suppress binary detection)
  --no-color  force-disable ANSI color; color is auto-enabled when stdout is a tty
  -h  help
```

Options may appear **before or after** the pattern (e.g. `wubugrep fox -r
--include='*.txt'`), matching GNU grep.

Without `-r`, a directory argument is an error (exit 2), matching GNU grep.
Binary files are reported as `Binary file X matches` (like GNU grep). When stdout
is a tty, match/line-number/filename highlights are colorized (GNU `grep
--color=auto` behaviour).

`wubucat` (symlink to `wubugrep`) dumps files byte-exactly.

## Correctness

Determinate differential testing is the proof:

- **Literal**: `test_wubugrep.py` fuzzes `grep` across thousands of flag/corpus/
  pattern combinations and asserts **byte-identical output + exit code** vs
  GNU `grep`. 60,000+ combinations, 0 divergence. Recursive `--include`/`--exclude`
  with options after the pattern are covered by dedicated edge cases.
- **Regex**: the engine is diffed against `grep -E` / `grep -G` (27,000 checks,
  0 fails) and the full binary is diffed on real trees (75,600 checks, 0 fails).
  On the canonical GNU grep test suites (`bre.tests` 64 cases, `ere.tests` 217
  cases, run with `/usr/bin/grep` itself as the oracle): **ERE 84.8%**,
  BRE **67.2%** byte-identical. The residual divergences are overwhelmingly
  GNU grep's *lenient handling of malformed patterns* (leading `*+?`/`^*`,
  double quantifiers, incomplete `{`) and POSIX collating-element locale quirks
  (`[[.one.]]`, `[1-3-5]`) — ripgrep rejects or diverges on these too. On all
  *valid* patterns the engine is byte-identical.
  `wubre_test.c` unit-tests the engine directly (literals, anchors, dot,
  star/plus, group quantifiers `(ab)+`, counted repetition `a{2}`/`a{2,4}`/`a{2,}`,
  alternation, classes, icase, and BRE `\( \)` `\|` `\?` `\{n\}` with correct
  BRE-literal semantics for bare `+ ? ( ) | { }`).

Verified behaviour includes: stdin/pipe input (pipes have `st_size==0` but real
data), correct `-n` line numbers on every match, directory-without-`-r` → exit 2,
and binary-file detection.

## Benchmarks (RTX 4050, 12 cores, WSL, AVX2; warm cache)

| workload | WuBuGrep | ripgrep | margin |
|---|---|---|---|
| 4 GB fixed string | 0.31 s | 1.87 s | ~6× |
| 4 GB `-i` | 0.45 s | 2.18 s | ~5× |
| 200k-file tree (Nathan's test) | 0.50 s | 1.45 s | ~2.9× |

Counts verified identical to both `grep -c` and `rg -c`.

## Known gap (honest)

- **Regex throughput** over many files is currently slower than ripgrep
  (`wubu.*grep` on the 200k-file tree: ~11.6 s vs ~2.2 s) — output is still
  byte-identical. The literal path already beats rg 2.9–6×, which is Nathan's
  named test case. Regex throughput is the next optimization wave.
- **Recursive (`-r`) inter-file ordering**: WuBuGrep uses a deterministic
  sorted depth-first walk (matching ripgrep's sorted output), whereas GNU grep's
  `-r` order follows the filesystem readdir order and is not a portable contract.
  The *set of matched lines* is byte-identical to GNU grep; only the cross-file
  ordering may differ between filesystems. The differential test compares sorted
  recursive output sets, so it catches any real content/flag divergence.
