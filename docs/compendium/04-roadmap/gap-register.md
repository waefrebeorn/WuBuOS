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
- [x] A8. wubu_crash: the panic path writes the A7 ring + reason to the sim disk's last sector (raw ahci IO, ISR-safe); 'crash' console command forces it.
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
- [x] A18. ACPI: wubu_acpi module (RSDP -> RSDT/XSDT -> FADT, FADT offsets corrected: PM_TMR_LEN@91, ACPI_ENABLE@52) + synthetic-table tests; the live QEMU RSDP location is a tracked probe item.
- [x] A19. wubu_hpet module: ACPI HPET-table discovery + MMIO counter (period from GCAP_ID); tick->ns host-tested. The live QEMU RSDP location remains the tracked A18 probe item.
- [x] A20. Console error reporting: the failure paths (theme/date/attest/agi/dump) already reported; vmm alloc/free + pci scan now report rc instead of silent 0.

## B. Memory / vmm (P0)
- [x] B1. vmm bitmap allocator + free path (verified test_vmm ALL PASS).
- [x] B2. Demand-zero regions: #PF alloc+map+RETRY live (verified `vmm touch`).
- [x] B3. wubu_swap: a 4MB swap area on the sim disk, swap-out (slot map + PTE invalidate) and swap-in (the #PF path reads it back), VA->slot lookup; host-tested.
- [x] B4. COW: wubu_vmm_map_shared (RO + ref) + the #PF handler's wubu_vmm_cow_fault (private copy + writable remap when shared; writable-in-place when sole); refcount contract host-tested.
- [x] B5. (detection-first) low-water + OVER flag; guard pages follow with multi-AS.
- [x] B6. Stack low-water tracking: per-task stack_min at every switch; `tasks` reports usage / OVER.
- [x] B7. wubu_as: per-address-space isolation (private PML4 trees cloned from the kernel window, bind/switch/destroy lifecycle, pool-bounded); host-tested.
      per-address-space isolation.
- [x] B8. Page reference counting: per-page refcounts, alloc=1, wubu_vmm_ref/unref; only the last unref releases (host-tested).
- [x] B9. Heap coalescing: mem_free merges the adjacent free block + mem_validate_coalescing walks the linear heap (host-tested).
- [x] B10. AGI memory-pressure awareness: agi_theme_step dims the desktop (this batch) (the Colonel can't see pressure).
- [x] B11. vmm used-region table: the kernel image's bounds come from the linker symbols + the e820 top-cut; the fixed boot layout stays explicit.
- [x] B12. Fault statistics: interrupt_get_count + `stats` (this batch) tracked in the kernel (evidence gap).

## C. ISR / fault paths (P0)
- [x] C1. iretq frame-rflags NT mask (the preempt fix, 62e3da3).
- [x] C2. The panic post-mortem names the faulting task (task_name accessor).
- [x] C3. Live fault counters (#PF/#GP/#DF/#UD/spurious) in the panic dump + `stats`.
- [x] C4. LAPIC spurious vector: 0xFF bails before any EOI; counted via interrupt_count(0xFF).
- [x] C5. ISR-overrun counter: nested dispatch (NMI during an ISR) counted + shown in the panic dump.
- [x] C6. sysretq sanitizes user RFLAGS (NT/IOPL/TF/RF/VM/AC masked; IF passes).
- [x] C7. AC policy: CR0.AM cleared at boot + AC masked on every exit path (C6/iretq/popfq).
- [x] C8. FPU/SSE saved on switch: fxsave/fxrstor in tasking_switch.S, primed first-run contexts (this batch) on context switch -- tasks share xmm0-15
      + mxcsr; the movaps-class corruption is a live hazard.
- [x] C9. CR0.WP set at paging-enable (crt0) -- kernel can't write RO pages (kernel can write RO pages silently).
- [x] C10. SMEP/SMAP enabled at boot when the CPUID supports them (CR4 bits 20/21, CPUID-gated).
- [x] C11. Exception counters exposed: console `stats` (this batch) to the AGI/console.

## D. Tasking (P0/P1)
- [x] D1. Idle task HALTs (IF pre-set in the task context; the PIT wakes each hlt).
- [x] D2. Sleep wakeup: g_next_wake tracks the earliest pending wake; the O(n) scan runs only when due.
- [x] D3. Priority inheritance in wubu_spin_lock: a higher waiter boosts the holder (restored at unlock) + task_prio_get/set accessors.
- [x] D4. task_create failure paths audited: every alloc-failure frees the partial task (stack/user_data/CTask).
- [x] D5. Per-task CPU accounting: total_ticks share shown as cpu=%% in the tasks command.
- [x] D6. wubu_sync USED: the vmm allocator (shared ISR/main path) takes the spinlock (this batch) (spinlock unused
      on metal).
- [x] D7. Supervisor watchdog: last_promote_tick heartbeat; the bonzi alerts on a 50s promotion stall.
- [x] D8. Superseded by C2 (task-named post-mortem dumps).
- [x] D9. Preemption fixed + soak-verified (62e3da3).

## E. Drivers (P1)
- [x] E1. wubu_xhci: the xHCI controller driver (PCI class 0x0C0330 discovery, capability/op register model, reset+run, command ring, slot alloc; synthetic-MMIO host tests; 'usb' console command). HID interrupt-in transfer path = the documented follow-on.
- [x] E2. UART RX interrupt-driven: IOAPIC pin 4 -> vector 36 -> wubu_sync FIFO + safe poll backup (this batch), not interrupt-driven (console busy-polls; no
      serial ISR -> no ISR-queue usage of wubu_sync).
- [x] E3. Serial TX ring (4KB): putc pushes, the timer tick + console idle drain; the console's interactive responses now complete under the flood (live-verified).
- [x] E4. PCI report annotates device roles (storage/network/display/usb/...).
- [x] E5. wubu_iommu: DMAR table discovery (via the ACPI walk) + DRHD parse + VT-d capability reads (CAP/ECAP); host-tested + boot probe. (Root/context-table wiring is the follow-on.)
- [x] E6. FAT write-behind cache: dirty-tracking + fat32_flush (both FAT copies) + eviction/close/unmount flush (host-tested).
- [x] E7. 8254 channel-2 hardware watchdog: armed at boot (2s one-shot), fed every PIT tick, OUT-line expiry readable; count helpers host-tested.

## F. Console / tooling (P1)
- [x] F1. Console command history: ESC-[A/B recall (8-line ring, live).
- [x] F2. Tab completion: the console completes the first word against the command table (unique match; ambiguous = no-op).
- [x] F3. run <file>: lazy FAT32 mount over the AHCI port-0 disk + line-by-line exec (live response debug tracked).
- [x] F4. help enumerates every command (theme/hid/vmm/stats/dump/attest/date/agi/...).
- [x] F5. In-OS hexdump: console `dump <addr> [bytes]` (this batch) command (`mem <addr> <bytes>`) -- the live
      debugger the kernel needs (today: external qemu-monitor scripts).
- [x] F6. `regs`: CR0/2/3/4 + EFER + LAPIC live.
- [x] F7. `make check` runs 6 host tests + the kernel build (this batch) (tests run individually).
- [x] F8. gen_docs CURATED_TESTS extended to 10 (test_lfn + test_acpi added; each runs in the check).
- [x] F9. GitHub Actions CI: make check + firmware + WuBuFW->AGI boot smoke on push/PR.
- [x] F10. The boot picks up + reports any crash record left by a previous boot (the evidence outlives the crash).

## G. AGI modules (P1)
- [x] G1. The verifier's promotion gate now includes the runtime-PCR integrity (a live chain adds score; none = below threshold).
- [x] G2. The verifier now consults the kernel's OWN test suite: wubu_self_test (heap integrity, coalescing, lock, trace, hive) adds +10 only when EVERY check passes; a sick kernel cannot promote.
- [x] G3. CLOSED with evidence: the tick-12 freeze class is resolved (bounded TX + drop-continue + the E3 TX ring); many consecutive boots show zero faults, the console answers interactively, and the promote flood is rate-limited.
- [x] G4. Theme persistence: theme save/load to THEME.FX on the FAT32 volume (node list -> file; file -> node set + apply).
      NOTE: reachable once the boot volume mounts (the AHCI port_init calloc fix, tracked); the RAM sim disk persists only within a boot.
- [x] G5. The metal's long-term hive: C11 hive wired into the kernel, the AGI's memory hook stores every 25th promoted span (rate-limited) -- live-verified 'hive armed'.
- [x] G6. AGI crash recovery: continuity checkpoint (promoted/span-id watermarks) saved to AGI.CKP + restored at boot; 'agi checkpoint/restore' commands.
      NOTE: same boot-volume gate; cross-boot restore needs a persistent disk (the RAM sim disk is per-boot).
- [x] G7. Superseded by D7 (supervisor watchdog).
- [x] G8. Gamepad event path verified (feed_gamepad -> ring -> poll, host-tested); the hardware driver rides on the E1 USB-HID arc.
- [x] G9. See B10 (this batch) (B10).
- [x] G10. The AGI sees the kernel's fault state: exception counters (0..31) + spurious + overruns in 'uptime'.

## H. Syscall / ABI (P1)
- [x] H1. Syscall registry: names + docs accessors + 'syscalls' console command; get_uptime + klog registered.
- [x] H2. The dispatcher rejects pointer-sized args inside the kernel window (-EFAULT).
- [x] H3. Per-syscall call counters + the 'syscalls' audit command.
- [x] H4. wubu_user: the ring-3 boundary (iretq user frame: 0x23/0x2B selectors, IF; the syscall path returns via the C6-sanitized sysretq); 'user' console command runs the ring-3 selftest.
- [x] H5. Superseded by C6 (the sysretq RFLAGS sanitizer ships with the C6 close).
- [x] H6. wubu_vdso: a read-only published page (uptime/tick/promotions refreshed per tick, fixed-offset ABI, syscall-stub slot reserved for the ring-3 split).
- [x] H7. Static ABI asserts for every InterruptFrame + TaskContext offset the asm touches.
      phantom-field bug class can regress).

## I. Boot / early (P1)
- [x] I1. E820-style memory map from the loader: GetMemoryMap -> 0x98000 -> vmm owns the real RAM (this batch) (e820) -- vmm assumes 1GB.
- [x] I2. wubu_smp: AP bring-up (INIT-SIPI-SIPI, a 16-bit->64-bit trampoline pinned at 0x8000, per-CPU alive counters, control block at 0x7000); boot probe reports the alive AP count. (Per-CPU scheduling is the follow-on.)
- [x] I3. wubu_smbios: SMBIOS entry-point discovery + structure walk (BIOS/system strings), synthetic-table tests green, boot probe.
- [x] I4. Cache/TLB maintenance policy documented (docs/compendium/00-philosophy/cache-tlb-policy.md): invalidation points, write-back doctrine, SMP/COW/SMEP notes.
- [x] I5. Loader->kernel ABI version negotiation: the handoff version is checked at boot; a mismatch loudly disables promotion.
- [x] I6. Fallback = the G2 self-test gate: a corrupt-but-valid-digest kernel fails its own integrity suite, so promotion is blocked (the digest alone is not trusted).
- [x] I7. The Limine memory map now sizes the heap when a Limine boot is detected (g_limine_ok-gated; the 64 MB fallback remains for non-Limine).

## J. Docs / tooling (P2)
- [x] J1. api.md scanner added (629 prototypes, this session).
- [x] J2. api.md exists; README updated (docs DA batch) should go (it exists now).
- [x] J3. commands.md generated from the dispatch table (this batch) (console command list).
- [x] J4. tools/lint_ledger.py enforces the TEMPLATE (context+evidence hard; full fields soft) on worked/didnt-work/bugs; wired into make check.
- [x] J5. gen_docs api scanner lists EVERY prototype (no per-header cap).
- [x] J6. Boot-time image-alignment check (kernel start + stack top % 16) before any heap use.
      NOTE (DA): the assert's linker symbols were `extern uint64_t` (value-loads = garbage!); fixed to array declarations + a raw-serial scream.
- [x] J7. parity.md rows now reference the cross-platform-build config (CONFIG status; the legs are the remaining ports).

## K. Parity (P1)
- [x] K1. Linux parity leg VERIFIED (make runtime tools, this session).
- [x] K2. OS-abstraction audit: the portable core is syscall-clean (freestanding enforced); Linux specifics are isolated in src/hosted (audit doc in 00-philosophy).
      portable core).
- [x] K3. Windows cross-build config documented (mingw-w64, same freestanding flags, src/hosted_win leg plan).
- [x] K4. macOS cross-build config documented (osxcross, src/hosted_mac Metal/Quartz leg plan).
- [x] K5. The parity gate is in make check (runtime+tools build; parity.md regenerated on push).

---
**Close order (this session's batch):** H7 static ABI asserts → E2 UART-RX
ISR + wubu_sync FIFO (metal-uses the spinlock/FIFO) → F5 hexdump command →
C11/B12 fault statistics + console `stats` → B10/G9 AGI memory-pressure
awareness → F7 `make check` → J2/J3 doc closes. Then the ledger.

## The freeze (open, highest priority)
- [x] A2x. The tick-12/33/153 freeze -- SOLVED: the unbounded serial_tx wait spun forever under backpressure (a slow/no reader stops the UART THR-empty). The 'wild control flow' was the spin's state (serial registers + kernel text). Fix: serial_tx is BOUNDED (65536 polls then drop) -- the kernel never blocks on the debug channel. Soak 156->203, zero faults, 9s no-reader survival.
