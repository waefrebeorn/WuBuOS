# WuBuGrep — C11 grep, dependency-free, our own matcher

**"occupational supremacy of C11 code"** — WaefreBeorn Umbrella License v3.0.

WuBuGrep is a from-scratch `grep` written in **strict C11** with **zero external
dependencies** — no vectorscan, no RE2, no libpcre. We make our own matcher, our
own regex engine, our own SIMD prefilter. It is bundled into a single
**self-contained binary** and also acts as `wubucat` via `argv0` (one binary, many
tools).

See **`WUBUGREP_SOTA.md`** for the full competitive analysis (fresh 4-way table
vs ripgrep / GNU grep / ugrep), the honesty ledger of retracted claims, and the
current triple devil's-advocate review.

## Why it exists

Nathan Baggs' video *"Why It's So Hard To Write A Fast grep"* ends with his
C++26 + vectorscan tool **losing to ripgrep by 80 ms**. So the bar is **ripgrep**
(and, measured, ugrep). WuBuGrep is **byte-exact** vs GNU `grep` on
literal/BRE/ERE/ICASE (verified by `gauntlet.sh`), beats ripgrep on dense
matching patterns, and owns a capability rg structurally lacks: **BRE
backreferences**. Honest speed position: behind ugrep overall, competitive with
rg on literals, ahead of rg on class-heavy matching and uppercase-reject.

## Architecture

- **Literal mode** (`-F`/default): `memmem`-based whole-buffer fast path that
  jumps to each needle occurrence — O(matches), not O(bytes) — with
  Boyer–Moore–Horspool backing the per-line fallback.
- **Regex mode** (`-E` ERE, `-G` BRE): our own **Thompson NFA → Pike VM**
  plus an eager **subset-construction DFA** (`wubre_dfa.c`) with a SIMD
  byte-class skip table so the walker jumps runs of non-matching bytes for any
  class. BRE is translated to ERE text, then parsed uniformly.
  - **Literal-set prefilter** (`wubre_litpref.c`): per-alternative required
    literal sets extracted at compile time (sound: only literals guaranteed to
    appear are required).
  - **SIMD presence gate** (`wub_simd_any_literal_present`): one AVX2 sweep
    proves whether any required literal is present; absent ⇒ sound reject
    before any line index is built.
  - **Fused scan** (`wub_simd_line_nul_lit_stats`): for single-alternative
    patterns, newline count + NUL detection + literal presence in ONE
    128-byte-block pass (ugrep's one-pass technique, native C11). Probes the
    **rarest** required literal (`wubre_litpref_rarest`, e.g. `the.*dog`
    probes `dog`).
  - **Lazy line index** (`rcb_build_index`): built only on the first actual
    match — reject patterns never pay the O(bytes) walk.
  - **BRE backreferences** (`\1`..`\9`) via a separate recursive backtracking
    matcher, DoS-guarded (20 M-step budget). The one regex feature ripgrep's
    engine cannot express.
  - Word boundaries `\b`/`\B`, POSIX classes, collating elements.
- **Parallel single-file scan**: mmap'd file split into line-aligned chunks,
  one thread each, output flushed in order.
- **Recursive walk**: sorted DFS, `.gitignore`-style excludes.

## Build

```bash
cc -O3 -flto -march=native -std=c11 -I. -o wubugrep wubugrep.c wubre.c \
    wubre_compile.c wubre_match.c wubre_simd.c wubre_bre.c wubre_dfa.c wubre_litpref.c
# or a self-tuning PGO build:
sh tools/build_wubugrep.sh
```

`make wubugrep_static` produces a fully static binary; `make tools` builds it
as part of WuBuOS.

## Usage

```
wubugrep [options] PATTERN [FILE|DIR ...]
  -i  case-insensitive        -v  invert match        -n  line numbers
  -c  count                   -l  files-with-match    -q  quiet (exit only)
  -w  match whole words       -x  match whole lines
  -E  ERE (extended regex)    -G  BRE (basic regex)   -F  fixed string (default)
  -r  recursive               --include=GLOB  --exclude=GLOB
  -a  treat binary as text    --no-color            -h  help
```

Options may appear before or after the pattern; a directory without `-r` is an
error (exit 2) — both matching GNU grep.

## Correctness (the proof is differential, not declarative)

- **`gauntlet.sh`** (in this directory): literal default/-F/-i/-n/-c × 4
  patterns, BRE × plain/`-n` × 10, ERE × plain/`-n` × 12, ICASE `-niE` × 4 —
  **ALL byte-exact (md5) vs GNU grep**. Extend this script whenever a new
  dispatch path lands; it is the regression gate that caught the literal-path
  `-n` off-by-one.
- Adversarial crafted corpus (rare literals, `zzzqqq.*the`-style fusions):
  byte-exact; this corpus is what exposed two fused-scan soundness bugs
  (tail-cursor reuse, prelude literal skip) — both fixed.
- 50-trial randomized scalar-oracle test on the fused scan; 2000-random-slice
  chunk-boundary check.
- `wubre_test.c` unit suite: ALL PASS. **ASan + UBSan clean** across engine
  patterns including the fused/rarest paths.
- Known divergence (tracked, pre-existing): GNU grep's malformed-pattern
  leniency in corner cases (unbalanced `)`, leading `*`, empty-alt `|`) is not
  fully replicated. See WUBUGREP_SOTA.md §6.

## Benchmarks

Fresh measured table (rev 3) lives in `WUBUGREP_SOTA.md`; regenerate with
`bash bench_fresh.sh` (needs `/tmp/c3.txt`, rg, and the ugrep study build).
Summary on the 28 MB / 1M-line corpus, count mode, best-of-3:

| class | WuBuGrep | ripgrep | ugrep 7.8.4 |
|---|---:|---:|---:|
| dense matching (`[a-z]+`, `a+`) | **18–35 ms** | 59–73 ms | 4–5 ms |
| simple literals (`the`, `error`) | 13–15 ms | 14–17 ms | 4–5 ms |
| uppercase reject (`[A-Z]`) | **9–10 ms** | 45 ms | 28–29 ms |
| alternation/quantifier reject | 12–27 ms | **8–9 ms** | 8–10 ms |

Honest: ugrep leads almost everywhere (its fixed cost is ~4 ms vs our ~13–27);
we beat rg on the first and third rows, tie on literals, lose on reject rows.
Run-to-run spread on our binary is up to ~30% on reject rows (two consecutive
full tables observed); ranges above reflect the observed spread, not best-case.

## SIMD acceleration (all ours, no vectorscan)

- `wub_simd_any_literal_present` — single-sweep multi-literal presence gate.
- `wub_simd_line_nul_stats` / `wub_simd_line_nul_lit_stats` — 128-byte-block
  newline count + NUL detect (+ literal presence in the fused variant).
- `wubre_litpref_rarest` — rarest-literal probe selection (Teddy/RSA idea).
- DFA skip table (`wubre_dfa.c`) — SIMD byte-class runs.
- All SIMD behind `__attribute__((target("avx2,...")))` with `force_align_arg_pointer`
  on cross-TU entry points (a missing one caused a real SIGSEGV — see skill
  notes) and scalar fallbacks everywhere.

## Known gaps (honest)

- Fixed per-file overhead (~13–20 ms) is above ugrep's (~4–12 ms) and rg's
  (~8 ms) — the tracked gap.
- ~~Malformed-pattern corner cases differ from GNU grep~~ **FIXED**: 100% on
  `ere.tests` (217/217) and `bre.tests` (64/64) plus the adversarial sweep.
- Directory recursion / multi-GB / kernel-tree benchmarks unmeasured.
- Rarest-literal heuristic uses a static English frequency prior, not corpus
  frequencies.
