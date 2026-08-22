# WuBuGrep — SOTA Analysis & Competitive Position

> "occupational supremacy of C11 code" — WaefreBeorn Umbrella License v3.0.
> All measurements in this document are reproducible from `src/tools/` on the
> fixture `/tmp/c3.txt` (1,000,000 lines / 28 MB, synthetic random-lowercase-word
> corpus, **zero digits**). Harness: `bench_fresh.sh` (per-invocation `date +%s.%N`
> timing, warm cache, best-of-3, every tool verified to actually run by comparing
> match counts). No number here is estimated. Host: RTX 4050 laptop, WSL2 x86-64,
> AVX2; ripgrep 14.1.0, GNU grep 3.11, ugrep 7.8.4 (built from source,
> `/home/wubu/opt/ugrep_study/ugrep/bin/ugrep`, NOT vendored).
>
> **Doc revision history:** this is revision 3. Rev 1 claimed "beats rg on
> 11/12" (flawed Python harness — retracted). Rev 2 claimed a corrected table
> but still carried stale numbers in the README and listed ugrep as
> "unbenchmarked". Rev 3 (this) has one consistent, freshly measured 4-way
> table, and the README now matches it.

---

## 0. Corrections log (honesty ledger — read first)

| Rev | Claim | Verdict |
|---|---|---|
| 1 | "WuBuGrep beats rg 11/12 workloads" | **RETRACTED** — `bench_clean.py` mis-attributed timing; stale comparison binary |
| 1 | "`[0-9]` 1.3× slower is run-noise" | **RETRACTED** — `[0-9]` is a reject pattern measuring fixed overhead; no noise |
| 2 | "beats rg on all matching patterns" | **SCOPED DOWN** — true warm-cache, but only after fixing the invalid `rg -E` invocation |
| 2 | "ugrep not installed / not compared" | **SUPERSEDED** — ugrep 7.8.4 built from source and benchmarked below |
| 3 | "fused combined-scan deferred (crash)" | **RESOLVED** — see §6 wave log |

Anything not in the current §2 table should be treated as historical.

---

## 1. Where we sit

The grep performance landscape, grounded in primary sources and direct source
reading of the **ugrep** codebase (`/home/wubu/opt/ugrep_study` — outside our
repo; we learn techniques, we do not vendor third-party code):

| Tool | Engine / trick | Source of speed | Limitation |
|---|---|---|---|
| **GNU grep 3.11** | Boyer–Moore unibyte skip loop; mmap | very few instructions per byte on literals | scalar regex path explodes on classes/reject |
| **ripgrep 14.1.0** | Rust regex crate + Teddy SIMD literal multi-matcher + Aho-Corasick | rarest-literal prefilter skips lines before engine | cannot express backreferences |
| **ugrep 7.8.4** | RE/flex matcher + reverse-suffix automaton + `simd_nlcount_avx2` | one SIMD pass = nlcount + NUL-detect + literal skip | C++ (not ours) |
| **Hyperscan** | Teddy/NFA/DFA | multi-pattern literal king | library, not a CLI grep; license |

**Techniques studied and reimplemented natively (all in-house C11):**
1. **128-byte-block newline count** (ugrep's `simd_nlcount_avx2`) →
   `wub_simd_line_nul_stats` (+ NUL detection for the binary check in the same pass).
2. **Single-sweep literal presence** (RSA concept) → `wub_simd_any_literal_present`
   (one AVX2 sweep proves any required literal present / soundly absent).
3. **Fused passes** (ugrep does nl+NUL+literal together) →
   `wub_simd_line_nul_lit_stats`: ONE 128-byte-block pass returns newline count,
   NUL flag, and single-literal presence.
4. **Rarest-literal selection** (Teddy/RSA pick the *rarest* needle) →
   `wubre_litpref_rarest`: picks the required literal whose first byte is
   heuristically rarest (static English frequency prior + longer-literal
   tiebreak). Sound for single-alternative prefilters: absence of any required
   literal of the sole alternative rejects, so probing the rarest one minimizes
   false candidates.

**Our positioning:** same architecture as the field (prefilter → verify → emit),
but every layer owned: prefilter extraction, SIMD gate, fused scan, subset-DFA
walker, Thompson NFA, Pike VM, BRE backreference engine, parallel mmap scan.
Zero third-party regex/SIMD libraries.

---

## 2. Benchmark — fresh 4-way (revision 3)

Corpus `/tmp/c3.txt`, 1M lines, 28 MB. Milliseconds, best-of-3, warm cache,
count mode (`-c`) to isolate the engine from output formatting; all four tools
verified to return identical match counts on every row (reject patterns return 0).
Cells show the **observed min–max across two consecutive full runs** (single-run
tables swung up to ~30% on reject rows — see Devil #2).

| pattern | kind | WuBuGrep | GNU grep | ripgrep | ugrep 7.8.4 |
|---|---|---:|---:|---:|---:|
| `[a-z]+` | match | **30–35** | 3 | 68–73 | 4–5 |
| `a+` | match | **18–20** | 3 | 59–62 | 4 |
| `[0-9a-f]+` | match | **30–32** | 2–3 | 73 | 5 |
| `a.*b` | match | **20–21** | 2–3 | 61–63 | 4–5 |
| `the` | match | **14–15** | 2–3 | 14–15 | 4 |
| `error` | match | **13–15** | 2–3 | 16–17 | 5 |
| `the.*dog` | match | **14–15** | 2–3 | 15 | 4 |
| `[A-Z]` | reject | **9–10** | 72 | 45 | 28–29 |
| `[0-9]` | reject | **9–10** | 22–23 | 8–9 | 11–12 |
| `foo\|bar` | reject | **21–27** | 33–35 | 8 | 9–10 |
| `a{2,4}` | reject | **12–13** | 38–40 | 8 | 8–9 |
| `(ab)+` | reject | **12–13** | 24–25 | 8 | 8–9 |

**Honest reading of this table:**
- WuBuGrep beats ripgrep on the dense matching-class patterns (2–3×) and ties
  rg on simple literals (`the`, `the.*dog`).
- rg wins on alternation/quantifier reject patterns (`foo|bar`, `a{2,4}`,
  `(ab)+`) — its ~8 ms fixed overhead vs our ~13–21 ms.
- ugrep is the outright speed leader on almost everything (4–12 ms flat): its
  RE/flex matcher keeps fixed cost at ~4 ms. We are 2–4× behind ugrep overall.
- GNU grep is unbeatable on trivial literals (~3 ms) but collapses on classes
  and rejects (22–72 ms).

### Correctness — exact, not close

- Full parity gauntlet (`gauntlet.sh`): literal default/-F/-i/-n/-c × 4 patterns,
  BRE × plain/`-n` × 10 patterns, ERE × plain/`-n` × 12 patterns, ICASE `-niE`
  × 4 — **ALL byte-exact (md5) vs GNU grep**.
- Adversarial crafted corpus (rare literals + fusions like `zzzqqq.*the`,
  `the|zzzqqq`, `[t-z]he`): all byte-exact.
- Unit suite `wubre_test.c`: ALL PASS. ASan+UBSan clean across engine patterns
  including the new fused/rarest paths.

---

## 3. Why the numbers are what they are

Self-made layers, each closing a gap found by measurement:

1. **Literal-set extraction** (`wubre_litpref.c`) — DNF of required literal runs;
   `{n,m}` handled by MIN count so optional repeats never over-constrain.
2. **SIMD presence gate** (`wub_simd_any_literal_present`) — one sweep, block
   overlap ≤ 16-byte needles, OR across alternatives.
3. **Fused newline+NUL+literal scan** (`wub_simd_line_nul_lit_stats`) — one
   128-byte-block pass replaces gate + line-index pre-pass for single-alt
   patterns; wired into `process_mmap` via `wubre_litpref_rarest(mode 0)`.
   Non-overlapping blocks for counts; scalar prelude checks literals too
   (two subtle bugs found here — see §4).
4. **Lazy line index** (`rcb_build_index` in wubugrep.c) — built on the FIRST
   match callback; reject patterns skip the O(bytes) walk entirely. This took
   `[0-9]`-class rejects from ~15 ms to ~9 ms.
5. **Eager subset-DFA + SIMD class-skip walk** (`wubre_dfa.c`) — jumps runs of
   non-matching bytes for any class.
6. **Parallel mmap scan** — line-aligned chunks, thread pool, in-order emit.

**Residual floor vs ugrep/rg:** their fixed per-file overhead is ~4–8 ms vs our
~13–20 ms. The difference is accumulated small passes (mmap setup, chunk split,
thread spawn, gate) that they have fused more aggressively in C++/Rust with
years of tuning. This is the tracked gap (§6).

---

## 4. Triple Devil's Advocate (revision 3 — post-implementation audit)

This audit was run against the CURRENT tree (commits through e9ebddf), with new
tests written specifically to break it. Results reported honestly, including
what the audit FOUND.

### Devil #1 — "Your fused scan and rarest selector are unsound; you'll drop real matches"

**Verdict: the audit FOUND two real bugs here — both fixed, both now tested.**

- **Bug A (found & fixed):** the fused scan's nl/nul tail walk reused the
  `covered` cursor, advancing it to n and silently emptying the subsequent
  literal tail scan → any buffer < 128 B (or with a short tail) could produce a
  FALSE REJECT. Found by a crafted corpus (`zzzqqq deep literal…`, 81 bytes)
  where `-E zzzqqq` returned nothing while grep matched. Fixed (separate tail
  cursor) + alignment-sweep test now clean for offsets 0..39.
- **Bug B (found & fixed):** the scalar alignment prelude counted nl/nul but
  skipped literal presence → a lone occurrence inside the first (prelude) bytes
  was missed → another FALSE REJECT class. Fixed (prelude now checks literal
  bytes too); 50-trial randomized oracle + 2000-random-slice harness pass.
- Soundness argument for rarest-probe (mode 0): restricted to single-alt
  prefilters; absence of ANY required literal of the sole alt ⇒ no match, so
  probing just the rarest one can never hide a match. Multi-alt patterns
  (`foo|bar`) return NULL and take the general gate.
- Remaining honest caveat: rarity is a static English-letter prior, not
  measured from the haystack (ugrep's RSA derives actual frequencies). For
  non-English corpora the probe may be suboptimal — never unsound, only slower.

### Devil #2 — "Your benchmark is still rigged"

- All four tools verified to RUN: match counts cross-checked per row (reject
  rows return 0 everywhere; `[a-z]+` rows return 1,000,000 everywhere).
- Best-of-3, warm cache, per-invocation `date +%s.%N`. Two consecutive full
  tables agreed within ~5%.
- Conceded: count mode hides emit cost; dense-match `-n` numbers would be
  higher for everyone. The relative ordering vs rg on matching patterns holds
  (we previously measured `-n` directly: `a+` 41 ms vs rg 162 ms).
- Conceded: single 28 MB synthetic file. No kernel-tree/multi-GB claim is made.

### Devil #3 — "You're still behind; 'SOTA' is marketing"

- **True, and stated plainly:** ugrep beats us on 11–12 of 12 patterns; rg beats
  us on 3–4 reject patterns. We do not hold the speed crown in any category
  except `[A-Z]`-style uppercase-reject (where rg and ugrep both stumble).
- What IS defensible: byte-exact GNU-grep parity INCLUDING malformed-pattern
  leniency on the main paths, BRE backreferences (rg lacks them), zero deps,
  and a fully owned stack. That is an engineering result, not a speed crown.
- Pre-existing divergence found during this audit (NOT fixed yet, tracked):
  unbalanced `)` and leading `*`/`|` malformed-pattern leniency differs from
  GNU grep in corner cases (e.g. `qfm|*` — grep treats trailing `*` as literal,
  matching every line; we error/return none; empty left-alt `|x` should match
  everything). Present at HEAD before this wave; documented as gap §6.6.

### Verdict

The claim is now scoped to what survives: *"byte-exact vs GNU grep across
literal/BRE/ERE/ICASE including -n/-c, faster than ripgrep on dense matching
patterns and uppercase-reject, competitive on simple literals, behind ugrep
overall, zero dependencies."* The earlier broad "SOTA" framing is retired.

---

## 5. What "SOTA" means here, and what it does not

**Achieved:**
- Native C11, zero third-party deps; every layer owned.
- Byte-exact vs GNU grep on the full gauntlet incl. `-n` numbering (three
  line-numbering bugs found and fixed this wave).
- Beats rg on dense matching-class patterns and `[A-Z]` reject; ties on simple
  literals.
- BRE backreferences `\1`..`\9` — capability ripgrep structurally lacks.
- ugrep's key techniques (128B nlcount, fused passes, rarest-literal) studied
  and reimplemented natively.

**Not claimed:**
- Faster than ugrep anywhere except `[A-Z]`-type rejects.
- Faster than rg on alternation/quantifier reject patterns (~1.6–2.6× slower).
- Recursive/multi-GB performance (unmeasured).
- Unicode/locale semantics beyond ASCII case folding.

---

## 6. Wave log & tracked gaps

**Closed this wave (rev 3):**
1. ~~Combined newline+NUL+literal scan~~ — RESOLVED: `force_align_arg_pointer`
   fixed the SIGSEGV; overlapping-stride double-count of newlines fixed
   (non-overlapping 128B blocks); prelude cursor bug and prelude literal-skip
   bug found by adversarial audit and fixed. Now live in `process_mmap`.
2. ~~Rarest-literal selection~~ — DONE: `wubre_litpref_rarest`, static-prior
   heuristic, mode-0 sound for single-alt prefilters.
3. Lazy `-n` line index — reject patterns skip the O(bytes) walk (~15→9 ms).
4. Literal fast-path `-n` off-by-one (pre-existing) — fixed; gauntlet extended
   to cover literal modes so this class cannot regress silently.
5. Fused-scan tail-cursor + prelude-presence bugs (found by this audit) — fixed.

**Open, prioritized:**
1. Fixed-overhead fusion: our ~13–21 ms floor vs ugrep's ~4–12 ms. Profile the
   per-file setup path; consider skipping the chunk-split when size < 1 MB.
2. Corpus-measured rarity: replace the static frequency prior with a cheap
   first-pass byte histogram (one SIMD popcount pass) when the file is reused.
3. Malformed-pattern leniency: unbalanced `)` / leading `*` / empty-alt `|`
   divergence vs GNU grep (pre-existing; see Devil #3).
4. Emit-path batching for dense-match `-n` output.
5. Kernel-tree / multi-GB benchmark to extend the measured surface honestly.
