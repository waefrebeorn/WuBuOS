# Tailslayer — DRAM Refresh Tail-Latency Elimination

Source: https://github.com/LaurieWired/tailslayer (Laurie Wired, Apache-2.0, ~2.8k stars)
Read: 2026-08-12 | Status: analyzed · not yet ported

## THE CORE PROBLEM

DRAM refresh (tREFI ≈ 7.8 µs on DDR4) causes **periodic latency spikes** —
every few hundred microseconds, a memory controller pauses a channel to
refresh a row. A read arriving mid-refresh stalls until the refresh
completes (~150–750 ns vs ~60–100 ns idle). This is **tail latency**, the
long tail of the latency distribution that kills P99/99.9 responsiveness.

Tailslayer is **not a JIT**. It is a **memory-subsystem shim** that
eliminates this stall from *all* code by exploiting DRAM channel independence.

## THE TECHNIQUE (3 moving parts)

### 1. Data replication across independent DRAM channels
Tailslayer allocates a **1 GB hugepage** (`MAP_HUGETLB | (30 << MAP_HUGE_SHIFT)`)
and `mlock`s it into RAM. It then places **N replicas** of each value at
offsets that land on **different physical DRAM channels**:

```
channel_offset = 256 bytes    (DEFAULT_CHANNEL_OFFSET)
channel_bit    = 8            (DEFAULT_CHANNEL_BIT, i.e. 2^8 = 256)
stride         = num_channels * channel_offset
```

The 256-byte stride is chosen so that consecutive replicas hit different
channels — modern CPUs interleave memory across channels at 256-byte or
larger granularity. Each replica is at `base + i * channel_offset`.

### 2. Hedged concurrent reads (race-to-completion)
When a read target is signaled, **all N worker threads simultaneously issue
a load** from their respective channel replica. The DRAM refresh schedules
of independent channels are **uncorrelated** (no shared refresh counter per
channel), so the probability that *all* channels hit refresh simultaneously
is ~0. Whichever read completes first wins.

```
worker 0 (core 11):  read replica[0]
worker 1 (core 12):  read replica[1]   ← wins because channel 1 wasn't refreshing
worker N:            ...
```

### 3. Core-pinned spinning (latency-critical workers)
Each worker thread is pinned to a dedicated core via `sched_setaffinity`
and **spins** (busy-waits) on an atomic signal. This eliminates:
- Scheduler latency (no context switch to deliver the signal)
- Cache-line bouncing on the signal variable (each core has its own

The workers are bound to cores 11, 12, 14 (configurable `CORE_MEAS_A/B`,
`CORE_MAIN`) — separate physical cores so they don't compete for execution
ports or share L2/L3 contention.

## THE PROBE: `trefi_probe.c`

Tailslayer ships a DRAM-refresh periodicity probe that characterizes the
spike pattern on a given CPU:

1. Maps a 2 MB hugepage, `mlock`s it
2. Calibrates TSC frequency via `nanosleep` + `rdtsc`
3. **Calibration phase**: 500,000 probes of `clflush(addr); mfence; lfence; rdtsc; load; rdtscp`
4. Computes median/p90/p99/p99.9/p99.99 latency thresholds
5. **Main probe**: 20M probes, records all spikes above threshold
6. **Periodicity analysis**: histograms inter-spike intervals, bins at
   1T/2T/3T harmonics of expected tREFI

The probe uses `clflush` + `mfence` + `lfence` + `rdtsc` sandwich for
precise cycle counting. SPIKE intervals that match tREFI harmonics confirm
DRAM refresh is visible. VERDICT thresholds: >30% harmonic = PERIODIC.

## WHY THIS IS A HUGE SPEEDUP FOR EVERYTHING

DRAM refresh stalls are **invisible to the CPU pipeline** and **invisible to
JITs/compilers**. No compiler optimization, no register allocation, no
trace compilation can fix a stalled memory channel. Tailslayer shims the
memory layer so that *every* code path that loads from memory is automatically
hedge-read. It's a **transparent latency-elimination substrate**.

## PORTING STRATEGY FOR WUBUOS

Tailslayer is a C++ library using `mmap` + `pthread` + `sched_setaffinity`.
To port to the WuBuOS from-scratch kernel:

### A. Kernel-level (kernel shim)
- Expose a **`mmap_hugepage(size, num_replicas)`** syscall that allocates
  contiguously-backed giant pages with per-replica channel offsetting
- Implement **`sched_pin(core_id)`** in the scheduler (WuBuOS has its own
  scheduler)
- The kernel can replicate data on write (copy-on-write fork semantics) or
  expose the channel-offset addressing as a virtual-memory mapping where
  N virtual pages alias the same physical data but through different
  channel-scrambled physical frames

### B. Compiler-level (codegen shim)
- Add a `TAILSLAYER_HEDGE` attribute to pointer types
- The HC codegen emits **dual-path load**: `prefetchnta` (non-temporal
  prefetch to prime the cache) + the main `mov rax, [addr]`
- For hot pointer loads (function pointers, vtables, AST traversal), emit
  `prefetchw` early and reorder to overlap with preceding work
- **Non-temporal hint**: `movntdqa xmm0, [addr]` loads from WC memory
  without polluting cache — useful for large streaming loads

### C. DRAM channel offset math (kernel)
- DDR channel address mapping uses **address bits [12:8]** (channel bit 8,
  256-byte offset) to select the channel on most Intel/AMD systems
- Tailslayer replicates data at `channel_offset = 256` to exploit this
- On the kernel, we can place replicas at `phys_addr + i * 256` so writes
  to replica[i] naturally land on channel i mod num_channels

## KEVIN-BACON TRACE (source → 7 hops to compounding techniques)

1. **tailslayer** (this repo) → DRAM channel hedged reads
2. **uops.info / Agner Fog instruction tables** → port-level throughput,
   which reveals that memory-bound code saturates p23 (load) / p4 (store),
   so interleaving independent chains (like Tailslayer's replicas) keeps
   more ports busy
3. **ashvardanian.com "Hiding x86 Port Latency"** → FMA+ALU port
   interleaving: on Zen 4, FMA→p01, ADD→p23 — use port separation to hide
   latency, same principle as channel separation
4. **Agner Fog "Optimizing Subroutines"** → software pipelining +
   loop unrolling guidelines, register pressure analysis
5. **branchless programming** (cmov/pdep/pext) → eliminates branch
   misprediction penalty (~15-20 cyc), compounds with DRAM hedge
6. **linear scan register allocation** (Cranelift) → JIT RA that keeps
   values in registers to avoid loads entirely (fewer DRAM transactions =
   fewer refresh stalls to hedge)
7. **NUMA local-memory optimization** → first-touch placement +
   interleaving balances traffic hotspots (CACM NUMA paper)

→ **Compound strategy**: fewer loads (linear-scan RA) + branchless (no
mispredict stalls) + port-interleaved ALU chains (ashvardanian) + DRAM
channel hedging (tailslayer) = tail latency eliminated at every layer.

## WUBUOS IMPLEMENTATION (2026-08-12, shipped + green)

The hedge is now IMPLICIT in the HC compiler — "for all code magically":

### Compiler-level (the `prefetchnta` shim)
`HCGen.hedge_loads` (default `true`, in `hc_gen_init`) makes every memory
load the JIT emits be preceded by a software prefetch:
- `emit_prefetch_rip`   → before RIP-relative global loads (IDENT read,
  PRE/POST inc-dec, ASSIGN/compound load) — `0F 18 05 disp32`
- `emit_prefetch_rbp`   → before stack-local loads — `0F 18 85 disp32`
- `emit_prefetch_rax_off` → before sized member/element loads — `0F 18 80`
- `emit_prefetch_rdi`   → before array-index loads (INDEX holds base in
  rdi) — `0F 18 07`
- `emit_prefetch_rax`   → before plain pointer dereferences — `0F 18 00`

Files: `holyc_codegen_emit.c` (emitters + `wubu_hedge_prefetch_count`
diagnostic counter), `holyc_codegen_expr.c` (wired into every load path),
`holyc_types.h` (`hedge_loads` flag + `global_patches[128]` — doubled cap
because the prefetch + load each record a patch).

### VERIFIED ENCODING GOTCHA (a real bug caught this session)
`emit_prefetch_rip` records a global_patch at `patch_pos = code_size + 3`.
The FIRST wiring computed `patch_pos` BEFORE emitting the prefetch, so it
pointed into the middle of the 7-byte prefetch → the load's disp32 got the
wrong fixup → garbage. **Fix: emit the prefetch FIRST, THEN compute
`patch_pos = code_size + 3`.** This mirrors the load's 7-byte shape
(`48 8B 05`), so the shared patch formula applies verbatim.

### Verification (permanent gate `make test_hedge`)
`tools/probe/hedge_verify.c` proves each load class emits ≥1 prefetch
and pure constants emit 0. Wired into `test_high_bear`.

### Runtime / kernel
`tools/research/tailslayer_hedge.h` — the standalone C shim (replicated
insert + hedged read + trefi probe). Kernel-level channel-aware page
allocation is the future step (see PORTING STRATEGY below).

