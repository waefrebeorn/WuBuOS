# The compiler gauntlet: first honest numbers

The WuBuOS tree carries the largest compiler test surface it has — gcc torture, gcc dg, gcc
compile, llvm, lacc, chibicc, compcert, c-testsuite, tinycc, slimcc, fujitsu, extern-gcc and
writing-a-c-compiler — in `src/compiler/test_gauntlet/suites/`. Until this session none of it
produced a number, for three separate reasons, each of which looked like a different problem:

1. `mk/tests.mk`'s `test_gauntlet` linked **one** of the thirteen corpora and listed
   `wubu_mir_interp.c` twice, so the gate died in 3 s with `collect2: ld returned 1 exit status`.
   A build break read as a failing test.
2. The runner printed a progress line per test to stderr and let every forked child inherit
   stdout; a 25 s run wrote **19.4 GB** to whatever it was redirected to.
3. Main reaped workers in array order (`waitpid(worker_pids[w])`), so every worker that finished
   early stayed a zombie for the rest of the run.

All three are fixed. Measurement method and result:

```
WUBU_GAUNTLET_STRIDE=20 ./gauntlet_v3      # every 20th test, all 13 corpora, 14 targets
```

| run | pass | fail | error | skip | total | valid_total | valid_pass_rate | wall | zombies |
|-----|------|------|-------|------|-------|-------------|-----------------|------|---------|
| 1   | 110  | 8    | 19    | 85   | 222   | 137         | 80.3 %          | 3 s  | 0       |
| 2   | 110  | 8    | 19    | 85   | 222   | 137         | 80.3 %          | 3 s  | 0       |
| 3   | 110  | 8    | 19    | 85   | 222   | 137         | 80.3 %          | 3 s  | 0       |

Deterministic across three isolated runs. `valid_total` excludes `skip` (tests the harness marks
invalid/unsupported for this build); `error` means the compiler crashed, timed out or failed to
parse; `fail` means it compiled and produced the wrong answer.

## What a sampled rate does and does not say

- It is a **stratified sample**, not the corpus: every 20th test in suite order, 222 of 4,431.
  `WUBU_GAUNTLET_LIMIT=n` exists too, but it takes the *first* n tests, which are all gcc
  torture — an 85 % rate from those is a claim about one corpus, not the compiler.
- The earlier `LIMIT=30` reading (17 pass / 0 fail / 3 error / 10 skip) is **not** the compiler's
  rate; it is the torture head's.
- A full run is 4,431 tests × 14 targets and has never been completed on this box.

## Open

- 8 `fail` cases are real wrong-answer bugs; the sample is deterministic, so they are worth
  bisecting one at a time.
- 19 `error` cases are parse/crash/limit failures; `HD_DIAG=1` prints the harness diagnostic, and
  one observed line is `[parse-peek] spun at pos=833 tok=79 ')' line=45 col=25`.
- Nothing here asserts a *conformance* level. 80.3 % of a stratified sample is a measurement, not
  a certificate, and the four `skip` classes have not been audited to see whether they are
  genuinely unsupported or quietly excluded.

## Reproduce

```
cd /home/wubu/wubunos
make gauntlet_v3
WUBU_GAUNTLET_STRIDE=20 ./gauntlet_v3 2>&1 | grep -a valid_pass_rate
```

Do not run two gauntlets at once: stale worker state reports `pass 0` for the whole suite, which
is how this measurement was nearly misread as total compiler failure.
