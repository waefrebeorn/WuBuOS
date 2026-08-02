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

## 2026-08-02 — Preemption FIXED + demand paging live + stack-alignment bug
- **Context:** the tracked #GP (resumed iretq with NT) + the P0 tier.
- **What worked (3 wins):**
  1. Preemption: wubu_tss (real TSS64 + GDT descriptor) + NT-mask in the
     switch's rflags restore + the iretq's frame-rflags masked at the exit
     (the definitive guarantee). 8s soak under FULL preemption, 5 samples,
     ZERO faults; tick 12->204 @100Hz, promoted 1138->20718.
  2. wubu_vmm: bitmap page allocator + CR3 page-table map + DEMAND-ZERO
     regions; the #PF handler now does real work (alloc+map+retry).
     Verified live: `vmm touch` -> faults=0 -> readback=12345678 faults=1
     (the iretq retried the faulting instruction).
  3. The vmm init surfaced a latent ABI bug: _stack_top was not 16-aligned,
     so the compiler's movaps #GP'd. Fixed with ALIGN(16) in kernel.ld.
- **Evidence:** soak + demand demo above; test_sync ALL PASS (20000 values,
  SPSC, in order); test_vmm ALL PASS; stable boot green.
- **When it may change:** the demand regions become the real segment/store
  substrate; the spinlock gets its metal workout when a driver uses it.

## 2026-08-02 — The 115-gap register + the first close batch
- **Context:** "devil's advocate find 100+ gaps and close them".
- **What worked:** a systematic sweep (53 kernel C files / 18K LOC / 29
  stub markers / 132 silent return-0s) -> docs/compendium/04-roadmap/
  gap-register.md with 115 VERIFIED gaps (A..K categories). Closed in
  this batch (10): static ABI asserts for InterruptFrame (compile-time
  frame contract), `stats` command (fault counters exposed), `dump`
  hexdump command (the in-OS debugger), AGI memory-pressure awareness
  (the desktop dims under pressure -- B10/G9), wubu_sync spinlock used
  by the vmm allocator (D6 -- the sync module's first metal user),
  `make check` (6 tests + kernel build), commands.md generator (J3).
- **Evidence:** make check ALL GREEN; live: stats (tick=12 irq32=12,
  zero exceptions), vmm touch (faults=1), dump 117c60 (the g_vbe struct
  hexdump); the static assert caught the frame-size=176 discrepancy.
- **When it may change:** 105 gaps remain OPEN in the register -- the
  next close batches are the IST/double-fault, FPU save on switch, the
  e820 memory map, and the UART-RX ISR.

## 2026-08-02 — Close batch 2: FPU save, IST, interrupt UART, e820
- **Context:** gap-register close batch (A2/C8/E2/I1 + F5 polish).
- **What worked:**
  1. FPU/SSE on switch (C8): fxsave/fxrstor in tasking_switch.S + primed
     first-run contexts (FCW 0x37F + MXCSR 0x1F80). The memset exposed a
     latent heap ABI bug: CMemUsed's 8-byte header made every payload
     8-off -> the compiler's movaps #GP'd on the CTask. Fixed: padded the
     header to 16 bytes (all mem_alloc payloads now 16-aligned).
  2. Double-fault IST (A2): wubu_tss IST1 (8KB dedicated stack) + the
     vector-8 gate rides it -- a task-stack overflow can't triple-fault.
  3. Interrupt-driven UART (E2): IOAPIC pin 4 -> vector 36 -> the wubu_sync
     FIFO; console pops it, safe poll backup. Console verified interactive
     over the IRQ path.
  4. Real memory map (I1): the LEGACY boot.S path never runs (the chain is
     WuBuFW -> BOOTX64.EFI loader -> kernel), so the LOADER now collects
     GetMemoryMap before ExitBootServices into 0x98000; the vmm owns the
     REAL RAM: free_pages 244736 -> 113664 (512MB QEMU).
  5. dump command reworked: klog has no %02x (prints literally) -- manual
     hex into a buffer + plain %s. Live-verified dumping the memmap.
- **Evidence:** make check ALL GREEN; soak: 4 samples ZERO faults under
  full preemption with the FPU save live; console interactive OK;
  loader: "memmap @ 0x98000 (5 regions)"; vmm free_pages=113664.
- **When it may change:** the memmap table is capped at 8 regions; a
  machine with more fragments needs the cap raised.

## 2026-08-02 — Close batch 3: canaries, WP, stack tracking, dump hardening
- **Context:** gap-register batch (A6/C9/B5/B6 + diagnostics).
- **What worked:**
  1. Heap canaries live: mem_validate_all wired into the `mem` command --
     'canaries=OK' on metal (the red-zone machinery existed, never ran).
  2. CR0.WP at paging-enable (crt0): the kernel can no longer silently
     write RO pages.
  3. Stack low-water tracking: per-task stack_min at every switch + the
     `tasks` command reports usage % / OVER. FLAGGED the agi-agent's
     256KB stack as OVER -> bumped to 512KB.
  4. dump hardening: wubu_vmm_is_mapped validates every page before
     reading -- a typo'd address now prints 'UNMAPPED' instead of
     #PF-halting the OS (the dump of 0x8011ac50 halted the console).
  5. The klog %llu lesson AGAIN: the AGI's 'PROMOTED span %llu' flooded
     the serial every tick (prints literally). Fixed both %llu sites.
- **Evidence:** make check ALL GREEN; live: spam=0, faults=NONE, canaries
  OK, stats tick/irq32 equal, dump UNMAPPED path verified.
- **When it may change:** the 'OVER' display's root (rsp sampled below the
  base) needs a follow-up with a real CTask dump; guard pages come with
  the multi-address-space work.

## 2026-08-02 — THE FREEZE SOLVED: the unbounded serial_tx
- **Context:** the tick-12/33/153 freeze (extensive forensics).
- **Root cause (finally):** the freeze's CPU state was the SERIAL SPIN --
  RAX=0xffffffff, RDX=0x3fd (the LSR port), RIP in the serial code,
  IF=0. The unbounded `while ((inb(LSR) & 0x20) == 0);` in serial_tx
  spun FOREVER whenever a slow/no serial reader backed up the socket
  (the UART's THR-empty stops). The "wild control flow" (the CR3=0x70000
  + the mid-instruction RIP) was the SPIN's state, not corruption. Every
  timing perturbation moved the freeze point because it changed the TX
  rate -- the flood (slow loop) hid it, the rate-limit (fast loop)
  exposed it.
- **Fix:** serial_tx waits a BOUNDED number of polls then DROPS the char
  -- the serial is a debug channel, the kernel must never block on it.
- **Evidence:** soak 156->203 ticks, promoted 20710, ZERO faults; 9
  seconds with NO serial reader: the kernel survives (the old code
  froze instantly). make check ALL GREEN.
- **When it may change:** the dropped chars under backpressure are the
  cost -- a TX ring buffer (ISR-fed) is the future polish.

## 2026-08-02 — THE "CORRUPTION" WAS THE FIRMWARE'S SHELL IDLE (resolved)
- **The hunt:** the no-input boots showed RIP=0x2010e0, CR3=0x70000,
  RSP=0x9ef18, IF=0 -- framed as a kernel wild-jump corruption.
- **The truth:** those boots NEVER LAUNCHED THE KERNEL. WuBuFW boots
  into its own interactive shell and waits for input ("exit" hands off
  to the kernel). The 0x2010e0/0x70000/0x9e000 values are the
  FIRMWARE's text/stack/page-table state -- the shell's idle wait, not
  a kernel bug.
- **Proof:** with the "exit" handoff the kernel runs a 15s soak with
  ZERO faults and the RIP in the normal higher-half text
  (ffffffff80101928). The kernel's REAL freezes (the qsoak's tick-12
  serial spin) were real and are fixed (bounded TX + the no-retry
  contract).
- **Lesson:** the QEMU monitor forensics must confirm the KERNEL is
  running (serial marker / "live console up") before reading the CPU
  state -- a firmware-shell boot looks like an arbitrary kernel state.

## 2026-08-02 — Console RX chain verified alive (the loss is TX-side)
- Probe: the monitor xp of wubu_serial's statics -- one byte bumps
  g_irq_count 0->1 (vector-36 IRQ fires) and the FIFO count returns to
  0 (the console task consumes it). The RX chain (UART -> IRQ ->
  wubu_sync FIFO -> console drain/pop) is fully alive.
- The typed command's response still doesn't appear: under the
  promote-flood backpressure the bounded TX drops chars, so the
  response's chars are slow/dropped. The rate-limit cut the flood
  ~90x; the remaining fix is the response path's TX priority or a real
  ISR-fed TX ring -- tracked.

## 2026-08-02 — Serial TX ring fixed the console's interactive (E3)
- The 4KB TX ring + the opportunistic drain (timer tick + console idle)
  made the console's responses COMPLETE under the promote flood --
  live-verified: "uptime" answered "tick=19 ... promoted=3870" while
  518 promote lines flowed. The earlier tracked "console goes silent"
  item is closed: the drop-continue contract + the ring = the kernel
  never blocks on its debug channel and the console stays usable.

## 2026-08-02 — The F3-family root: the metal's malloc returned NULL
- **Context:** the boot volume mount + the `run` command + the crash disk all failed on metal while the identical host tests passed.
- **What worked:** two real roots, both fixed: (1) the libc bump `malloc` was never initialized on metal (kernel_main never calls libm_heap_init), so EVERY calloc/malloc returned NULL -- now delegates to the kernel's mem_alloc (with the bump fallback for the hosted pre-heap window); (2) the ahci read/write return the SECTOR COUNT while the fat32 layer expects 0-on-success -- the boot-volume adapters now normalize.
- **Why (root cause / reason):** VERIFIED by instrumenting the boot: `c1=0` (calloc NULL with 48MB available + a valid heap) pointed at the allocator, and the host repro showed `format: -1` (the ahci's 1 != 0). After the fixes: `boot FAT32 volume mounted (hba=0 en=2 p=0 d=0 a=0)` on metal + the host's format/mount/create/write all 0.
- **Evidence:** `c1=0 avail=50475904 used=16628864 val=0`; `format: -1` through the raw ahci vs `format: 0` through the normalized adapters; test_ahcifat now in make check.
- **When it may change:** the G4/G6 saves are now reachable (boot volume mounted); their klog responses are still subject to the serial drop policy under the promote flood.
- **Related:** libc.c malloc, metal_main.c bootvol adapters, fat32_boot_attach, ahci.c return convention.
