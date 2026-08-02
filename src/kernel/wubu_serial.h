/*
 * wubu_serial.h  --  WuBuOS Serial Console (interrupt-driven RX, gap E2)
 *
 * COM1 RX is moved from pure polling to an INTERRUPT path: the UART's
 * data-ready interrupt (IRQ4 -> IOAPIC pin 4 -> vector 36) reads the byte
 * and pushes it into wubu_sync's ISR-safe FIFO; the console task pops the
 * FIFO.  A poll BACKUP still drains the UART into the FIFO when the IRQ
 * path is quiet (e.g. the wiring is not yet live on a new machine) --
 * each byte is consumed exactly once, so there is no double-delivery.
 */
#ifndef WUBU_SERIAL_H
#define WUBU_SERIAL_H

#include <stdint.h>

/* Initialize: FIFO + UART RX interrupt + IOAPIC pin 4 -> vector 36. */
void wubu_serial_init(void);

/* The IRQ handler (vector 36): read the UART + push to the FIFO. */
void wubu_serial_irq(void);

/* Console pop from the FIFO. Returns 0 + the byte, or -1 if empty. */
int wubu_serial_pop(uint8_t *out);

/* Backup drain: if the UART has a byte, read it into the FIFO
 * (safe: the data register is consumed exactly once, by the IRQ or here). */
void wubu_serial_drain(void);

/* Stats. */
uint32_t wubu_serial_fifo_count(void);
uint32_t wubu_serial_irq_count(void);

#endif /* WUBU_SERIAL_H */
