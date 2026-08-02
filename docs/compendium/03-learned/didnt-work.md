# 03-learned — What Didn't Work (Yet)

*The honesty ledger: failures with why + when they may change. Append-only;
newest last. Copy TEMPLATE.md for new entries.*

## 2026-08-02 — Timer PREEMPTION: resumed iretq #GPs (open)
- **Context:** enabling `task_preempt_enable()` on metal.
- **What didn't work:** the second preemption #GPs at the ISR's iretq.
- **Evidence:** `GP: rip=8010637f (iretq) cs=8 rflags=4006 err=0 iretframe=[d,0,8010637f,8,4006,1440fb8]`
  — the iretq ran with the NT flag (0x4000) set in its own RFLAGS; with no
  TSS in the GDT the CPU attempts a task-return and faults. The saved
  rflags that carried NT likely came from the resume-path register shuffle
  (the switch's save/restore alignment under preemption).
- **Why it failed:** hypothesis — the preempt resume's rsp/register
  alignment is off by a slot for the ISR case (the cooperative case is
  aligned and works); the NT bit then slides into the restored rflags.
- **When it may change:** a dedicated preempt deep-dive (fix the resume
  alignment AND add a real TSS to the GDT so even a stray task-return is
  defined). P1, not blocking the stable base.

## 2026-08-02 — The "vector-0 #DE" hypothesis (wrong root cause)
- **Context:** the long #DE hunt — interrupts never delivered, faults
  reported vector 0.
- **What didn't work:** the theory that the LAPIC/IOAPIC delivered a spurious
  vector 0 (LAPIC ISR=0 seemed to confirm). The real cause was a READ
  OFFSET bug ([rsp+120] vs [rsp+112]) — delivery was perfect all along.
- **Why it failed:** trusting frame values before the struct layout was
  proven. The discipline: dump RAW slots before reading named fields.
- **When it may change:** never as a theory; the raw-dump discipline stays.

## 2026-08-02 — Frame reads via a struct with an unsaved register
- **Context:** the InterruptFrame struct had a `rax` field the asm never
  saved, shifting every field by 8 → "garbage frames" (rip=8, cs=0x202).
- **What didn't work:** reading rip/cs/rflags through the shifted struct.
- **Why:** the struct must mirror the assembly's EXACT save list. Fixed by
  removing rax — then RE-ADDED when the rax-clobber bug proved rax must
  actually be saved. The struct now matches the asm 1:1 (15 regs).
- **When it may change:** the struct stays in lockstep with the asm; any asm
  save change MUST update the struct in the same commit.

## 2026-08-02 — IOAPIC flat-MMIO accessors (fixed earlier)
- **Context:** the redirection table was "permanently unprogrammable".
- **What didn't work:** indexing the IOAPIC as a flat array; it needs the
  IOREGSEL@0x00 / IOWIN@0x10 select-window protocol.
- **Evidence:** the fixed accessors read the correct 23-pin version.
- **When it may change:** never — the select-window protocol is hardware.

## 2026-08-02 — RESOLVED: timer preemption works (the #GP is dead)
The tracked #GP is FIXED. Root cause confirmed: the iretq ran with the NT
flag (bit 14) set in the frame's rflags -> hardware task-return -> #GP
(garbage TR; no TSS). Three-part fix:
1. `wubu_tss` -- a real TSS64 + GDT descriptor (stray task-returns defined).
2. `tasking_switch.S` -- the rflags restore masks NT out (`and -16384`).
3. `isr_stubs.S` -- the iretq itself masks the frame's rflags slot before
   returning (the definitive guarantee: the iretq can never pop NT).
Evidence: 8s soak under FULL timer preemption, 5 samples, ZERO faults;
tick 12->204 at 100Hz, promoted 1138->20718, attest_valid=1.

## 2026-08-02 — The tick-12/33/153 freeze (OPEN, tracked)
- **Context:** the batch-3 kernel froze deterministically (tick 12, then
  33/57/153 as changes perturbed it) -- no faults, silent halt.
- **Evidence:** QEMU monitor at the freeze: CR3=0x70000 (the early-stack
  address!), RIP=0x2010e0 (kernel serial code), RSP=0x9ef18 (top of
  conventional RAM), RCX=0x3d5/RDX=0x3fd (UART); the page tables at
  0x300000 were INTACT (watchpoints never fired); the CPU had wandered
  into the kernel text with a garbage stack.
- **What was ruled out (each perturbed the freeze point instead of
  fixing it):** the ring race lock (tick 12 -> 153), the promote %s on
  span data (tick 12 -> 33 -- the data is not reliably NUL-terminated;
  the ID is safe), CR0.WP (12 -> 57), the idle HLT (no effect), the
  fxsave/fxrstor (no effect), the low-water tracking (no effect).
- **Stable configuration (committed):** the batch-2 base + the promote
  message prints the span ID (never the data) + the console additions.
  Soak: 155 -> 203 ticks, promoted 20838, ZERO faults.
- **When it may change:** the next session picks up the freeze with the
  QEMU forensics (the CR3=0x70000 + the conventional-memory stack point
  at a low-memory corruption -- the loader/firmware area).
