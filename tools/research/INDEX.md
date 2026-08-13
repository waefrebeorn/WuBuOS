# Research Index — WUBUOS Optimization Research

Persistent research artifacts (NOT /tmp — these survive across sessions).
Last updated: 2026-08-12

## Tailslayer / DRAM Latency
- `notes_tailslayer.md` — Full deep-dive: DRAM-refresh tail-latency hedge
  via channel-replicated hedged reads. The technique is a memory-subsystem
  shim, not a JIT. Port strategy for WuBuOS kernel + HC codegen.
  **IMPLEMENTED**: HCGen.hedge_loads (default on) emits a `prefetchnta`
  before every memory load class. Gate: `make test_hedge`.
- `tailslayer_hedge.h` — C header shim: `ts_hedge_init/insert/read/probe`
  replicating Tailslayer's 1 GB hugepage + 256-byte channel-offset replication.
- `trefi_probe.c` — DRAM refresh periodicity probe (clflush+reload,
  harmonic binning at 1T/2T/3T of expected tREFI).

## x86-64 Microarchitecture
- `x86_speedup_cheatsheet.md` — Latency/throughput table (Zen 4, Intel
  ARL-P/Golden Cove), LEA-shift-multiply table, division-avoidance,
  branchless (cmov/sign-bit mask), port-pressure/ILP, cache hints,
  verified HC-JIT encodings. Sources: Agner Fog instruction tables
  (2025-09-20), uops.info.

## Kevin-Bacon Trace
`notes_tailslayer.md` ends with the 7-step Kevin-Bacon trace from
Tailslayer → channel interleaving → port interleaving → software
pipelining → branchless → linear-scan RA → NUMA topology.

## Related (to be added)
- [ ] pext/pext variable-bit latency deep-dive (microcode penalty)
- [ ] AVX-512 / AVX2 vector-width utilization for WUBU kernel math
