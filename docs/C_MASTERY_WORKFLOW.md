# C mastery / workflow doctrine for WuBuOS + WEMU

This is the working contract for making the body and the emulator better,
faster, and stronger. It is deliberately short: a rule that is not checked
is an opinion.

## Sources researched (primary first)

- **SEI CERT C Coding Standard (C11)** — rules for PRE/EXP/INT/ARR/STR/MEM/
  FIO/ERR/CON plus POSIX; risk = severity x likelihood x mitigation cost.
  https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/
- **ISO/IEC TS 17961** — the analyzer-enforceable subset of C secure coding;
  a conforming analyzer must diagnose each rule. Use it as the mental model
  for "can a tool actually catch this class?"
  https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/front-matter/introduction/isoiec-ts-17961-c-secure-coding-rules/
- **GCC manual, Options That Control Static Analysis** — `-fanalyzer` is
  path-sensitive and ships with the compiler already in our toolchain.
  https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/ (section 3.9/3.10)
- **Linux kernel coding style** — tabs/8, K&R, 80 columns; more than three
  indentation levels is a design smell, not a formatting problem.
  https://docs.kernel.org/process/coding-style.html
- **CI static analysis practice** — analyze the diff on developer/CI paths,
  keep `clang-tidy`/`cppcheck` as a quality gate, suppress with documented
  justification, and never retry a flaky statistical gate into passing.
  https://programming.wyattau.com/enviroment_and_toolchain/4_development_enviroment_analysis/3_static_analysis/
- **Runnable directories / hybrid repo organization** — self-contained units
  with their own test lifecycles beat both a giant monorepo and uncoordinated
  polyrepos; submodules give atomic dependency pins but demand explicit
  cross-repo publication.
  https://arxiv.org/html/2512.03815

## The loop (every increment, no exceptions)

1. **Spec first.** Name the authoritative source and section before editing:
   Intel SDM for WEMU, VIRTIO/xHCI/PCI for drivers, C11 for the language.
   A fix without a spec citation is a guess, however green the gate is.
2. **Reproduce on a host oracle.** For WEMU use real inline asm on this CPU;
   for drivers use a real QEMU boot and assert the guest-visible line.
3. **Write the failing gate.** Exact instruction/input/expected-vs-got.
   A test that cannot go RED is not a test; a bug found by hand gets a gate
   as the other half of the fix.
4. **Negative control.** Revert only the fix, rebuild freshly, confirm the
   same FAIL, restore, confirm PASS. Stale objects make fake controls.
5. **Minimal fix, one concern per file.** No drive-by refactors in a bug leg.
6. **Full gates.** WEMU `make test-clean`; WuBuOS `make -k -j24 test`,
   `make test_linux_product`, `make test_docs`, `make tandem_check`.
7. **Publish with the proof.** Commit message names the reproducer, the
   spec, the gate and the RED/GREEN numbers. Then check live CI via API,
   not cached HTML.

## Gates now wired

| Gate | Command | Catches |
|---|---|---|
| Static analysis | `make static_check` (both repos) | GCC `-fanalyzer` path-sensitive C defects, with `-Werror`; `STATIC_ANALYZE_SKIP` may only shrink |
| Declaration hygiene | `make kernel-strict` (in `tandem_check`) | freestanding declaration/type errors CI sees |
| Standards ratchet | `make test_standards` | long files, fat headers, literal drift — fails on GROWTH |
| Tandem one-command check | `make tandem_check` | kernel contract, boot, PCI bridge, modern virtio, guest WEMU, standalone WEMU, static analysis, sanitizers |
| Emulator sanitizers | `make -C wemu test_sanitize` | ASan+UBSan over the core and five host-differential ISA gates |
| Statistical non-vacuity | `make test_default_optin` | fixed thresholds, no retries; add samples, never loosen a shipped-path ceiling |

`static_check` is intentionally scoped to sources that parse with a narrow
include set: WEMU's whole `src/`, and 342 of WuBuOS's kernel sources (340
hosted + 2 bare-metal analyzed with `MYSEED_METAL`). The one that cannot be
parsed standalone is named in `STATIC_ANALYZE_SKIP` and is not declared
clean — it remains a `kernel-strict` campaign.
Each skipped file must be removed from that list when its include hygiene
is fixed; the list may only shrink. This is the ratchet idea: never claim a
standard you cannot measure.

`static_check` uses `-std=gnu11`, not `-std=c11`: bare-metal leaves use GNU
`asm`, and glibc exposes `siginfo_t`/POSIX declarations under the GNU
dialect. Source remains C11 plus explicitly recorded GNU extensions.

## Code rules that pay immediately

- C11 only; opaque structs; minimal includes; no function bodies in headers.
- One concern per file (~500 lines, 600 measured ceiling); dissolve, don't expand.
- Name the layer in the file name; check both definition and callers on rename.
- Own a fact once. Search for an existing helper before writing a new one.
- Fail closed: refusal leaves the caller's buffer untouched and returns an error.
- Comments explain WHY, never WHAT. A stale comment is a build failure in
  spirit: fix it in the same commit as the code it describes.
- SeriaIize timing gates. Two `-j24` suites with statistical controls running
  concurrently depress the Welch t-statistic; that is a scheduling bug, not
  a reason to accept a near miss. If a control lands at `|t|=9.7` against
  `>10`, add samples (64 -> 128) or lower ONLY the control threshold to a
  still-overwhelming `>6` with the p-value in a comment.

## Repository organization

- One canonical checkout per repo. Parallel agents get git worktrees, never
  second copies; merge sequentially and re-run each leg's gates on the merge.
- `wubuos/src/wemu` is a pinned submodule: publish the WEMU commit first,
  then bump the gitlink. Never describe a WuBuOS driver change as WEMU work.
- Docs are the front door. A document naming a path that does not exist is a
  build failure (`make test_docs` checks paths).
- `wemu` keeps its own gates and CI; a change that only compiles locally is
  not portable. The tracked busybox `.deb` fixture makes CI independent of
  `/home/wubu/opt`.

## What to do next, in priority order

1. Extend `static_check` file-by-file as `kernel-strict` clears include
   hygiene, rather than lowering the gate or suppressing diagnostics.
2. Add ASan+UBSan runs for hosted/WEMU gates that do not depend on a bare-metal
   memory map; keep them separate from the freestanding kernel link.
3. Keep the host-oracle differential suites growing one instruction class at
   a time; every WEMU fix lands with a named gate and a pin in WuBuOS.
4. Keep driver proofs guest-visible: a report-only line is a diagnostic, a
   live disk mount or packet round-trip is a gate.
