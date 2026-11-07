# WuBu Gap-Closure Roadmap (assembled from the triple devil's-advocate assessment)

Snapshot: wubuos `da1d414`, wubunos `a51d296` (2026-11-06). `make test-module` =
9 targets / 91 checks GREEN on a hosted Mint container; `make all` EXIT 0; the
kernel boots bare-metal via `run-agi.sh` (AGI attestation PASS). The /n module
loader, PE contract, registry, session stack, and 9P transport are proven as
HOSTED/harness artifacts. `wubuwizard` stays blocked on the pre-existing
`wubu_moe.c` brace imbalance (out of scope).

Every item below is a REAL_GAP (the standing rule: no stubs, prove a number not
a shape). Each lists its acceptance proof. Order = highest leverage first.

---

## THE THREE GAP CLASSES (what the assessment established)

1. **"Hosted, not an OS" (DA#3)** — the loader/registry/session/9P all run as
   hosted make-targets on the Mint box. The *booted kernel* reads nothing from
   `/n`; `wubu_zxreg_load`'s non-hosted branch is `WUBU_ZX_ERR_MAP`. The single
   biggest risk: we've proven a harness can load a PE, not that the OS runs one.
2. **"A demo, not a compiler" (DA#1)** — self-hosting is proven on programs with
   globals, loops, arrays, params + recursion (fib), but NOT on the language the
   compiler itself is written in: structs, pointers, unions, nested strings,
   switch-on-enum. Two backends (older `holyd_codegen_*` on the loader path, the
   modern `holyd_mir_eval.c` NOT on it) have diverged.
3. **"A junk drawer, not a kernel" (DA#2)** — ~450 vendored `wubu_*.c` driver
   files with no per-file test; the actual identity (~50 files: /n, VSL, NT,
   session, module stack) is the tested surface. `.o`/`.d` are gitignored (disk
   noise, not repo pollution); the unproven driver raft is the real debt.

---

## ROADMAP (highest-leverage first)

### GAP A — The on-kernel leg: "the OS loads an app from /n"  [BIGGEST]
**What:** a kernel 9P CLIENT + a wired `/n` read so the booted kernel fetches a
module over 9P and `wubu_zxreg_load` runs it — the hardware proof none of the
hosted tests can produce.
**Why:** DA#3's core charge. P3 `/n/background` was declared done on hosted
checks; its real value ("apps start at boot from /n") is unwon until on-kernel.
**Has several completable sub-rungs:**
- A1 (hosted, done): transport proven — `test_wubu_zx9p` fetches module bytes
  over real Styx/9P and runs them (91/91 includes it).
- A2: extract a **kernel-clean 9P client message module** (`wubu_9pc`:
  tversion/tattach/twalk/topen/tread/tclunk encode+decode over an abstract byte
  transport). Proof: compiles under `-ffreestanding -nostdlib -mcmodel=kernel`
  AND drives the in-process server in the hosted test (same codec both legs).
- A3: a booted-image verification harness: QEMU/`run-agi.sh` + serial, kernel
  fetches `/n/apps/<mod>` over 9P from a host-exported server and runs it.
  Proof: real `RESULT: PASS` printed from the kernel, module result observed.
- A4: P3-on-kernel — session boot on the booted kernel launches the fetched app.
**Gated by:** A3 needs emulation + serial observation (not possible on this
hosted container); A2 is a clean next step.
**Acceptance:** kernel prints the fetched module's computed result (e.g. 7 / 72 /
55) from a 9P-attached /n, in a QEMU boot log.

### GAP B — Deepen self-hosting past the toy  [BIGGEST is A; B is #2]
**What:** prove the loader runs programs using the language's real features:
- B1 (done): globals, loops, arrays (init), params, recursion — fib(10)=55,
  data-array buf[0]=72.
- B2: strings/pointers/structs through the emitter→loader (the MIR backend
  already handles nested brace-init/byte arrays — fold it onto the loader path
  OR extend `holyd_codegen_*` to match). Proof: a self-hosted program that walks
  a pointer into a string and returns a checksum.
- B3: THE forcing function — **holyd compiles holyd**: build a compiler stage
  (lexer or expression parser) as a holyd-program, emit it, run it over the
  loader, and have it lex a real source string. Proof: entry returns the token
  count / symbol table size of a real snippet.
- B4: resolve the two-backend split (declare one the loader's codegen; don't let
  them diverge further). Proof: a cross-backend consistency test.
**Gated by:** B2/B3 risk (each could be a honey-trap like the array-init — which
*is* fixed). B3 is the honest self-hosting bar.
**Acceptance:** B3 returns a real parse/lex result for non-trivial input.

### GAP C — The kernel tree boundary (DA#2 debt)
**What:** stop the driver raft masquerading as "the OS."
- C1: classify each `wubu_*.c`: OWNED-and-tested (in test-module or a named
  target) vs VENDORED-and-unproven. Add a `docs/owned-surface.md` manifest
  derived from the Makefile targets, not prose.
- C2: move the vendored raft under `src/kernel/vendor/` and give every OWNED
  module a test-module member (enforce: a module without a test = NOT owned).
- C3: kill the stale `holyc` names (`wubd_holyc_*`) and explain/remove
  `holyd_bf_stub.c`.
**Acceptance:** `make test-module` remains the honest inventory; a grep of
"owned" modules all appear in a test target.

### GAP D — Naming & organization (governance, not churn)
**What:** `docs/naming.md` (done) documented the `wubu_ns` vs `/n` vs `styx`
collision and the `mod_` rename-forward family. Enforce forward-only:
- D1: new loader/PE code uses `mod_run/mod_reg/mod_load`; never new `zx`.
- D2: do NOT mass-rename (regex rename already corrupted 9 files); rename at
  edit-time.
**Acceptance:** no new `wubu_zx*`-style koan prefixes; naming map stays
authoritative.

### GAP E — Workflow/CI that prevents the failure modes  [already started]
**What:** the aggregate gate + rules exist. Extend for durability:
- E1 (done): `make test-module` = one-command 91/91 gate (in test-module).
- E2: a pre-push self-check script `scripts/ship.sh <target>` that CLEANS →
  builds → runs the target → only then `git add/commit/push` (kills the two
  red-push failures outright).
- E3: cross-repo rule enforced in the script: a holyd change ships wubunos AND
  the wubuos gate that proves it, in one review.
**Acceptance:** `scripts/ship.sh test-module` is the only "ship" path; a red gate
aborts before commit.

### GAP F — P4 Omarchy first-boot onboarding  [least leverage, furthest]
**What:** first-boot greeter + configurator (omarchy_install_study.md is done).
**Gated by:** the desktop/session must be demonstrably running first (Gap A4).
**Acceptance:** a bootable first-run stream or a hosted end-to-end onboarding test.

---

## ONE-LINE PRIORITY
Close **A2 (kernel-clean 9P client)** next — it's the completable half of the
biggest gap and the seam everything else on the kernel leg hangs on. Then A3 (a
QEMU/serial campaign) is the honest "OS runs an app" proof. Everything labelled
DONE here is committed (`da1d414`/`a51d296`, gate 91/91); the rest is the roadmap,
each item carrying its own acceptance proof.

> Standing rule recap: every item is real work with a number as proof; never a
> stub, never a shape, never a fabricated boot. Update this file as gaps close.
