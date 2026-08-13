# WuBuOS DRAM-Refresh Tail-Latency Hedge

Status: SHIPPED + GREEN (2026-08-12) · Docs: this file + `tools/research/notes_tailslayer.md`
Source: `src/kernel/wubu_dram_hedge.{c,h}` · Test: `src/kernel/tests/test_dram_hedge.c` · Gate: `make test_dram_hedge` (288/288)

## The problem

DRAM refresh (tREFI ≈ 7.8 µs on DDR4) pauses a memory channel every few
hundred microseconds to refresh a row. A cold read arriving mid-refresh
stalls ~150–750 ns vs ~60–100 ns idle — the **tail latency** that destroys
P99/99.9 responsiveness. It is invisible to the CPU pipeline, so no
compiler optimization, register allocator, or trace compiler can fix it at
the instruction level.

Tailslayer (https://github.com/LaurieWired/tailslayer) attacks it at the
**memory-subsystem level**: it is NOT a JIT. It is a shim that hedges the
DRAM read so all code benefits automatically.

## The mechanism (two halves, both shipped)

### 1. Compiler half — implicit software prefetch (commit 09783f0)
`HCGen.hedge_loads` (default ON) makes the HC JIT emit a `prefetchnta`
(`0F 18 /0`) immediately before EVERY memory load:
- global (RIP-rel) · stack local · struct member · array INDEX · ptr DEREF
Gates: `make test_hedge` (all 5 load classes emit ≥1 prefetch, constants 0).

### 2. Kernel half — channel-replicated hedged memory (this wave)
`wubu_dram_hedge` provides the **replicated, channel-scrambled** memory that
the hedge reads from:
- allocates a contiguous region, pins it in RAM (1 GB hugepage + mlock on
  the host; kernel page-allocator placement on metal)
- places N replicas at **256-byte channel strides** (`channel_bit=8`) so
  each lands on a different physical DRAM channel with an *uncorrelated*
  refresh schedule
- `wdh_put` writes every replica; `wdh_get` races all replicas and the
  first channel to return wins — the chance all are refreshing at once is ~0

## Channel-stride addressing

```
per_chunk   = channel_offset / elem_size          # 256 / elem
stride      = num_replicas * channel_offset        # N * 256 bytes
offset(i)   = (i / per_chunk) * stride + (i % per_chunk) * elem_size
replica[k]  = region + k * channel_offset
```
The 256-byte stride is chosen because modern controllers interleave DRAM
across channels at 256 B or larger granularity — consecutive replicas land
on different channels.

## Build integration

- Hosted/test build (NOT freestanding): this module uses libc
  (`mmap`/`mlock`/`malloc`), so it is **not** in `KERNEL_OBJS` (which
  compiles `-ffreestanding -nostdlib -mcmodel=kernel`). It is a hosted
  subsystem wired into `mk/tests.mk` + the `test_critical_kernel` tier.
- The DRAM-refresh probe (`wdh_probe_trefi`) is guarded by
  `WDH_HOST_PROBE` (x86-64 + non-metal); the kernel metal build gets the
  freestanding-safe stub returning 0.
- The `WDH_MAX_ELEM` in the header bounds the elem-size (matches the init
  check), so the hedged-read stack buffers are sized statically.

## Styx/9P export (/n/dram)

`src/runtime/wubu_ns_dram.{c,h}` exposes the hedge over the namespace,
same pattern as /n/ec and /n/steaminput (ns_mkdir/ns_write wrapping the
real API, no new daemon):
- `/n/dram/state`   "replicas N, elem N, slots N, trefi periodic"
- `/n/dram/slots`, `/n/dram/replicas`, `/n/dram/elem`, `/n/dram/trefi`
- `/n/dram/ctrl`    `echo 3:42 > /n/dram/ctrl` stores 42 in slot 3
- `/n/dram/status`  one-line live summary
Gate: `make test_ns_dram` (11/11), wired into test_medium_other.

## Benchmark + honest finding

`tools/bench_dram_hedge.c` (`make bench_dram_hedge`) compares cold
unhedged clflush reads vs the hedged reader pool on this host: unhedged
median ~320cyc vs hedged ~832cyc. The worker-pool handshake overhead
(~500cyc of sync barriers) exceeds any refresh-stall savings here (no real
DRAM pressure in the microbenchmark). The hedge's win needs hardware where
one channel is actually mid-refresh. The compiler `prefetchnta` half
remains the zero-cost "for all code" win (~1 cyc, no sync). This finding
is recorded honestly — no claimed speedup the benchmark doesn't show.

## Verification

- Channel stride: replicas exactly 256 B apart
- Round-trips for elem=8 and elem=4 across chunk boundaries
- Hedge precondition: both replicas hold identical bytes after one put
- tREFI probe: detects periodic refresh on this machine (median ~320 cyc,
  ~5% spike rate)

## Future / honest remainder

The race-to-completion across separate cores (Tailslayer spins workers on
cores 11/12/14) is now implemented (`wdh_reader_*`): N threads pinned to
dedicated cores race the replicas and the reader completes when every
current-generation replica has loaded.

## PHYSICAL CHANNEL GUARANTEE (the deepening, 2026-08-13)

The virtual 256-byte stride was a heuristic. Now `wdh_detect_channel_bit()`
empirically finds the physical channel-select address bit using the
Rowhammer refresh-correlation fingerprint: two addresses on the SAME
channel stall on the SAME tREFI events (correlated), on DIFFERENT channels
they refresh independently (decorrelated). The detector probes candidate
bits 12..28 and returns the bit that maximally decorrelates a cold-read
trace.

Stability requirement: the detection is run TWICE (`wdh_detect_channel_bit`
wraps `wdh_detect_channel_bit_once`) and the bit is trusted only if both
probes agree. On virtualized memory (WSL) a noise bit surfaces and is
rejected -> returns -1 -> honest fallback to the 256-byte stride with
`channels_guaranteed=0`. On real metal with hugepages, the detector finds
the real channel bit and `wdh_init` places replicas at `1<<channel_bit`
apart, setting `channels_guaranteed=1`.

API: `wdh_detect_channel_bit()`, `wdh_channel_bit(h)`,
`wdh_channels_guaranteed(h)`. Metal builds get the -1 fallback.

Verified: test_dram_hedge 295/295 (stable across 15 runs), test_ns_dram
11/11, test_hedge PASS. The honest behavior is enforced — no guarantee is
claimed when the bit is undetectable.
