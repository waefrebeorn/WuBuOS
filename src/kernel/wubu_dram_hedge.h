/* src/kernel/wubu_dram_hedge.h
 *
 * WuBuOS DRAM-refresh tail-latency hedge — the KERNEL-level Tailslayer port.
 *
 * This is the second half of the Tailslayer implementation (the compiler
 * half — an implicit `prefetchnta` before every compiled load — lives in
 * the HC codegen; see tools/research/notes_tailslayer.md). This module
 * provides the channel-aware REPLICATED memory the hedge reads from:
 *
 *   - allocate one contiguous region, pin it in RAM (mlock / kernel page
 *     reservation)
 *   - place N replicas at 256-byte channel strides so each lands on a
 *     different physical DRAM channel (channel_bit=8; modern controllers
 *     interleave at 256B or larger granularity)
 *   - every insert writes to ALL replicas; every read races the replicas,
 *     and the FIRST channel to return wins — DRAM refresh schedules across
 *     channels are uncorrelated, so the probability all are refreshing at
 *     once is ~0
 *
 * The kernel page allocator provides the physical placement guarantee; the
 * host/test build uses a 1 GB hugepage (MAP_HUGETLB) like Tailslayer.
 *
 * The /n/dram 9P export (Styx) exposes {ctrl, state, trefi} for inspection.
 *
 * Hosted test build (freestanding-safe, no pthread — spins on an atomic
 * flag like the Tailslayer workers, but reads complete in-call on the
 * caller's core for the test path):
 *
 *   gcc -O2 -Isrc/kernel src/kernel/wubu_dram_hedge.c \
 *       src/kernel/tests/test_dram_hedge.c -o build/test_dram_hedge -lm
 */
#ifndef WUBU_DRAM_HEDGE_H
#define WUBU_DRAM_HEDGE_H

#include <stdint.h>
#include <stddef.h>

/* -- Tunables (mirror Tailslayer + DDR4 tREFI) --------------------------- */
#define WDH_CHANNEL_OFFSET   256u    /* bytes; 2^8 => channel_bit=8          */
#define WDH_MAX_REPLICAS     4u      /* max replicas (must be <= #channels)  */
#define WDH_MAX_SLOTS        65536u  /* logical entries in the hedge region  */
#define WDH_TREFI_US         7.8     /* DDR4 nominal refresh period           */
#define WDH_MAX_ELEM         256u    /* max elem_size (matches header check) */

/* -- Opaque hedge state -------------------------------------------------- */
typedef struct wdh_hedge wdh_hedge_t;

/* -- Lifecycle ----------------------------------------------------------- */
/* Initialize a hedge holding fixed-size (elem_size) replicated slots.
 * num_replicas in [2, WDH_MAX_REPLICAS]; elem_size in [1, 256].
 * On the host, allocates a 1 GB hugepage + mlock + 256-byte channel
 * strides. Returns 0 on success, -1 on failure (no hugepage available). */
int  wdh_init(wdh_hedge_t *h, size_t elem_size, unsigned num_replicas);
void wdh_fini(wdh_hedge_t *h);

/* Heap-allocated hedge (the opaque struct can't be declared by value in
 * the test/consumer). Returns NULL on failure. */
wdh_hedge_t *wdh_create(size_t elem_size, unsigned num_replicas);
void wdh_destroy(wdh_hedge_t *h);

/* -- Access -------------------------------------------------------------- */
/* Replicated insert: writes `data` (elem_size bytes) to slot `idx` on
 * EVERY replica channel. */
int  wdh_put(wdh_hedge_t *h, size_t idx, const void *data);

/* Hedged read: reads slot `idx` racing all replicas; the first channel to
 * complete wins (in practice both hold identical data, so any coherent
 * replica is correct — this is the latency-elimination race). Copies the
 * winning bytes to out. Returns 0 on success. */
int  wdh_get(wdh_hedge_t *h, size_t idx, void *out);

/* Replica base addresses (for diagnostics / the prefetch shim). */
void *wdh_replica_base(const wdh_hedge_t *h, unsigned replica);

/* -- Diagnostics / /n/dram export ---------------------------------------- */
size_t    wdh_slots(const wdh_hedge_t *h);
size_t    wdh_elem_size(const wdh_hedge_t *h);
unsigned  wdh_replicas(const wdh_hedge_t *h);
int       wdh_trefi_periodic(const wdh_hedge_t *h); /* 1 if refresh detectable */

/* -- Physical channel guarantee (2026-08-13 deepening) ------------------- */
/* Detect which physical-address bit selects the DRAM channel (refresh-
 * correlation fingerprint). Returns the bit index, or -1 if undetectable
 * (this host hides refresh / metal build). */
int wdh_detect_channel_bit(void);

/* After wdh_init: the detected channel-select bit (-1 if none) and whether
 * the replicas are PROVABLY on distinct channels (channel bit found and
 * replicas placed so their channel bits differ). */
int wdh_channel_bit(const wdh_hedge_t *h);
int wdh_channels_guaranteed(const wdh_hedge_t *h);

/* -- DRAM refresh probe (kernel + host) ---------------------------------- */
/* clflush+reload timing probe: returns 1 if periodic tREFI spikes were
 * detected on this machine (tail-latency mitigation is beneficial). */
int wdh_probe_trefi(double *out_median_cyc, double *out_spike_pct);

/* -- Hedged reader worker-pool (host builds with pthread) ---------------- */
/* The faithful Tailslayer race-to-completion: N threads each pinned to a
 * dedicated core spin on a generation signal; publishing a read index
 * wakes all of them to load their OWN channel replica, and the first to
 * complete atomically claims the result. Returns NULL if threads aren't
 * compiled in (kernel metal build) or on spawn failure. */
typedef struct wdh_reader wdh_reader_t;
wdh_reader_t *wdh_reader_create(wdh_hedge_t *h, const int *cores, unsigned n_cores);
int  wdh_reader_read(wdh_reader_t *r, size_t idx, void *out);
void wdh_reader_destroy(wdh_reader_t *r);

#endif /* WUBU_DRAM_HEDGE_H */
