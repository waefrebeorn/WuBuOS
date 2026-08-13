/* tools/research/trefi_probe.c
 *
 * DRAM refresh periodicity probe — C port of Tailslayer's
 * discovery/trefi_probe.c. Detects periodic tREFI latency spikes via
 * clflush+reload timing.
 *
 * Build:  gcc -O2 -o tools/research/trefi_probe tools/research/trefi_probe.c -lm
 * Run:    sudo chrt -f 99 taskset -c 3 ./tools/research/trefi_probe
 *
 * Writes: CSV spike data to stdout, diagnostics to stderr.
 *         VERDICT line tells you if DRAM-refresh tail latency is present.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>
#include <getopt.h>

#define HUGEPAGE_2M       (1ULL << 21)
#define CALIB_PROBES      500000
#define MAX_SPIKES        2000000
#define DEFAULT_PROBES    20000000
#define DEFAULT_TREFI_US  7.8

static inline uint64_t rdtsc_lfence(void) {
    uint64_t lo, hi;
    asm volatile("lfence\n\trdtsc" : "=a"(lo), "=d"(hi));
    return (hi << 32) | lo;
}
static inline uint64_t rdtscp_lfence(void) {
    uint64_t lo, hi; uint32_t aux;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    asm volatile("lfence" ::: "memory");
    return (hi << 32) | lo;
}
static inline void clflush_addr(volatile void *addr) {
    asm volatile("clflush (%0)" :: "r"(addr) : "memory");
}
static inline void mfence_inst(void)  { asm volatile("mfence" ::: "memory"); }

static double calibrate_tsc_ghz(void) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    uint64_t tsc0 = rdtsc_lfence();
    struct timespec req = { .tv_sec = 0, .tv_nsec = 100000000 };
    nanosleep(&req, NULL);
    uint64_t tsc1 = rdtscp_lfence();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed_ns = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    return (double)(tsc1 - tsc0) / elapsed_ns;
}

static inline uint64_t timed_probe(volatile char *addr) {
    clflush_addr(addr);
    mfence_inst();
    asm volatile("lfence" ::: "memory");
    uint64_t t0 = rdtsc_lfence();
    *(volatile char *)addr;
    uint64_t t1 = rdtscp_lfence();
    return t1 - t0;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t va = *(const uint64_t *)a, vb = *(const uint64_t *)b;
    return (va > vb) - (va < vb);
}

int main(int argc, char **argv) {
    int n_probes = DEFAULT_PROBES;
    uint64_t manual_threshold = 0;
    double trefi_us = DEFAULT_TREFI_US;
    double thresh_mult = 2.0;

    static struct option long_opts[] = {
        {"probes",         required_argument, NULL, 'n'},
        {"threshold",      required_argument, NULL, 'T'},
        {"trefi-us",       required_argument, NULL, 't'},
        {"thresh-mult",    required_argument, NULL, 'm'},
        {"help",           no_argument,       NULL, 'h'},
        {NULL, 0, NULL, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "n:T:t:m:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'n': n_probes = atoi(optarg); break;
        case 'T': manual_threshold = strtoull(optarg, NULL, 0); break;
        case 't': trefi_us = atof(optarg); break;
        case 'm': thresh_mult = atof(optarg); break;
        case 'h': default:
            fprintf(stderr, "Usage: %s [--probes N] [--threshold N] [--trefi-us F] [--thresh-mult F]\n", argv[0]);
            return opt=='h'?0:1;
        }
    }

    double tsc_ghz = calibrate_tsc_ghz();
    fprintf(stderr, "TSC: %.3f GHz\n", tsc_ghz);
    double expected_trefi_cyc = trefi_us * 1000.0 * tsc_ghz;
    fprintf(stderr, "Expected tREFI: %.1f us = %.0f cycles\n", trefi_us, expected_trefi_cyc);

    void *p = mmap(NULL, HUGEPAGE_2M, PROT_READ|PROT_WRITE,
                   MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB|(21<<MAP_HUGE_SHIFT), -1, 0);
    if (p == MAP_FAILED) { perror("mmap 2MB hugepage"); return 1; }
    memset(p, 0x42, HUGEPAGE_2M);
    mlock(p, HUGEPAGE_2M);
    volatile char *addr = (volatile char *)p;

    /* Calibration */
    uint64_t *calib = malloc(CALIB_PROBES * sizeof(uint64_t));
    for (int i = 0; i < 2000; i++) timed_probe(addr);
    for (int i = 0; i < CALIB_PROBES; i++) calib[i] = timed_probe(addr);
    qsort(calib, CALIB_PROBES, sizeof(uint64_t), cmp_u64);
    uint64_t p50 = calib[CALIB_PROBES/2], p90 = calib[(int)(CALIB_PROBES*0.90)];
    uint64_t p99 = calib[(int)(CALIB_PROBES*0.99)], p999 = calib[(int)(CALIB_PROBES*0.999)];
    uint64_t threshold = manual_threshold ? manual_threshold : (uint64_t)(thresh_mult * p50);
    int n_above = 0;
    for (int i = 0; i < CALIB_PROBES; i++) if (calib[i] > threshold) n_above++;
    fprintf(stderr, "calib: median=%lu p90=%lu p99=%lu p99.9=%lu thr=%lu spikes=%d%%\n",
            p50, p90, p99, p999, threshold, 100*n_above/CALIB_PROBES);
    free(calib);

    struct spike { uint64_t tsc, latency; } *spikes = malloc(MAX_SPIKES*sizeof*spikes);
    if (!spikes) { perror("malloc spikes"); return 1; }
    int n_spikes = 0;

    fprintf(stderr, "=== PROBING (%d probes) ===\n", n_probes);
    uint64_t tsc_start = rdtsc_lfence();
    for (int i = 0; i < n_probes; i++) {
        clflush_addr(addr); mfence_inst(); asm volatile("lfence":::"memory");
        uint64_t t0 = rdtsc_lfence();
        *(volatile char *)addr;
        uint64_t lat = rdtscp_lfence() - t0;
        if (lat > threshold && n_spikes < MAX_SPIKES) {
            spikes[n_spikes].tsc = t0;
            spikes[n_spikes].latency = lat;
            n_spikes++;
        }
    }
    uint64_t tsc_end = rdtscp_lfence();
    double elapsed_s = (double)(tsc_end - tsc_start) / (tsc_ghz * 1e9);

    printf("abs_tsc,latency_cyc\n");
    for (int i = 0; i < n_spikes; i++) printf("%lu,%lu\n", spikes[i].tsc, spikes[i].latency);
    fprintf(stderr, "Duration: %.2fs  Spikes: %d (%.4f%%)\n",
            elapsed_s, n_spikes, 100.0*n_spikes/n_probes);

    /* Periodicity (harmonic binning) */
    fprintf(stderr, "=== PERIODICITY ANALYSIS ===\n");
    if (n_spikes < 10) { fprintf(stderr, "VERDICT: INSUFFICIENT DATA\n"); goto done; }
    double T = expected_trefi_cyc;
    int n_int = n_spikes - 1;
    double *iv = malloc(n_int * sizeof(double));
    for (int i = 0; i < n_int; i++) iv[i] = (double)(spikes[i+1].tsc - spikes[i].tsc);
    int c1=0,c2=0,c3=0,co=0;
    for (int i = 0; i < n_int; i++) {
        double v = iv[i];
        if      (v>=T*0.85 && v<=T*1.15) c1++;
        else if (v>=T*1.85 && v<=T*2.15) c2++;
        else if (v>=T*2.85 && v<=T*3.15) c3++;
        else co++;
    }
    double frac = (double)(c1+c2+c3)/n_int;
    fprintf(stderr, "1T=%d(%.0f%%) 2T=%d(%.0f%%) 3T=%d(%.0f%%) other=%d(%.0f%%)\n",
            c1,c1*100.0/n_int, c2,c2*100.0/n_int, c3,c3*100.0/n_int, co,co*100.0/n_int);
    if (frac > 0.30) fprintf(stderr, "VERDICT: PERIODIC — %.0f%% at tREFI harmonics\n", frac*100);
    else if (frac > 0.15) fprintf(stderr, "VERDICT: WEAK SIGNAL — %.0f%%\n", frac*100);
    else fprintf(stderr, "VERDICT: NO PERIODIC SIGNAL — %.0f%% at harmonics (spikes=noise)\n", frac*100);
    free(iv);

done:
    free(spikes);
    munmap(p, HUGEPAGE_2M);
    return 0;
}
