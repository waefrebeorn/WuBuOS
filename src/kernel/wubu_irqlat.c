/*
 * wubu_irqlat.c -- per-IRQ interrupt-latency tracker (KERNEL P0 #6).
 *
 * Records latency samples per IRQ line and reports the p99 (and max/mean)
 * per IRQ through /n/kernel/irqlat: `cat /n/kernel/irqlat` shows p99 per
 * IRQ.  Self-contained C11, opaque struct, no kernel headers  --  the
 * dispatcher calls wubu_irqlat_record(irq, cycles) at ISR entry/exit, and
 * the control plane reads the aggregate.  Samples live in a fixed ring
 * per IRQ (bounded memory, no allocation on the hot path).
 *
 * Latency unit is caller-defined (TSC ticks, ns, us)  --  reported verbatim.
 */
#include "wubu_irqlat.h"
#include <string.h>
#include <stdio.h>

#define IRQLAT_MAX_IRQ   32      /* IRQ lines tracked (0..31) */
#define IRQLAT_RING      256     /* samples per IRQ (power of two) */

typedef struct {
    uint32_t samples[IRQLAT_RING];  /* ring buffer of latency samples */
    int      head;                  /* next write index */
    int      count;                 /* samples recorded (capped at RING) */
    uint64_t total;                 /* running sum (for mean) */
    uint32_t max;
    uint32_t min;
} irq_lat_t;

static irq_lat_t g_lat[IRQLAT_MAX_IRQ];

void wubu_irqlat_reset(void) {
    memset(g_lat, 0, sizeof(g_lat));
}

/* Record one latency sample for `irq`. Out-of-range IRQs are ignored. */
void wubu_irqlat_record(int irq, uint32_t latency) {
    if (irq < 0 || irq >= IRQLAT_MAX_IRQ) return;
    irq_lat_t *l = &g_lat[irq];
    l->samples[l->head] = latency;
    l->head = (l->head + 1) & (IRQLAT_RING - 1);
    if (l->count < IRQLAT_RING) l->count++;
    l->total += latency;
    if (latency > l->max) l->max = latency;
    if (l->count == 1 || latency < l->min) l->min = latency;
}

int wubu_irqlat_count(int irq) {
    if (irq < 0 || irq >= IRQLAT_MAX_IRQ) return 0;
    return g_lat[irq].count;
}

/* p99 latency for an IRQ: the value at the 99th percentile of the recorded
 * samples.  Sorts a local copy (bounded 256 entries) and indexes. */
uint32_t wubu_irqlat_p99(int irq) {
    if (irq < 0 || irq >= IRQLAT_MAX_IRQ) return 0;
    irq_lat_t *l = &g_lat[irq];
    if (l->count == 0) return 0;
    uint32_t tmp[IRQLAT_RING];
    memcpy(tmp, l->samples, sizeof(uint32_t) * (size_t)l->count);
    /* insertion sort (n <= 256, cheap and deterministic) */
    for (int i = 1; i < l->count; i++) {
        uint32_t v = tmp[i];
        int j = i - 1;
        while (j >= 0 && tmp[j] > v) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = v;
    }
    /* nearest-rank p99 index */
    int idx = (l->count * 99 + 99) / 100;  /* ceil(0.99 * count) */
    if (idx >= l->count) idx = l->count - 1;
    if (idx < 0) idx = 0;
    return tmp[idx];
}

uint32_t wubu_irqlat_max(int irq) {
    if (irq < 0 || irq >= IRQLAT_MAX_IRQ) return 0;
    return g_lat[irq].max;
}

uint32_t wubu_irqlat_mean(int irq) {
    if (irq < 0 || irq >= IRQLAT_MAX_IRQ) return 0;
    irq_lat_t *l = &g_lat[irq];
    if (l->count == 0) return 0;
    return (uint32_t)(l->total / (uint64_t)l->count);
}

/* Format the /n/kernel/irqlat payload: one line per IRQ that has samples,
 * "irq=<n> count=<c> p99=<p> max=<m> mean=<a>".  Returns buf. */
char *wubu_irqlat_format(char *buf, int buflen) {
    if (!buf || buflen <= 0) return buf;
    int off = 0;
    for (int irq = 0; irq < IRQLAT_MAX_IRQ; irq++) {
        int c = g_lat[irq].count;
        if (c == 0) continue;
        int n = snprintf(buf + off, (size_t)(buflen - off),
                         "irq=%d count=%d p99=%u max=%u mean=%u\n",
                         irq, c, wubu_irqlat_p99(irq),
                         g_lat[irq].max, wubu_irqlat_mean(irq));
        if (n < 0 || off + n >= buflen) break;
        off += n;
    }
    if (off == 0 && buflen > 0) buf[0] = '\0';
    return buf;
}
