# 03-learned — Bug War Stories (root causes, full detail)

*The deep-dive archive. Each entry: symptom → hunt → root cause → fix →
verification. Append-only; newest last.*

## The "memcpy writes to 0xff000000" #PF

**Symptom:** live session crashed with `PF: cr2=ff000000` during Bonzi's
draw path; `memcpy` (from `vbe_swap`) wrote to an unmapped address.

**Hunt:**
1. Caller chain via the faulted stack: `caller=[8010cdee,...]` → inside
   `wubu_bonzi_tick` → `bn_draw` → `vbe_swap` → `memcpy`.
2. BSS dump via the QEMU monitor (phys 0x117c40): `g_vbe.fb=0x402018,
   back=0xbec018, fb_size=0x7e9000` — ALL CORRECT. Memory was not corrupt.
3. Frame regs at the fault: `rsi=0xbec018 (src ✓) rdx=0x7e9000 (size ✓)
   rcx=0x200000 (loop counter, 2 MB in ✓)` — but the write landed at
   `0xff000000` = dest + 2 MB ⇒ the DEST register (rax) was garbage.
4. `common_isr_handler` saves 14 regs — **rax was not among them.** The ISR's
   C code (klog_printf etc.) clobbers caller-saved rax; a timer IRQ landing
   mid-memcpy-loop destroyed the dest held in rax.

**Root cause:** the ISR did not preserve rax across the dispatch.

**Fix:** push/pop rax in `common_isr_handler` (15 regs now), `rax` first in
`InterruptFrame`, syscall entry aligned.

**Verification:** the #PF is gone; `tick` advances 24→173 across probes;
`agi_uptime_ms` + trace ring advance; `attest_valid=1`. Stable boot.

**Lesson:** an interrupt is a function call — caller-saved registers must be
preserved, or every interrupted loop is a latent corruption.

---

## The "vector-0 #DE" that wasn't

**Symptom:** every interrupt appeared as a #DE (vector 0); ISR never ran.

**Hunt (abridged):** PIT counter ran + IRQ pending at the PIC but never
delivered; LAPIC timer enabled → #DE with LAPIC ISR=0 (⇒ real CPU #DE?);
software `int $0x20` also "#DE'd"; gates verified perfect via physical dump.

**Root cause (compounded):**
1. crt0 relative `call kernel_main` ⇒ whole kernel ran at identity addresses
   (fixed: absolute jump).
2. The vector READ was at `[rsp+120]` = the stub's ERROR slot (0); the
   vector sits at `[rsp+112]`. Every interrupt "looked like" vector 0.

**Fix:** `mov rdi, [rsp+112]`.

**Verification:** interrupts deliver; `tick` advances; the 'r' vector-32
diagnostic confirmed delivery.

**Lesson:** dump RAW stack slots before trusting named struct fields; one
offset byte can masquerade as a hardware failure for an entire session.
