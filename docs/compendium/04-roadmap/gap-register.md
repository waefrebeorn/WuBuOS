# Gap Register — 100+ bare-metal gaps (triple-DA hunt, 2026-08-02)

*Devil's-advocate gap hunt. Every item is VERIFIED against the code
(sweep: 53 kernel C files / 18K LOC, 29 TODO/stub markers, 132 silent
`return 0;`). Status: OPEN / CLOSED (with the commit). Priority P0 (kernel
correctness), P1 (subsystem), P2 (tooling/docs).*

## A. Kernel robustness (P0)
- [x] A1. ISR vector coverage: ALL 256 vectors get stubs; unknown vectors
      log a full raw frame + LAPIC state + halt (verified interrupt.c:392).
- [x] A2. Double-fault IST: wubu_tss IST1 (dedicated 8KB stack) + gate ist=1 (this batch): no IST stack -> a #DF in a #DF triple-faults
      to reset. CLOSE: give vector 8 an IST via wubu_tss.
- [x] A3. NMI (IST2) logs the hardware-error frame + dumps the panic ring.
- [x] A4. Watchdog: per-task stall counter (reset on yield); past 50s the stuck task is named + the ring dumped.
- [x] A5. Reaper: DYING tasks unlinked + freed in task context (main loop).
- [x] A6. Heap red-zone canaries: mem_validate_all wired into `mem` (canaries=OK live) (the 8GB
      alloca-in-loop bug class can regress silently).
- [x] A7. Panic ring: the klog captures the last 4KB in RAM; fault handlers dump it (post-mortem).
- [ ] A8. No crash dump to the disk (the ledger wants evidence, not dumps).
- [x] A9. Hex progress codes: the crt0 + metal_main boot markers emit 2-digit hex checkpoints (01 crt0 -> 37 final), verified live.
- [x] A10. Runtime PCR: wubu_sha256 (FIPS 180-4, own C11) + every promotion chains into a kernel-side runtime PCR; `attest` command verified live (rtPCR chained).
- [x] A11. PS/2 ack/self-test validation + device-ID handshakes (live: kbd 0x83 / mouse 0x00).
      device-id handshake.
- [x] A12. ps2 mouse feeds the unified HID ring (wubu_hid_feed_mouse).
      KeyEvent; MouseEvent unused by ps2).
- [x] A13. AHCI HBA reset (GHC.HR cycle) + port error recovery (SERR/PIS write-1-to-clear).
      cleared on error).
- [x] A14. AHCI multi-drive: ports 0+1 independent sim disks (write/read isolation host-tested).
- [x] A15. FAT32 dirty-volume flag: set at mount, cleared at clean unmount, reported on a crash-remount (host-tested).
- [x] A16. FAT32 LFN: wubu_lfn codec (encode/decode/chain, 9 tests) + dir-layer write integration; long-name create/find/list verified end-to-end.
- [x] A17. RTC wall clock: wubu_rtc module (CMOS 0x70/0x71, BCD/12h, UIP); `date` command + boot stamp verified live.
- [ ] A18. No ACPI/FADT parsing (firmware memory map is assumed, not read).
- [ ] A19. No HPET (the LAPIC timer is the only time source).
- [x] A20. Console error reporting: the failure paths (theme/date/attest/agi/dump) already reported; vmm alloc/free + pci scan now report rc instead of silent 0.

## B. Memory / vmm (P0)
- [x] B1. vmm bitmap allocator + free path (verified test_vmm ALL PASS).
- [x] B2. Demand-zero regions: #PF alloc+map+RETRY live (verified `vmm touch`).
- [ ] B3. No swap: demand pages are never evicted (the "VM+swap" P0 item).
- [ ] B4. No COW.
- [x] B5. (detection-first) low-water + OVER flag; guard pages follow with multi-AS.
- [x] B6. Stack low-water tracking: per-task stack_min at every switch; `tasks` reports usage / OVER.
- [ ] B7. Single address space (kernel+future user share CR3); no
      per-address-space isolation.
- [x] B8. Page reference counting: per-page refcounts, alloc=1, wubu_vmm_ref/unref; only the last unref releases (host-tested).
- [x] B9. Heap coalescing: mem_free merges the adjacent free block + mem_validate_coalescing walks the linear heap (host-tested).
- [x] B10. AGI memory-pressure awareness: agi_theme_step dims the desktop (this batch) (the Colonel can't see pressure).
- [ ] B11. The vmm's used-region table is hardcoded (no e820).
- [x] B12. Fault statistics: interrupt_get_count + `stats` (this batch) tracked in the kernel (evidence gap).

## C. ISR / fault paths (P0)
- [x] C1. iretq frame-rflags NT mask (the preempt fix, 62e3da3).
- [x] C2. The panic post-mortem names the faulting task (task_name accessor).
- [x] C3. Live fault counters (#PF/#GP/#DF/#UD/spurious) in the panic dump + `stats`.
- [x] C4. LAPIC spurious vector: 0xFF bails before any EOI; counted via interrupt_count(0xFF).
- [x] C5. ISR-overrun counter: nested dispatch (NMI during an ISR) counted + shown in the panic dump.
- [ ] C6. syscall exit (sysretq) has no rflags sanitization (the iretq has it).
- [ ] C7. No alignment-check (AC) policy.
- [x] C8. FPU/SSE saved on switch: fxsave/fxrstor in tasking_switch.S, primed first-run contexts (this batch) on context switch -- tasks share xmm0-15
      + mxcsr; the movaps-class corruption is a live hazard.
- [x] C9. CR0.WP set at paging-enable (crt0) -- kernel can't write RO pages (kernel can write RO pages silently).
- [ ] C10. SMEP/SMAP not enabled.
- [x] C11. Exception counters exposed: console `stats` (this batch) to the AGI/console.

## D. Tasking (P0/P1)
- [ ] D1. No idle task (the run loop busy-yields).
- [ ] D2. No sleep wakeup optimization.
- [ ] D3. No priority-inversion handling.
- [x] D4. task_create failure paths audited: every alloc-failure frees the partial task (stack/user_data/CTask).
- [x] D5. Per-task CPU accounting: total_ticks share shown as cpu=%% in the tasks command.
- [x] D6. wubu_sync USED: the vmm allocator (shared ISR/main path) takes the spinlock (this batch) (spinlock unused
      on metal).
- [x] D7. Supervisor watchdog: last_promote_tick heartbeat; the bonzi alerts on a 50s promotion stall.
- [x] D8. Superseded by C2 (task-named post-mortem dumps).
- [x] D9. Preemption fixed + soak-verified (62e3da3).

## E. Drivers (P1)
- [ ] E1. USB HID: wubu_usb.h is design-only (xHCI/HID implementation).
- [x] E2. UART RX interrupt-driven: IOAPIC pin 4 -> vector 36 -> wubu_sync FIFO + safe poll backup (this batch), not interrupt-driven (console busy-polls; no
      serial ISR -> no ISR-queue usage of wubu_sync).
- [ ] E3. No serial output buffering.
- [ ] E4. PCI report doesn't filter by class (no device roles).
- [ ] E5. No IOMMU/VT-d (the anticheat below-OS plane).
- [ ] E6. No disk cache flush policy.
- [ ] E7. No watchdog timer (the 8254/HPET not used as a WDT).

## F. Console / tooling (P1)
- [x] F1. Console command history: ESC-[A/B recall (8-line ring, live).
- [ ] F2. No tab completion.
- [ ] F3. No script execution ("run <file>").
- [ ] F4. Help text doesn't enumerate all commands.
- [x] F5. In-OS hexdump: console `dump <addr> [bytes]` (this batch) command (`mem <addr> <bytes>`) -- the live
      debugger the kernel needs (today: external qemu-monitor scripts).
- [x] F6. `regs`: CR0/2/3/4 + EFER + LAPIC live.
- [x] F7. `make check` runs 6 host tests + the kernel build (this batch) (tests run individually).
- [ ] F8. gen_docs tests scanner covers 6 of ~15 test targets.
- [ ] F9. No CI config.
- [ ] F10. No crash-file pickup (post-mortem from the metal).

## G. AGI modules (P1)
- [ ] G1. Verifier is static policy; DA-3 wants runtime PCRs.
- [ ] G2. Verifier doesn't consult the TEST SUITE results (the DA's
      "test-suite verifier" is not wired).
- [ ] G3. OPEN: the soft cap re-triggered the tick-12 freeze (timing-dependent corruption -- tracked).
- [ ] G4. Theme writes not persisted (reset each boot).
- [ ] G5. No long-term memory on metal (hive is hosted-side).
- [ ] G6. No AGI crash recovery (checkpoints, DA-2.6).
- [x] G7. Superseded by D7 (supervisor watchdog).
- [ ] G8. Gamepad events fed by nothing (no driver).
- [x] G9. See B10 (this batch) (B10).
- [ ] G10. No AGI-side fault awareness (C11).

## H. Syscall / ABI (P1)
- [ ] H1. Syscall table undocumented (numbers/return codes).
- [ ] H2. Syscall args (pointers) not validated.
- [ ] H3. No syscall audit log.
- [ ] H4. No ring-3 boundary (all ring 0; the hedged-human design needs it).
- [ ] H5. sysretq rflags sanitization (C6).
- [ ] H6. No vDSO/vsyscall page.
- [ ] H7. No static ABI asserts for InterruptFrame/TaskContext offsets (the
      phantom-field bug class can regress).

## I. Boot / early (P1)
- [x] I1. E820-style memory map from the loader: GetMemoryMap -> 0x98000 -> vmm owns the real RAM (this batch) (e820) -- vmm assumes 1GB.
- [ ] I2. No SMP (APs never started; single CPU).
- [ ] I3. No SMBIOS/DMI parsing (machine identity unknown).
- [ ] I4. No cache/TLB maintenance policy doc.
- [ ] I5. No firmware API version negotiation.
- [ ] I6. No fallback if the loaded image is corrupt-but-valid-digest.
- [ ] I7. Limine protocol accepted but unused.

## J. Docs / tooling (P2)
- [x] J1. api.md scanner added (629 prototypes, this session).
- [x] J2. api.md exists; README updated (docs DA batch) should go (it exists now).
- [x] J3. commands.md generated from the dispatch table (this batch) (console command list).
- [ ] J4. Ledger TEMPLATE not lint-enforced.
- [ ] J5. gen_docs api scanner caps 40 prototypes/header (truncation).
- [ ] J6. The kernel.ld ALIGN(16) fix has no boot-time assertion.
- [ ] J7. parity.md: Windows/macOS rows PLANNED with no leg files.

## K. Parity (P1)
- [x] K1. Linux parity leg VERIFIED (make runtime tools, this session).
- [ ] K2. Hosted core OS-abstraction audit (Linux-only syscalls in the
      portable core).
- [ ] K3. A Windows cross-build config.
- [ ] K4. A macOS cross-build config.
- [ ] K5. Parity regression in CI (K2-K4 gate).

---
**Close order (this session's batch):** H7 static ABI asserts → E2 UART-RX
ISR + wubu_sync FIFO (metal-uses the spinlock/FIFO) → F5 hexdump command →
C11/B12 fault statistics + console `stats` → B10/G9 AGI memory-pressure
awareness → F7 `make check` → J2/J3 doc closes. Then the ledger.

## The freeze (open, highest priority)
- [x] A2x. The tick-12/33/153 freeze -- SOLVED: the unbounded serial_tx wait spun forever under backpressure (a slow/no reader stops the UART THR-empty). The 'wild control flow' was the spin's state (serial registers + kernel text). Fix: serial_tx is BOUNDED (65536 polls then drop) -- the kernel never blocks on the debug channel. Soak 156->203, zero faults, 9s no-reader survival.
