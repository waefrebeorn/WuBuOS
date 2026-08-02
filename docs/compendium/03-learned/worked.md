# 03-learned — What Worked

*The prestige ledger: verified wins with evidence. Append-only; newest last.
Copy TEMPLATE.md for new entries.*

## 2026-08-02 — Higher-half entry via absolute jump
- **Context:** the whole kernel ran at identity addresses because crt0 used a
  relative `call kernel_main` from physical code.
- **What worked:** `movabs rax, kernel_main; jmp rax` — fault RIPs are now
  proper higher-half addresses.
- **Evidence:** fault frames changed from physical (0x10ed00) to
  `0xffffffff8010f900`-style.

## 2026-08-02 — The vector read: [rsp+112], not [rsp+120]
- **Context:** every interrupt "looked like vector 0" (#DE) — the #DE hunt.
- **What worked:** the stub pushes [error, vector]; after the register
  pushes the VECTOR (the last push) sits at +112 and the error at +120.
  The C code had read +120 (the error). One-byte offset, whole-system
  impact.
- **Evidence:** `raw14=20 raw15=0` (vector 32 at +112, error 0 at +120);
  after the fix, interrupts deliver and `tick` advances.

## 2026-08-02 — Save RAX in the ISR
- **Context:** "memcpy writes to 0xff000000" #PF during the live session.
- **What worked:** the common ISR handler now saves/restores rax (15 regs).
  The ISR's C code clobbers caller-saved rax; an interrupt mid-memcpy-loop
  corrupted the dest held in rax.
- **Evidence:** the #PF is gone; full live session stable
  (`tick=24→173`, `agi_uptime_ms` advancing, `attest_valid=1`).

## 2026-08-02 — Monitor-based debugging (QEMU physical dumps)
- **Context:** in-weeds fault hunts.
- **What worked:** `xp /Ngx <phys>` physical memory dumps via the QEMU
  monitor (BSS at phys 0x117c40 = higher-half 0xffffffff80117c40) —
  proved g_vbe was NOT corrupted and isolated the fault to the ISR rax
  clobber. Frames printed as raw qword slots (raw14..raw20) before the
  struct was trustworthy.
- **Evidence:** the BSS dump showing fb=0x402018/back=0xbec018/size=0x7e9000
  all correct — the corruption had to be a register, not memory.

## 2026-08-02 — Cooperative scheduling as the stable base
- **Context:** the preempt resume #GPs.
- **What worked:** cooperative round-robin (task_yield) + the 100 Hz tick
  (wakes sleepers, advances the AGI) delivers the entire live system
  without preemption.
- **Evidence:** console, bonzi, agent, uptime, sleep, attestation all green
  in one boot.

## 2026-08-02 — The /theme namespace + unified HID on metal
- **Context:** Kevin-Bacon pass 3 build-out — the self-modifying graphic set
  and the GameInput-style input layer.
- **What worked:** `wubu_theme` (a writable node tree `/theme/...` with an
  EDR write counter; presets seed it, `wubu_theme_apply()` re-derives the
  draw struct) + `wubu_hid` (one event model for keyboard/mouse/gamepad,
  common time base, per-device filters + master disable). Bonzi's gorilla +
  speech bubble now read the theme — `theme set /theme/gorilla/fur ff0000`
  re-skins the framebuffer live on metal.
- **Evidence:** host `test_theme_hid` ALL PASS; live console
  `theme set /theme/win/title_active 123456` → node list shows
  `00123456`, writes=19; stable boot probes green.
- **When it may change:** the /theme tree becomes the real Styx9 namespace
  when the fs lands on metal; the HID ring gets the xHCI USB feeder.

## 2026-08-02 — The promote loop is LIVE + the Colonel re-skins itself
- **Context:** the DA-3 self-improve loop was dormant (verifier=NULL on
  metal). Built wubu_verifier (well-formedness + emitter trust + semantic
  budget; deterministic, kernel-resident) + the AGI-writable theme step.
- **What worked:** installing the verifier ACTIVATED promotion — the AGI
  now consumes + promotes spans every tick (promoted climbs live); the
  tick's agi_theme_step derives theme nodes from supervisor state
  (attestation → title color, promoted_total → gorilla fur lerp green→gold,
  frozen → muted desktop), writing only on change (EDR-counted).
- **Evidence:** live console `promoted=1365→4550→7736`;
  `/theme/gorilla/fur = 00e0a000` (gold at mood cap);
  test_verifier ALL PASS; stable boot green.
- **When it may change:** the static scorer becomes the test-suite
  verifier (kernel self-tests gate real changes); the theme step gets more
  signals (agent focus, load, time of day).

## 2026-08-02 — The new-generation doc scanners (docs DA overhaul)
- **Context:** the generator covered 4 of 16 src dirs (135 of 582 modules);
  tests.md was a static list; no live state; no parity evidence.
- **What worked:** gen_docs.py rebuilt as a scanner suite — modules (full
  recursive walk, 582), api (629 header-sourced prototypes), tests (RUNS
  the curated suite, records PASS/FAIL), state (BOOTS QEMU, records the
  live vitals), parity (BUILDS the hosted targets, records the matrix).
- **Evidence:** the tests scanner immediately caught a REAL regression
  (agi_theme_step broke test_agi_kernel — fixed via stub additions);
  final run 4/4 PASS; Linux parity leg VERIFIED (make runtime tools).
- **When it may change:** the state scanner becomes a heartbeat; the
  verifier doctrine extends to docs (a change that breaks make docs or a
  curated test is not promotable).
