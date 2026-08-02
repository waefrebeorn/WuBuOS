# Triple Devil's-Advocate Audit — AGI OS Design & Build Plan

> Three adversarial passes over `AGI_OS_DESIGN.md` + the metal-first plan, done
> in the weeds (2026-08-02) because the weeds are where the design's real
> problems live. Each issue below was verified against the running code where
> possible; "clean" means the design holds. Output: the dedicated sub-module
> strategy (resources / dependencies / tools) for the build.

---

## DA-1 — Design soundness (the architecture itself)

### 1.1 REAL — Ring-0-everything has no crash isolation for the AGI's brain
TempleOS's all-ring-0 model means ONE bug = total loss. This session alone
proved it (a one-byte stack offset killed the system five times). The design
says "single address space" but the AGI's memory (hive, weights, trace, the
measured core) MUST be protected orthogonally — the SASOS lesson (Opal/Mungi:
protection orthogonal to translation). Without it, a buggy overlay driver can
corrupt the Colonel's brain.
**Strategy → `wubu_capguard`:** capability-style segment guards around the
Colonel's core regions (the hive, the AGI trace, the attestation table, the
kernel code pages = read-only after boot). Faults in overlays are contained
even at ring 0.

### 1.2 REAL — The anti-cheat claim needs runtime integrity, not just boot
"Measured boot proves the state" is necessary but NOT sufficient: boot
attestation proves the image, not the runtime. Vanguard's real power is
boot-time PLUS runtime scanning. The design needs DIM (Dynamic Integrity
Measurement) — periodic hashing of the ring-0 code pages + runtime PCR
extension — so the game server's attestation covers the CURRENT state.
**Strategy → `wubu_anticheat`:** core-integrity scanner (hash code pages on a
tick), runtime PCR extension, per-overlay attestation scopes, IOMMU policy
(DMA remapping enforced in the measured firmware).

### 1.3 REAL — "Absorb every OS" needs a compatibility TIER, not one mechanism
CP/M .COM, a DOS game, a Win32 game, a Linux CLI, a macOS tool, and a
foreign kernel cannot all run through one path. The design must classify
programs into explicit tiers with a router:
- **Tier 0** — 8086/16-bit → interpreter (`wubu_dos_emu` — exists)
- **Tier 1** — 32/64-bit foreign binaries → VSL syscall translation
- **Tier 2** — containerized native overlays (.wubu + bwrap)
- **Tier 3** — foreign kernels → EPT-style virtualized overlays
- **Tier 4** — museum (OS/2, BeOS, Amiga…) → emulation, best-effort
**Strategy → `wubu_personality`:** the personality registry (per-OS
translation tables + tier tags + a loader router that extends `wubu_exec`).

### 1.4 REAL — Human access "hedged through Bonzi" is not enforced
The serial console is full ring-0 today — anyone with the console owns the
machine. The hedged-human model needs a gateway: the human's commands are
mediated (Bonzi is the face), with an explicit permission model (what the
human may run vs what the Colonel owns).
**Strategy → `wubu_human`:** the human-command gateway — allowlist/denylist
per command class, audit every human action into the AGI trace, and the
Colonel remains the principal.

### 1.5 REAL — No defined boundary between levels 0 and 1
The design draws levels but not their interface. In a single address space,
"user space" is a set of equal-privilege subsystems — so the design must say
so explicitly: level-1 = a subsystem registry (the Win98 shell, the browser,
games = subsystems with ring-0 privileges) + the capability guards from 1.1.
**Strategy:** fold the subsystem registry into `wubu_personality`; the
boundary is the capability guard, not a ring.

### 1.6 CLEAN — The magical-Colonel feel (Genera heritage) is sound
Live-everything, inspect-any-memory, change-and-apply, whimsy-as-feature:
this is exactly the right north star and the live console + HolyC JIT plan
deliver it. No change.

---

## DA-2 — Implementation reality (what the plan needs that metal lacks)

### 2.1 REAL — There is NO virtual memory subsystem on metal
The design's centerpiece is the single-level store, but the metal kernel has:
identity + higher-half maps, a #PF handler that HALTS, no paging, no swap,
no fault-in. The memory design's foundation does not exist.
**Strategy → `wubu_vmm` (P0):** page allocator, demand paging, swap to the
store, a real #PF handler (fault-in instead of halt). This unblocks
**`wubu_segments` / `wubu_store` (P1):** the single-level store — persistent
segments backed by the fs, the "restart = the world reappears" promise.

### 2.2 REAL — No network stack on metal, yet the design promises internet
"Instantly download a lot of things" requires a NIC driver + TCP/IP + DNS +
TLS + HTTP **on metal**. `wubu_network` is hosted-only; the firmware already
has an e1000 driver (`fw_e1000`) to port.
**Strategy → `wubu_net` (P1):** port the e1000 driver, a minimal IPv4/TCP/UDP
stack, DNS + HTTP(S) client on metal. P1 because the seeded content store
can ship first; P0 once the internet promise needs delivery.

### 2.3 REAL — The AGI self-improve loop is dormant: NO VERIFIER on metal
Promotion requires `k->verifier` — nothing sets it on metal, so the loop is
dead by design. The first real verifier should be the kernel's own test
suite: a change only promotes if it passes the gate.
**Strategy → `wubu_verifier` (P0):** the test-suite-as-verifier — runs the
kernel's self-tests, scores spans, signs promotions. This activates the
whole attestation-gated self-improve loop that is currently dormant.

### 2.4 REAL — Preemption is broken (#GP on the 2nd preempt) and the design
depends on a scheduler
The cooperative model is stable; the timer PREEMPT switch corrupts the
resumed iret frame. The multi-task Colonel (agent + bonzi + console +
drivers) needs preemption + synchronization.
**Strategy → fix `tasking_switch.S` resume path + `wubu_sync` (P0):**
spinlocks, ISR-safe FIFO queues, the disable-interrupt discipline — the
primitives every future driver/ISR needs.

### 2.5 REAL — The HolyC JIT port needs kernel gaps filled first
The compiler needs: `strtod`/`strtol` (kernel libc lacks them), executable
memory (mem_alloc is executable since the PTEs lack NX — OK but unverified),
and hosted-runtime shims (`holyc_runtime.c` uses fopen/fputs → klog/serial).
**Strategy → `wubu_holyd_metal` (P0):** the REPL daemon on metal — persistent
globals, live eval from the console, the JIT in ring 0. Kernel-libc additions
(strtod/strtol) are a prerequisite.

### 2.6 REAL — No crash recovery beyond reboot
Ring-0 bugs will happen (this session!). The measured boot chain reboots
clean, but state is lost. Phantom OS's promise — the world returns — needs a
journal: the AGI trace + hive persisted to the store with a recoverable
checkpoint.
**Strategy:** fold checkpoints into `wubu_store` (P1): periodic hive+stack
snapshots; on boot, recover the last checkpoint instead of cold start.

---

## DA-3 — Process & verification (how we know it works)

### 3.1 REAL — Metal verification is manual (boot + grep + eyeball)
The run-agi.sh assertions help, but the live-console iteration is
hand-driven. The fix discipline needs an in-QEMU test harness that drives
the console and asserts outputs programmatically.
**Strategy → `test_metal_console` (Makefile target):** boot, wait for the
prompt, send commands, assert the responses (uptime advances, pci lists the
xhci, agi status sane). Every metal change ships with its console assertions.

### 3.2 REAL — "Tests ≠ correct" applies to the frame fixes
The InterruptFrame fix (extra rax field) was verified by reading raw frame
slots — good. But the preempt #GP and the bonzi memset#PF (caller
`wubu_bonzi_tick.part.0` @ 0xffffffff8010cdee → memset at 0xff000000 — likely
the heartbeat `snprintf(span,…)` or the input path) are NOT yet root-caused
with certainty. DA rule: verify REAL before patching — disassemble the
caller, find the exact memset call site.

### 3.3 REAL — The design doc has no interface contracts
Levels 0-3 need explicit ABI/API contracts (the subsystem registry, the
personality tables, the store segment format, the attestation wire format —
the last one exists via `fw_agi_attest.h` ✓). Without contracts, parallel
builds collide.
**Strategy:** each new sub-module ships with its header as the contract; the
single source of truth pattern (`fw_agi_attest.h`) is the model.

### 3.4 CLEAN — The measured boot + attestation chain is the right root
WuBuFW → PCR4 → kernel digest → attestation table → promotion gate: verified
green on metal, and it is the foundation every other trust claim stands on.
Keep it; extend it (runtime PCRs) rather than replace it.

---

## The dedicated sub-module plan (resources / dependencies / tools)

| Module | Tier | Depends on | Purpose |
|--------|------|-----------|---------|
| `wubu_vmm` | P0 | — | Page allocator, demand paging, swap, real #PF fault-in |
| `wubu_sync` | P0 | — | Spinlocks + ISR-safe FIFO queues (preemption prerequisite) |
| `wubu_verifier` | P0 | test suite | Test-suite-as-verifier → activates the AGI promote loop |
| `wubu_holyd_metal` | P0 | libc strtod/strtol, exec-mem | HolyC JIT REPL daemon on metal (live dev) |
| `wubu_segments` | P1 | wubu_vmm, wubu_store | Single-level store: persistent segments |
| `wubu_store` | P1 | fs, vmm | Backing store + checkpoints (Phantom-style resume) |
| `wubu_net` | P1 | fw_e1000 port | NIC + IPv4/TCP/UDP + DNS + HTTP(S) on metal |
| `wubu_anticheat` | P1 | attestation, vmm, sync | Core-integrity scan, runtime PCRs, overlay scopes, IOMMU policy |
| `wubu_personality` | P1 | VSL, wubu_exec, dos_emu | Personality registry + tier router (absorb-everything) |
| `wubu_capguard` | P1 | vmm | Capability guards on the Colonel's core memory |
| `wubu_editor` | P1 | vbe, console | Live text editor (Genera feel) on fb + serial |
| `wubu_human` | P1 | console, bonzi, trace | Hedged human-command gateway (allowlist + audit) |

### Fix-first (this week's weeds)
1. Bonzi tick memset@0xff000000 — disassemble `wubu_bonzi_tick.part.0` and
   find the exact call site (heartbeat snprintf vs input path) before patching.
2. Preempt-switch #GP — the resumed iret frame corruption in `tasking_switch.S`.
3. InterruptFrame struct — DONE (rax removed, field order aligned to the asm
   pushes; verified via raw frame reads).

## DA verdict
The design's spine is sound (measured ring-0 Colonel + Wayland surface +
VSL overlays + attestation anti-cheat). The real work is the missing FOUNDATION
(VM, sync, verifier, network, compiler-runtime gaps) — that is what makes the
"magical" promises true. Build the P0 tier first; the rest hangs off it.
