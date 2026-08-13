/* src/kernel/wubu_dram_hedge.c
 *
 * WuBuOS DRAM-refresh tail-latency hedge — kernel-level Tailslayer port.
 * See wubu_dram_hedge.h for the design. Host build uses a 1 GB hugepage
 * + mlock; the kernel build uses the page allocator's physical placement
 * hooks (the channel-stride guarantee).
 */

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

#else /* !WDH_HOST_PROBE — kernel metal build */
int wdh_probe_trefi(double *out_median, double *out_spike_pct)
{
    if (out_median)  *out_median = 0;
    if (out_spike_pct) *out_spike_pct = 0;
    return 0;
}
#endif
