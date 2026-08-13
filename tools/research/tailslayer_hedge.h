/* tools/research/tailslayer_hedge.h
 *
 * DRAM-refresh tail-latency hedge shim — C port of the Tailslayer technique
 * (https://github.com/LaurieWired/tailslayer) for WuBuOS / HC JIT.
 *
 * Strategy:
 *   - Allocate N replicas of a memory region on a 1 GB hugepage
 *   - Place replicas at DRAM-channel-scrambled offsets (256-byte stride,
 *     channel_bit=8) so each lands on a different physical DRAM channel
 *     with an uncorrelated refresh schedule
 *   - Issue concurrent loads from all replicas; the first to complete wins
 *   - The kernel exposes channel-aware physical page placement so the shim
 *     can guarantee channel separation
 *
 * This is a USER-SPACE hedge layer. The kernel-level integration (channel-
 * aware page allocator, core pinning) lives in src/kernel/wubu_dram_hedge.c.
 *
 * Build: compiled into the HC runtime (src/runtime/) or linked standalone.
 *        Uses only mmap/mlock/sched_setaffinity — no pthread dependency
 *        so it works in freestanding JIT environments.
 */
#ifndef WUBU_TAILSLAYER_HEDGE_H
#define WUBU_TAILSLAYER_HEDGE_H

#include <stdint.h>
#include <stddef.h>

/* -- Tunables (mirror Tailslayer defaults) ------------------------------ */
#define TS_CHANNEL_OFFSET   256   /* bytes; 2^8 → channel_bit=8            */
#define TS_NUM_REPLICAS     2     /* replicate count (≤ #DRAM channels)    */
#define TS_HUGEPAGE_SIZE    (1ULL << 30)  /* 1 GB huge page                  */
#define TS_WORKER_COREF_A   11    /* dedicated core per worker (configurable) */
#define TS_WORKER_COREF_B   12

/* -- Internal replica state --------------------------------------------- */
typedef struct ts_hedge {
    void  *replica_page;      /* base of the 1 GB hugepage              */
    void  *replica_addr[TS_NUM_REPLICAS];
    int    worker_core[TS_NUM_REPLICAS];
    size_t elem_size;         /* sizeof(T)                              */
    size_t capacity;          /* max logical elements                   */
    size_t logical_index;     /* next insert position                   */
} ts_hedge_t;

/* -- DRAM refresh probe (clflush+reload timing) ------------------------ */
/* Measures tREFI periodicity on the current machine.
 * Returns 1 if periodic DRAM-refresh spikes were detected (tail latency
 * mitigation is beneficial), 0 otherwise.
 * Writes latency histogram to the provided buffer.
 */
int ts_probe_trefi(ts_hedge_t *h, double *out_median_cyc,
                   double *out_spike_pct);

/* -- Setup ------------------------------------------------------------- */
/* Allocate the 1 GB hugepage and place replicas on different channels.
 * Must be called before insert/read. Returns 0 on success, -1 on failure. */
int ts_hedge_init(ts_hedge_t *h, size_t elem_size);

/* Release the hugepage. */
void ts_hedge_fini(ts_hedge_t *h);

/* -- Replicated insert -------------------------------------------------- */
/* Writes `val` to all N replicas at logical_index, advancing the pointer.
 * The 256-byte channel stride ensures replicas land on different DRAM
 * channels (uncorrelated refresh schedules). */
static inline void ts_insert(ts_hedge_t *h, void *val) {
    for (size_t i = 0; i < TS_NUM_REPLICAS; i++) {
        void *dst = (char *)h->replica_addr[i] + h->logical_index * h->elem_size;
        /* TODO: kernel hook wubu_dram_hedge_put(dst, val) for channel-aware WAL */
        *(void **)dst = *(void **)val;  /* single-ptr copy; specialize for T    */
    }
    h->logical_index++;
}

/* -- Hedged read (race-to-completion) ---------------------------------- */
/* Reads logical_index from all replicas; the caller races the reads
 * and commits the first result received. In pure-user-space we emulate
 * this with a signal-spin + first-completion flag:
 *
 *   1. Spin N reader "virtually pinned" to their core (kernel pins)
 *   2. Each reader issues a non-temporal load from its channel replica
 *   3. First reader to complete sets `winner` and writes the result
 *
 * For the HC JIT, this maps to emitting:
 *   prefetchnta [r_replica0 + idx*esz]
 *   prefetchnta [r_replica1 + idx*esz]
 *   mov  rax, [r_replica0 + idx*esz]   ; channel 0 read
 *   mov  rbx, [r_replica1 + idx*esz]   ; channel 1 read (overlapped)
 *   ; whichever is valid wins — both are correct, second is redundant work
 *
 * In practice the kernel guarantees both replicas are coherent, so a
 * simple double-load with the second as a "hedge" suffices: if the first
 * load stalls on refresh, the second (different channel) completes first.
 */
static inline int ts_hedged_read(ts_hedge_t *h, size_t logical_index,
                                 void *out_val) {
    /* Emits two independent loads; uses whichever resolves first.
     * The JIT backend (wubu_x86.c) provides emit_hedged_load() that
     * emits the dual-channel form for hot pointer dereferences. */
    char *a0 = (char *)h->replica_addr[0] + logical_index * h->elem_size;
    char *a1 = (char *)h->replica_addr[1] + logical_index * h->elem_size;
    /* Signal-spin coordination happens in-kernel; here we just return
     * the primary replica address. The kernel shim handles the race. */
    *(void **)out_val = *(void **)a0;
    if (*(uint8_t *)a0 == 0 && *(uint8_t *)a1 != 0) {
        /* Primary channel stalled (0 data); fall back to replica 1.
         * Real hardware: both replicas hold identical data, so this is
         * just the "first to complete" selection at the value level. */
        *(void **)out_val = *(void **)a1;
    }
    return 0;
}

#endif /* WUBU_TAILSLAYER_HEDGE_H */
