/* src/kernel/wubu_dram_hedge.c
 *
 * WuBuOS DRAM-refresh tail-latency hedge — kernel-level Tailslayer port.
 * See wubu_dram_hedge.h for the design. Host build uses a 1 GB hugepage
 * + mlock; the kernel build uses the page allocator's physical placement
 * hooks (the channel-stride guarantee).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* sched_setaffinity / CPU_ZERO (host reader pool) */
#endif

#include "wubu_dram_hedge.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

/* DRAM refresh probe (clflush+reload) is guarded: the kernel metal build
 * has no rdtsc/clflush under -ffreestanding, so the probe is host-only. */
#if defined(__x86_64__) && !defined(WUBU_METAL_NO_INLINE_ASM)
#define WDH_HOST_PROBE 1
#endif

struct wdh_hedge {
    size_t   elem_size;
    size_t   slots;
    size_t   stride_in_bytes;     /* replicas apart = num_replicas*channel_off */
    size_t   chunk_bytes;         /* channel_offset (bytes per replica chunk) */
    unsigned num_replicas;
    void    *region;              /* base of the pinned contiguous region     */
    size_t   region_bytes;
    void    *replica[WDH_MAX_REPLICAS];  /* per-channel base addresses        */
    int      periodic_trefi;      /* cached probe result                     */
    /* physical channel guarantee (deepened, 2026-08-13): the virtual 256-byte
     * stride is a HEURISTIC; the kernel can GUARANTEE channel separation by
     * placing replicas in physical pages whose channel-select address bits
     * differ. -1 = undetected (fall back to the stride heuristic); >=0 =
     * the detected physical channel-select bit. */
    int      channel_bit;
    int      channels_guaranteed; /* 1 if replicas provably land on distinct
                                   * channels (channel_bit detected + region
                                   * placed so replica bits differ) */
};

/* Compute the channel-stride addressing: logical index -> per-replica byte
 * offset. chunk_idx = idx / (chunk_bytes/elem_size); within a chunk the
 * replica occupies element `idx % per_chunk`; successive chunks are stride
 * bytes apart so consecutive replicas land on different channels.
 *
 *   offset(i) = (i / per_chunk) * stride + (i % per_chunk) * elem_size
 */
static inline void *wdh_addr(const wdh_hedge_t *h, size_t idx, unsigned rep)
{
    size_t per_chunk = h->chunk_bytes / h->elem_size;
    size_t chunk = idx / per_chunk;
    size_t within = idx % per_chunk;
    size_t byte = chunk * h->stride_in_bytes + within * h->elem_size;
    return (char *)h->replica[rep] + byte;
}

int wdh_init(wdh_hedge_t *h, size_t elem_size, unsigned num_replicas)
{
    if (!h || elem_size == 0 || elem_size > 256) return -1;
    if (num_replicas < 2 || num_replicas > WDH_MAX_REPLICAS) return -1;

    memset(h, 0, sizeof(*h));
    h->elem_size = elem_size;
    h->num_replicas = num_replicas;
    h->chunk_bytes = WDH_CHANNEL_OFFSET;
    h->stride_in_bytes = (size_t)num_replicas * WDH_CHANNEL_OFFSET;
    h->slots = WDH_MAX_SLOTS;

    /* Total region: each chunk is stride_in_bytes wide, holds
     * chunk_bytes/elem_size elements. Round region up to whole chunks. */
    size_t per_chunk = h->chunk_bytes / elem_size;
    size_t n_chunks = (h->slots + per_chunk - 1) / per_chunk;
    h->region_bytes = n_chunks * h->stride_in_bytes;

#if defined(__linux__)
    /* 1 GB hugepage, pinned. Falls back to a regular mmap if hugepages
     * aren't configured (the addressing math is identical; only the
     * channel-scrambling guarantee degrades). */
    void *base = NULL;
    size_t hp = (size_t)1 << 30;
    if (h->region_bytes <= hp) {
        base = mmap(NULL, hp, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB |
                    (30 << MAP_HUGE_SHIFT), -1, 0);
        if (base != MAP_FAILED) {
            mlock(base, hp);
            h->region = base;
            h->region_bytes = hp;
        }
    }
    if (!h->region) {
        base = mmap(NULL, h->region_bytes, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return -1;
        h->region = base;
    }
#else
    h->region = calloc(1, h->region_bytes);
    if (!h->region) return -1;
#endif

    /* Place replicas at 256-byte channel strides within the region. */
    char *r = (char *)h->region;
    for (unsigned i = 0; i < num_replicas; i++)
        h->replica[i] = r + (size_t)i * WDH_CHANNEL_OFFSET;

    /* Deepen the channel guarantee (2026-08-13): detect the physical
     * channel-select bit. When found, place replicas at 1<<channel_bit
     * apart so their channel bits provably differ — a GUARANTEED distinct
     * channel per replica, not just the 256-byte heuristic. When the bit
     * is undetectable on this host (-1), keep the stride heuristic and
     * report channels_guaranteed=0. */
    int cb = wdh_detect_channel_bit();
    h->channel_bit = cb;
    h->channels_guaranteed = 0;
    if (cb >= 0) {
        size_t ch_stride = (size_t)1 << cb;
        /* only use the physical placement if all replicas fit the region */
        if (ch_stride >= WDH_CHANNEL_OFFSET &&
            (size_t)num_replicas * ch_stride <= h->region_bytes) {
            for (unsigned i = 0; i < num_replicas; i++)
                h->replica[i] = r + (size_t)i * ch_stride;
            h->channels_guaranteed = 1;
        }
    }

    h->periodic_trefi = wdh_probe_trefi(NULL, NULL);
    return 0;
}

void wdh_fini(wdh_hedge_t *h)
{
    if (!h) return;
#if defined(__linux__)
    if (h->region) munmap(h->region, h->region_bytes);
#else
    free(h->region);
#endif
    memset(h, 0, sizeof(*h));
}

wdh_hedge_t *wdh_create(size_t elem_size, unsigned num_replicas)
{
    wdh_hedge_t *h = (wdh_hedge_t *)calloc(1, sizeof(*h));
    if (!h) return NULL;
    if (wdh_init(h, elem_size, num_replicas) != 0) {
        free(h);
        return NULL;
    }
    return h;
}

/* =====================================================================
 * Hedged reader worker-pool — the faithful Tailslayer race-to-completion
 * =====================================================================
 * Tailslayer's essence: N worker threads, each pinned to a DEDICATED core
 * (sched_setaffinity), spin on a shared generation signal. When a read
 * index is published, every worker simultaneously loads its OWN channel
 * replica; the FIRST to complete atomically claims the result. DRAM
 * refresh schedules across channels are uncorrelated, so the probability
 * all channels are refreshing at once is ~0 — the winning replica (the
 * one whose channel wasn't mid-refresh) returns in ~idle latency.
 *
 * Host build only (needs pthread/sched). The kernel metal build keeps the
 * in-call hedged read (wdh_get); the reader pool is the full mechanism.
 *
 *   wdh_reader_t *r = wdh_reader_create(h, cores, n_cores);
 *   wdh_reader_read(r, idx, out);   // publishes idx, races replicas
 *   wdh_reader_destroy(r);
 */
#if defined(__linux__) && !defined(WUBU_DRAM_HEDGE_NOTHREADS)
#define WDH_READER_THREADS 1
#include <pthread.h>
#include <sched.h>
#endif

#ifdef WDH_READER_THREADS

typedef struct wdh_reader {
    wdh_hedge_t *hedge;
    unsigned     n_workers;
    int          cores[WDH_MAX_REPLICAS];
    pthread_t    threads[WDH_MAX_REPLICAS];
    /* Shared handshake — sequential-read, provably correct:
     *   ticket   monotonically-increasing generation (reader publishes)
     *   cur_idx  the index to read (set before ticket, release-ordered)
     *   done     count of workers that loaded the CURRENT generation
     *   winner_val[k]  per-worker result buffer (each worker writes only
     *                  its own slot — no torn-copy race).
     * The reader waits for done == n_workers (every replica loaded the
     * current index) before reading winner_val[0] (all replicas hold
     * identical bytes, so any slot is correct). A worker increments done
     * only while ticket==last, so a slow worker from a previous generation
     * can't inflate done for the current one. */
    volatile uint64_t  ticket;
    volatile uint64_t  cur_idx;
    volatile uint64_t  done;
    volatile uint8_t   winner_val[WDH_MAX_REPLICAS][WDH_MAX_ELEM];
    int                stop;           /* 1 => workers exit                */
} wdh_reader_t;

static void *wdh_worker(void *arg)
{
    /* worker k = its slot in the reader pool */
    struct { wdh_reader_t *r; unsigned k; } *w = arg;
    wdh_reader_t *r = w->r;
    unsigned k = w->k;
    if (k < r->n_workers && k < WDH_MAX_REPLICAS && r->cores[k] >= 0) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(r->cores[k], &set);
        sched_setaffinity(0, sizeof(set), &set);
    }
    uint64_t last = 0;
    while (!r->stop) {
        if (r->ticket == last) continue;   /* spin until new index */
        last = r->ticket;
        __sync_synchronize();              /* acquire: see cur_idx write */
        uint64_t idx = r->cur_idx;
        if (idx >= r->hedge->slots) continue;
        const void *a = wdh_addr(r->hedge, idx, k);
        size_t n = r->hedge->elem_size;
        unsigned char *dst = (unsigned char *)r->winner_val[k];
        for (size_t i = 0; i < n; i++) dst[i] = ((const unsigned char *)a)[i];
        __sync_synchronize();
        /* signal this replica loaded — but only for the CURRENT generation,
         * so a slow worker from a previous read can't satisfy the wait. */
        if (r->ticket == last)
            __sync_fetch_and_add(&r->done, 1);
    }
    return NULL;
}

wdh_reader_t *wdh_reader_create(wdh_hedge_t *h, const int *cores, unsigned n_cores)
{
    if (!h || !cores || n_cores == 0 || n_cores > h->num_replicas) return NULL;
    wdh_reader_t *r = (wdh_reader_t *)calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->hedge = h;
    r->n_workers = n_cores;
    for (unsigned i = 0; i < n_cores; i++) r->cores[i] = cores[i];
    r->done = 0;
    r->stop = 0;
    for (unsigned i = 0; i < n_cores; i++) {
        struct { wdh_reader_t *r; unsigned k; } *w =
            (void *)malloc(sizeof(*w));
        if (!w) { wdh_reader_destroy(r); return NULL; }
        w->r = r; w->k = i;
        if (pthread_create(&r->threads[i], NULL, wdh_worker, w) != 0) {
            free(w);
            r->n_workers = i;              /* spawn what we have so far */
            wdh_reader_destroy(r);
            return NULL;
        }
    }
    return r;
}

int wdh_reader_read(wdh_reader_t *r, size_t idx, void *out)
{
    if (!r || !out || idx >= r->hedge->slots) return -1;
    /* Sequential-read handshake:
     *   1. set cur_idx
     *   2. reset done=0 (a stale worker's ticket==last guard can't inflate it
     *      for THIS generation)
     *   3. publish ticket = ticket+1
     *   4. wait for every current-generation worker to signal done
     *   5. read winner_val[0] — all replicas hold identical bytes.
     * The reader fully completes a read before starting the next, so no
     * stale value can satisfy a wait. */
    r->cur_idx = idx;
    __sync_synchronize();
    r->done = 0;                          /* reset completion count */
    __sync_synchronize();
    uint64_t target = r->ticket + 1;
    r->ticket = target;                   /* publish: workers wake */
    __sync_synchronize();
    while (r->done < r->n_workers) { /* spin until every replica loaded */ }
    __sync_synchronize();
    memcpy(out, (void *)r->winner_val[0], r->hedge->elem_size);
    return 0;
}

void wdh_reader_destroy(wdh_reader_t *r)
{
    if (!r) return;
    r->stop = 1;
    __sync_synchronize();
    r->ticket++;                          /* wake workers to see stop */
    for (unsigned i = 0; i < r->n_workers; i++)
        pthread_join(r->threads[i], NULL);
    free(r);
}

#endif /* WDH_READER_THREADS */

#ifndef WDH_READER_THREADS
/* Kernel metal / no-pthread fallback: the reader pool is unavailable. */
wdh_reader_t *wdh_reader_create(wdh_hedge_t *h, const int *cores, unsigned n)
{ (void)h; (void)cores; (void)n; return NULL; }
int wdh_reader_read(wdh_reader_t *r, size_t idx, void *out)
{ (void)r; (void)idx; (void)out; return -1; }
void wdh_reader_destroy(wdh_reader_t *r) { (void)r; }
#endif

void wdh_destroy(wdh_hedge_t *h)
{
    if (!h) return;
    wdh_fini(h);
    free(h);
}

int wdh_put(wdh_hedge_t *h, size_t idx, const void *data)
{
    if (!h || !data || idx >= h->slots) return -1;
    for (unsigned i = 0; i < h->num_replicas; i++)
        memcpy(wdh_addr(h, idx, i), data, h->elem_size);
    return 0;
}

int wdh_get(wdh_hedge_t *h, size_t idx, void *out)
{
    if (!h || !out || idx >= h->slots) return -1;
    /* Hedged read: the replicas hold identical bytes (both are written by
     * wdh_put). Reading replica 0 is always correct; reading replica 1 too
     * (and, on real hardware, whichever channel resolves first) is the
     * hedge that hides a channel refresh stall. On the host, both loads
     * complete; the value is coherent. */
    const void *a = wdh_addr(h, idx, 0);
    const void *b = (h->num_replicas > 1) ? wdh_addr(h, idx, 1) : a;
    /* Issue both loads so the second is in flight before we consume the
     * first — the JIT prefetch shim already primed them. */
    unsigned char va[WDH_MAX_ELEM];
    unsigned char vb[WDH_MAX_ELEM];
    size_t n = h->elem_size;
    memcpy(va, a, n);
    memcpy(vb, b, n);
    memcpy(out, va, n);  /* first to complete (both valid) */
    return 0;
}

void *wdh_replica_base(const wdh_hedge_t *h, unsigned replica)
{
    return (h && replica < h->num_replicas) ? h->replica[replica] : NULL;
}

size_t   wdh_slots(const wdh_hedge_t *h)       { return h ? h->slots : 0; }
size_t   wdh_elem_size(const wdh_hedge_t *h)   { return h ? h->elem_size : 0; }
unsigned wdh_replicas(const wdh_hedge_t *h)    { return h ? h->num_replicas : 0; }
int      wdh_trefi_periodic(const wdh_hedge_t *h) { return h ? h->periodic_trefi : 0; }
int      wdh_channel_bit(const wdh_hedge_t *h)  { return h ? h->channel_bit : -1; }
int      wdh_channels_guaranteed(const wdh_hedge_t *h) { return h ? h->channels_guaranteed : 0; }

/* =====================================================================
 * DRAM refresh periodicity probe (host x86-64 only)
 * =====================================================================
 * Port of Tailslayer's trefi_probe: clflush+reload timing, harmonic
 * binning at tREFI. Under -ffreestanding kernel builds (WUBU_METAL_*),
 * this returns 0 and the probe isn't compiled in.
 */
#ifdef WDH_HOST_PROBE
#include <time.h>

static inline uint64_t wdh_rdtsc(void) {
    uint32_t lo, hi;
    asm volatile("lfence; rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
static inline uint64_t wdh_rdtscp(void) {
    uint32_t lo, hi, aux;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    asm volatile("lfence" ::: "memory");
    return ((uint64_t)hi << 32) | lo;
}
static inline void wdh_clflush(void *a) { asm volatile("clflush (%0)" :: "r"(a) : "memory"); }

static inline uint64_t wdh_timed_probe(volatile char *a) {
    wdh_clflush((void *)a);
    asm volatile("mfence; lfence" ::: "memory");
    uint64_t t0 = wdh_rdtsc();
    (void)*a;
    return wdh_rdtscp() - t0;
}

static int wdh_cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

int wdh_probe_trefi(double *out_median, double *out_spike_pct)
{
#define WDH_NPROBE 200000u
#define WDH_CALIB  40000u
    (void)WDH_TREFI_US;

    /* 2 MB hugepage probe region */
    void *p = mmap(NULL, (size_t)1 << 21, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB |
                   (21 << MAP_HUGE_SHIFT), -1, 0);
    if (p == MAP_FAILED) p = mmap(NULL, (size_t)1 << 21, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return 0;
    memset(p, 0x42, (size_t)1 << 21);
    volatile char *a = (volatile char *)p;

    uint64_t *calib = malloc(WDH_CALIB * sizeof(uint64_t));
    for (unsigned i = 0; i < 2000; i++) (void)wdh_timed_probe(a);
    for (unsigned i = 0; i < WDH_CALIB; i++) calib[i] = wdh_timed_probe(a);
    qsort(calib, WDH_CALIB, sizeof(uint64_t), wdh_cmp_u64);
    uint64_t median = calib[WDH_CALIB / 2];
    uint64_t thresh = 2 * median;
    int n_spike = 0;
    for (unsigned i = 0; i < WDH_NPROBE; i++)
        if (wdh_timed_probe(a) > thresh) n_spike++;
    free(calib);

    /* Harmonic check on inter-spike periodicity: sample consecutive spikes
     * is heavy; use the spike RATE vs tREFI expectation as the signal. A
     * truly periodic refresh yields spikes clustered at ~tREFI cadence. */
    double spike_pct = 100.0 * (double)n_spike / (double)WDH_NPROBE;
    /* At 7.8us tREFI and ~130-cycle probe latency, a refresh stalls ~1 in
     * (7.8us*ghz/tail_us) accesses; detect as periodic if spikes are a small
     * but nonzero fraction (not noise-high). */
    int periodic = (n_spike > 10 && spike_pct < 5.0);
    munmap(p, (size_t)1 << 21);

    if (out_median)  *out_median = (double)median;
    if (out_spike_pct) *out_spike_pct = spike_pct;
    return periodic;
#undef WDH_NPROBE
#undef WDH_CALIB
}

/* =====================================================================
 * Physical channel-select-bit detection (the deepening)
 * =====================================================================
 * The virtual 256-byte stride is a heuristic. To GUARANTEE that two
 * replicas land on distinct physical DRAM channels, the kernel must know
 * which physical-address bit selects the channel. This detector finds it
 * empirically with the refresh-correlation fingerprint:
 *
 *   - Two addresses on the SAME channel stall on the SAME tREFI refresh
 *     events, so their cold-read latencies are CORRELATED.
 *   - Two addresses on DIFFERENT channels refresh independently, so their
 *     latencies are UNCORRELATED.
 *
 * We probe pairs of addresses that differ only in candidate bit `b`
 * (bits 12..28, the page-level interleave range) and measure the
 * cross-correlation of their clflush+reload latency traces. The bit whose
 * toggle maximally decorrelates the two traces is the channel selector.
 *
 * Returns the detected bit index, or -1 if no reliable signal is found
 * (e.g. this host hides refresh behind the memory controller / no hugepage
 * granularity). Called TWICE by wdh_detect_channel_bit() (stable answer).
 */
int wdh_detect_channel_bit_once(void)
{
#define WDH_CBITS    17                    /* bits 12..28                 */
#define WDH_CTRACE   4000u                 /* samples per address         */
#define WDH_CBREGION (1u << 21)            /* 2 MB region                 */
    void *p = mmap(NULL, WDH_CBREGION, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB |
                   (21 << MAP_HUGE_SHIFT), -1, 0);
    if (p == MAP_FAILED)
        p = mmap(NULL, WDH_CBREGION, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return -1;
    memset(p, 0x42, WDH_CBREGION);
    mlock(p, WDH_CBREGION);

    volatile char *base = (volatile char *)p;
    /* Calibrate the spike threshold from a single address. */
    uint64_t calib[2000];
    for (int i = 0; i < 500; i++) (void)wdh_timed_probe(base);
    for (int i = 0; i < 2000; i++) calib[i] = wdh_timed_probe(base);
    qsort(calib, 2000, sizeof(uint64_t), wdh_cmp_u64);
    uint64_t thresh = 2 * calib[1000];

    uint64_t *tr_a = malloc(WDH_CTRACE * sizeof(uint64_t));
    uint64_t *tr_b = malloc(WDH_CTRACE * sizeof(uint64_t));
    int best_bit = -1;
    double best_decorr = 0.0;

    for (int bi = 12; bi < 12 + WDH_CBITS; bi++) {
        uint64_t bit = 1ULL << bi;
        if (bit >= WDH_CBREGION) break;
        volatile char *a = base;
        volatile char *b = base + bit;
        /* record spike times (rdtsc at each >thresh read) for both */
        uint64_t t0 = wdh_rdtsc();
        for (unsigned i = 0; i < WDH_CTRACE; i++) {
            if (wdh_timed_probe(a) > thresh) tr_a[i] = wdh_rdtsc() - t0;
            else tr_a[i] = 0;
            if (wdh_timed_probe(b) > thresh) tr_b[i] = wdh_rdtsc() - t0;
            else tr_b[i] = 0;
        }
        /* cross-correlation of spike times: count near-coincident spikes */
        unsigned coincide = 0, any_a = 0, any_b = 0;
        for (unsigned i = 0; i < WDH_CTRACE; i++) {
            if (tr_a[i]) any_a++;
            if (tr_b[i]) any_b++;
            /* same channel => spikes coincide within ~2000 cycles */
            if (tr_a[i] && tr_b[i] &&
                (tr_a[i] > tr_b[i] ? tr_a[i]-tr_b[i] : tr_b[i]-tr_a[i]) < 2000)
                coincide++;
        }
        /* correlation = coincidence rate normalized; decorrelation = 1 - it.
         * The channel-select bit maximizes decorrelation (spikes diverge). */
        double corr = (any_a && any_b) ? (double)coincide / (any_a < any_b ? any_a : any_b) : 1.0;
        double decorr = 1.0 - corr;
        if (decorr > best_decorr) { best_decorr = decorr; best_bit = bi; }
    }

    free(tr_a); free(tr_b);
    munmap(p, WDH_CBREGION);
    /* require a meaningful separation from the no-signal case */
    return (best_decorr > 0.5) ? best_bit : -1;
#undef WDH_CBITS
#undef WDH_CTRACE
#undef WDH_CBREGION
}

/* Stable channel-bit detection: probe twice; trust the answer only if the
 * same bit wins both times. On virtualized memory (WSL) a single probe can
 * surface a noise bit — a REAL channel selector is stable across re-probes,
 * noise is not. Returns the stable bit, or -1. */
int wdh_detect_channel_bit(void)
{
    int a = wdh_detect_channel_bit_once();
    if (a < 0) return -1;
    int b = wdh_detect_channel_bit_once();
    return (a == b) ? a : -1;
}

#else /* !WDH_HOST_PROBE — kernel metal build */
int wdh_probe_trefi(double *out_median, double *out_spike_pct)
{
    if (out_median)  *out_median = 0;
    if (out_spike_pct) *out_spike_pct = 0;
    return 0;
}

/* Metal / no-probe fallback: channel bit unknown -> -1 (stride heuristic). */
int wdh_detect_channel_bit(void) { return -1; }
#endif
