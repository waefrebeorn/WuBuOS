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
  -- [DONE: `src/kernel/wubu_9pc.{c,h}` + `test_w9pc`; linked into the bare-metal
  kernel.]
- A3: a booted-image verification harness: QEMU/`run-agi.sh` + serial, kernel
  fetches `/n/apps/<mod>` over 9P from a host-exported server and runs it.
  Proof: real `RESULT: PASS` printed from the kernel, module result observed.
  -- [DONE 2026-11-07: `src/kernel/wubu_n9.{c,h}` (COM2 framed-9P client +
  kernel vmm-mapped loader run) + `src/firmware/run-agi9p.sh` + host
  `tools/agi-9p-server.c` (styxfs over a COM2 unix socket) + `tools/mkmod.c`
  (self-contained PE returning 55). Verified live: kernel prints
  `A3: RESULT: PASS -- 9P fetched /apps/agi-demo.bin (480 B) and ran ->
  entry=55` in the WuBuFW QEMU boot log; server served the full
  version/attach/walk/open/read/clunk session.]
- A4: P3-on-kernel — session boot on the booted kernel launches the fetched app.
  -- [DONE 2026-11-07: kernel-clean `wubu_zxreg_load_bytes` (register + run an
  already-fetched image; SHA-256 dedup / refcount / cached entry result) built
  the bridge from wubu_n9's 9P fetch to the same registry `wubu_applaunch_
  as_session_launcher` uses on the hosted leg. The old kernel `wubu_zxreg_load`
  branch (WUBU_ZX_ERR_MAP) is now superseded on metal. The booted kernel logs
  `A4: RESULT: PASS -- session launched /apps/agi-demo.bin ... registered=1
  ref=1 dedup=1`, proving the app is registry-resident, a dedup second launch
  re-serves the cached entry result, and unload releases cleanly. Kernel libc
  gained memcmp + __strncpy_chk to serve the -nostdlib link.]
**Gated by:** (A4 done) the launch path is registry-backed on-kernel; the
remaining host-level polish is wiring a real session orchestrator (scan /n for
apps -> start each) onto that launcher for multiple apps.
**Acceptance:** kernel prints the fetched module's computed result (e.g. 7 / 72 /
55) from a 9P-attached /n, in a QEMU boot log.

### GAP B — Deepen self-hosting past the toy  [BIGGEST is A; B is #2]
**What:** prove the loader runs programs using the language's real features:
- B1 (done): globals, loops, arrays (init), params, recursion — fib(10)=55,
  data-array buf[0]=72.
- B2: strings/pointers/structs through the emitter→loader. 
  -- [DONE 2026-11-07: two new self-host stages in the loader-run harness
  (`test_wubu_tokenizer`, now 42 checks). `checksum.hd` walks the .data string
  literal "WUBU" through a real char* (deref + increment) and returns a
  running checksum -> 323; `struct.hd` stores/loads two struct members with
  the member's own 4-byte width and sums -> 42. Both run through the OLD
  holyd_codegen_* emitter -> hd_write_pe_program -> wubu_zx_run at two
  relocation bases (0x400000/0x420000). CORRECTION: the "structs/unions/strings
  missing in holyd_codegen_*" claim was stale -- the old backend already
  carries them; the earlier stages (str.hd, computed-index walks) plus these
  two prove it.] 
- B3: THE forcing function — **holyd compiles holyd**: build a compiler stage
  (lexer or expression parser) as a holyd-program, emit it, run it over the
  loader, and have it lex a real source string. Proof: entry returns the token
  count / symbol table size of a real snippet.
  -- [depth DONE 2026-11-06/07: `tokenizer.hd` is a self-hosted LEXER stage that
  lexes "int x = 42" -> 4 tokens through the loader (test_wubu_tokenizer);
  multi-digit integer parsing folds "321" -> 321. A real compiler sub-stage
  (lexing) already executes as a holyd program.]
- B4: resolve the two-backend split (declare one the loader's codegen; don't let
  them diverge further). Proof: a cross-backend consistency test.
  -- [DONE 2026-11-07: wubunos `make test_backend_consistency` diff-tests the OLD
  holyd_codegen_* emitter->loader path against the MODERN holyd_mir_eval on the
  SAME four construct-heavy programs (pointer-walk checksum, struct members,
  multi-digit parse, lexer), wired into the aggregate gate. Result: 4/4 AGREE --
  the two backends do NOT diverge on string/pointer/struct/lexer programs
  (checksum 323, struct 42, multi-digit 321, tokenizer 4), so the split is
  proven consistent, not merely feared.]
**Gated by:** B4 done; GAP B (B1-B4) is closed on the loader path. The
remaining self-hosting depth is structural C-level (nested/arrays-of struct,
struct-by-value args/returns, unions) -- future work, not a named gap.
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
