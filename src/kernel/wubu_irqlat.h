/*
 * wubu_irqlat.h -- per-IRQ interrupt-latency tracker (KERNEL P0 #6).
 * C11, opaque, self-contained. Latency unit is caller-defined (TSC/ns/us).
 */
#ifndef WUBU_IRQLAT_H
#define WUBU_IRQLAT_H

#include <stdint.h>

/* Reset all per-IRQ latency state. */
void wubu_irqlat_reset(void);

/* Record one latency sample for `irq` (out-of-range IRQs ignored). */
void wubu_irqlat_record(int irq, uint32_t latency);

/* Per-IRQ statistics. */
int      wubu_irqlat_count(int irq);   /* samples recorded (capped) */
uint32_t wubu_irqlat_p99(int irq);     /* 99th-percentile latency */
uint32_t wubu_irqlat_max(int irq);
uint32_t wubu_irqlat_mean(int irq);

/* Format the /n/kernel/irqlat payload into buf; returns buf. */
char *wubu_irqlat_format(char *buf, int buflen);

#endif /* WUBU_IRQLAT_H */
